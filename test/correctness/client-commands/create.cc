#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include "../../../src/server/epoll-server.h"
#include "../../../src/client/chat-client.h"

namespace tt::chat::test {

// Simple sink to capture only the last log entry
class LastLogSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        last_level_ = msg.level;
    }

    void flush_() override {}

    spdlog::level::level_enum get_last_level() {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_level_;
    }

private:
    spdlog::level::level_enum last_level_ = spdlog::level::off;
    mutable std::mutex mutex_;
};

// Test fixture for change name tests
class CreateChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create simple test sink
        test_sink_ = std::make_shared<LastLogSink>();
        
        // Setup spdlog with our test sink
        auto logger = std::make_shared<spdlog::logger>("test_logger", test_sink_);
        spdlog::set_default_logger(logger);
        
        // Start server in a separate thread
        server_thread_ = std::thread([this]() {
            server_ = std::make_unique<server::EpollServer>(test_port_);
            server_->run();
        });

        // Allow time for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // Create two clients
        client1_ = std::make_unique<client::Client>(test_port_, "127.0.0.1");
        client2_ = std::make_unique<client::Client>(test_port_, "127.0.0.1");

        // Clear any welcome messages for both clients
        receive_messages_from_client(client1_.get(), 500);
        receive_messages_from_client(client2_.get(), 500);
    }

    void TearDown() override {
        client1_.reset();
        client2_.reset();
        if (server_thread_.joinable()) {
            pthread_cancel(server_thread_.native_handle());
            server_thread_.join();
        }
    }

    // Check if last log was at expected level
    bool check_last_log_level(spdlog::level::level_enum expected_level) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow log to be written
        return test_sink_->get_last_level() == expected_level;
    }

    // Helper to receive messages from a specific client
    std::vector<std::string> receive_messages_from_client(client::Client* client, int timeout_ms) {
        std::vector<std::string> messages;
        auto start_time = std::chrono::steady_clock::now();

        while (true) {
            try {
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(client->get_socket_fd(), &read_fds);

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 10000;

                int ready = select(client->get_socket_fd() + 1, &read_fds, nullptr, nullptr, &tv);

                if (ready > 0) {
                    char buffer[1024] = {0};
                    ssize_t bytes_read = read(client->get_socket_fd(), buffer, sizeof(buffer) - 1);
                    if (bytes_read > 0) {
                        messages.push_back(std::string(buffer, bytes_read));
                    }
                }

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                if (elapsed > timeout_ms) {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (const std::exception& e) {
                std::cerr << "Error receiving message: " << e.what() << std::endl;
            }
        }
        return messages;
    }


    bool wait_for_response_from_client(client::Client* client, const std::string& expected, int timeout_ms = 1000) {
        auto messages = receive_messages_from_client(client, timeout_ms);
        for (const auto& msg : messages) {
            if (msg.find(expected) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<LastLogSink> test_sink_;
    std::unique_ptr<server::EpollServer> server_;
    std::thread server_thread_;
    std::unique_ptr<client::Client> client1_;
    std::unique_ptr<client::Client> client2_;
    static constexpr int test_port_ = 8081;
};

// Test channel creation without spaces
TEST_F(CreateChannelTest, CreateChannelWithoutSpaces) {
    client1_->send_message("/create TestChannelCreationWithoutSpaces");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));
    client1_->send_message("/list");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful list";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "TestChannelCreationWithoutSpaces"));
}

// Test channel creation with spaces
TEST_F(CreateChannelTest, CreateChannelWithSpaces) {
    client1_->send_message("/create TestChannelCreation WithSpaces");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));
    client1_->send_message("/list");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful list";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "TestChannelCreation WithSpaces"));
}

// Test channel with empty name
TEST_F(CreateChannelTest, CreateEmptyChannelWithoutSpace) {
    client1_->send_message("/create");
    EXPECT_TRUE(check_last_log_level(spdlog::level::warn)) << "Expected WARN level for unsuccessful channel creation";
    client1_->send_message("/list");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful list";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channels:"));
}

// Test channel starting with a space
TEST_F(CreateChannelTest, CreateEmptyChannelWithSpace) {
    client1_->send_message("/create ");
    EXPECT_TRUE(check_last_log_level(spdlog::level::warn)) << "Expected WARN level for unsuccessful channel creation";
    client1_->send_message("/list");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful list";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channels:"));
}

// Test channel creation with big name
TEST_F(CreateChannelTest, CreateBigChannelName) {
    std::string channel_name(2050, 'a');
    client1_->send_message("/create " + channel_name);
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));
    client1_->send_message("/list");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful list";
}

// Test creating multiple channels
TEST_F(CreateChannelTest, CreateMultipleChannels) {
    client1_->send_message("/create Channel 1");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));

    client1_->send_message("/create Channel2");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));

    client1_->send_message("/list");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful list";
    auto messages = receive_messages_from_client(client1_.get(), 1000);

    bool found_channel1 = false;
    bool found_channel2 = false;

    for (const auto& msg : messages) {
        if (msg.find("Channel 1") != std::string::npos) {
            found_channel1 = true;
        }
        if (msg.find("Channel2") != std::string::npos) {
            found_channel2 = true;
        }
    }

    EXPECT_TRUE(found_channel1) << "Channel 1 should be in the channel list";
    EXPECT_TRUE(found_channel2) << "Channel2 should be in the channel list";
}

// Test creating multiple channels with the same name
TEST_F(CreateChannelTest, CreateDuplicateChannels) {
    client1_->send_message("/create DuplicateChannel1");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));
    client1_->send_message("/create DuplicateChannel1");
    EXPECT_TRUE(check_last_log_level(spdlog::level::warn)) << "Expected WARN level for unsuccessful channel creation";;
}

// Test auto-join behavior after creating channel
TEST_F(CreateChannelTest, AutoJoinAfterCreate) {
    client1_->send_message("/create JoinTestChannel");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));

    client1_->send_message("Hello channel");

    auto messages = receive_messages_from_client(client1_.get(), 500);
    bool error_message = false;

    for (const auto& msg : messages) {
        if (msg.find("not in a channel") != std::string::npos) {
            error_message = true;
            break;
        }
   }

    EXPECT_FALSE(error_message) << "User should be automatically joined to created channel";
}

TEST_F(CreateChannelTest, CreateDifferentChannelsFromDifferentClients) {
    client1_->send_message("/create DuplicateChannel1");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
    EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));
    client2_->send_message("/create DuplicateChannel2");
    EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";;
    EXPECT_TRUE(wait_for_response_from_client(client2_.get(), "Channel created"));
}

TEST_F(CreateChannelTest, CreateSameChannelsFromDifferentClients) {
  client1_->send_message("/create DuplicateChannel1");
  EXPECT_TRUE(check_last_log_level(spdlog::level::info)) << "Expected INFO level for successful channel creation";
  EXPECT_TRUE(wait_for_response_from_client(client1_.get(), "Channel created"));
  client2_->send_message("/create DuplicateChannel1");
  EXPECT_TRUE(check_last_log_level(spdlog::level::warn)) << "Expected WARN level for unsuccessful channel creation";;
}

} // namespace tt::chat::test

// Test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
