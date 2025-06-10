#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <omp.h>

#include "../../src/server/epoll-server.h"
#include "../../src/client/chat-client.h"

#define CLIENT_COUNT 200
#define MSG_PER_CLIENT 200

namespace tt::chat::test {

// Test fixture for channel create command tests
class MultipleSendTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Start server in a separate thread
    server_thread_ = std::thread([this]() {
      server_ = std::make_unique<server::EpollServer>(test_port_);
      server_->run();
    });

    // Allow time for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // // Create client
    // client_ = std::make_unique<client::Client>(test_port_, "127.0.0.1");

    // // Set username for the test
    // client_->send_message("/name TestUser");

    // // Clear any welcome messages
    // receive_messages(500);

    for(int i=0; i<CLIENT_COUNT; i++)
    {
      clients.emplace_back(test_port_, "127.0.0.1");
      std::string name_cmd = "/name user";
      name_cmd += std::to_string(i);
      clients.back().send_message(name_cmd);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    for(int i=0; i<CLIENT_COUNT; i++)
    {
      receive_messages(i, 15);
    }

    std::cerr<<"Done init\n";
  }

  void TearDown() override {
    // Cleanup resources
    // client_.reset();

    // Terminate server (in a real implementation, we would need a cleaner way)
    if (server_thread_.joinable()) {
      // This is a hack for testing - in production code we would need proper shutdown
      pthread_cancel(server_thread_.native_handle());
      server_thread_.join();
    }
  }

  // Helper to receive and collect messages from server
  std::vector<std::string> receive_messages(int client_num, int timeout_ms) {
    std::vector<std::string> messages;
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
      try {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(clients[client_num].get_socket_fd(), &read_fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms timeout for select

        int ready = select(clients[client_num].get_socket_fd() + 1, &read_fds, nullptr, nullptr, &tv);

        if (ready > 0) {
          char buffer[1024] = {0};
          ssize_t bytes_read = read(clients[client_num].get_socket_fd(), buffer, sizeof(buffer) - 1);

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

  bool wait_for_response(int client_num, const std::string& expected, int timeout_ms = 1000) {
    auto messages = receive_messages(client_num, timeout_ms);
    for (const auto& msg : messages) {
      if (msg.find(expected) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  std::unique_ptr<server::EpollServer> server_;
  std::thread server_thread_;
  // std::unique_ptr<client::Client> client_;
  std::vector<client::Client> clients;
  static constexpr int test_port_ = 8081; // Use different port for testing
};


TEST_F(MultipleSendTest, CheckSendReliability)
{
  clients.back().send_message("/create CheckSendReliability");
  EXPECT_TRUE(wait_for_response(clients.size()-1, "Channel created"));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  #pragma omp parallel for schedule(dynamic)
  
}

// Test runner
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
