#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include "../../../src/server/epoll-server.h"
#include "../../../src/client/chat-client.h"

namespace tt::chat::test {

// Test fixture for channel create command tests
class CreateChannelTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Start server in a separate thread
    server_thread_ = std::thread([this]() {
      server_ = std::make_unique<server::EpollServer>(test_port_);
      server_->run();
    });

    // Allow time for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Create client
    client_ = std::make_unique<client::Client>(test_port_, "127.0.0.1");

    // Set username for the test
    client_->send_message("/name TestUser");

    // Clear any welcome messages
    receive_messages(500);
  }

  void TearDown() override {
    // Cleanup resources
    client_.reset();

    // Terminate server (in a real implementation, we would need a cleaner way)
    if (server_thread_.joinable()) {
      // This is a hack for testing - in production code we would need proper shutdown
      pthread_cancel(server_thread_.native_handle());
      server_thread_.join();
    }
  }

  // Helper to receive and collect messages from server
  std::vector<std::string> receive_messages(int timeout_ms) {
    std::vector<std::string> messages;
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
      try {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_->get_socket_fd(), &read_fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms timeout for select

        int ready = select(client_->get_socket_fd() + 1, &read_fds, nullptr, nullptr, &tv);

        if (ready > 0) {
          char buffer[1024] = {0};
          ssize_t bytes_read = read(client_->get_socket_fd(), buffer, sizeof(buffer) - 1);

          if (bytes_read > 0) {
            messages.push_back(std::string(buffer, bytes_read));
          }
        }

        // Check if we've timed out
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (elapsed > timeout_ms) {
          break;
        }

        // Short sleep to avoid spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

      } catch (const std::exception& e) {
        // Log error and continue
        std::cerr << "Error receiving message: " << e.what() << std::endl;
      }
    }

    return messages;
  }

  bool wait_for_response(const std::string& expected, int timeout_ms = 1000) {
    auto messages = receive_messages(timeout_ms);
    for (const auto& msg : messages) {
      if (msg.find(expected) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  std::unique_ptr<server::EpollServer> server_;
  std::thread server_thread_;
  std::unique_ptr<client::Client> client_;
  static constexpr int test_port_ = 8081; // Use different port for testing
};

// Test channel creation without spaces
TEST_F(CreateChannelTest, CreateChannelWithoutSpaces) {
  client_->send_message("/create TestChannelCreationWithoutSpaces");
  // EXPECT_TRUE(wait_for_response("Channel created"));
  EXPECT_TRUE(wait_for_response("Channel created"));
  client_->send_message("/list");
  EXPECT_TRUE(wait_for_response("TestChannelCreationWithoutSpaces"));
}

// Test channel creation with spaces
TEST_F(CreateChannelTest, CreateChannelWithSpaces) {
  client_->send_message("/create TestChannelCreation WithSpaces");
  EXPECT_TRUE(wait_for_response("Channel created"));
  client_->send_message("/list");
  EXPECT_TRUE(wait_for_response("TestChannelCreation WithSpaces"));
}

// Test channel with empty name
TEST_F(CreateChannelTest, CreateEmptyChannelWithoutSpace) {
  client_->send_message("/create");
  EXPECT_TRUE(wait_for_response("Channel cannot be created"));
  client_->send_message("/list");
  EXPECT_TRUE(wait_for_response("Channels:"));
}

// Test channel starting with a space
TEST_F(CreateChannelTest, CreateEmptyChannelWithSpace) {
  client_->send_message("/create ");
  EXPECT_TRUE(wait_for_response("Channel cannot be created"));
  client_->send_message("/list");
  EXPECT_TRUE(wait_for_response("Channels:"));
}

// Test channel creation with big name
TEST_F(CreateChannelTest, CreateBigChannelName) {
  std::string channel_name(2050, 'a');
  client_->send_message("/create " + channel_name);
  EXPECT_TRUE(wait_for_response("Channel created " + channel_name));
  client_->send_message("/list");
  EXPECT_TRUE(wait_for_response(channel_name));
}

// Test creating multiple channels
TEST_F(CreateChannelTest, CreateMultipleChannels) {
  client_->send_message("/create Channel 1");
  EXPECT_TRUE(wait_for_response("Channel created"));

  client_->send_message("/create Channel2");
  EXPECT_TRUE(wait_for_response("Channel created"));

  client_->send_message("/list");
  auto messages = receive_messages(1000);

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
  client_->send_message("/create DuplicateChannel1");
  EXPECT_TRUE(wait_for_response("Channel created"));
  client_->send_message("/create DuplicateChannel2");
  EXPECT_TRUE(wait_for_response("Channel not created"));
}

// Test auto-join behavior after creating channel
TEST_F(CreateChannelTest, AutoJoinAfterCreate) {
  client_->send_message("/create JoinTestChannel");
  EXPECT_TRUE(wait_for_response("Channel created"));

  client_->send_message("Hello channel");

  auto messages = receive_messages(500);
  bool error_message = false;

  for (const auto& msg : messages) {
    if (msg.find("not in a channel") != std::string::npos) {
      error_message = true;
      break;
    }
  }

  EXPECT_FALSE(error_message) << "User should be automatically joined to created channel";
}

} // namespace tt::chat::test

// Test runner
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
