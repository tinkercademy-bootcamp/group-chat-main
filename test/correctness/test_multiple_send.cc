#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <omp.h>
#include <regex>

using ::testing::HasSubstr;

#include "../../src/server/epoll-server.h"
#include "../../src/client/chat-client.h"

// Default values that can be overridden by command-line arguments
int CLIENT_COUNT = 50;
int MSG_PER_CLIENT = 50;

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
      clients.emplace_back(std::make_unique<client::Client>(test_port_, "127.0.0.1"));
      // std::cerr<<i<<" on socket "<<clients[i]->get_socket_fd()<<std::endl;
      std::string name_cmd = "/name user";
      name_cmd += std::to_string(i);
      // std::cerr<<"sending on "<<i<<std::endl;
      clients.back()->send_message(name_cmd);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    for(int i=0; i<CLIENT_COUNT; i++)
    {
      receive_messages_from_client(clients[i].get(), 15);
    }

    // std::cerr<<"Done init"<<std::endl;
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

  bool wait_for_response(int client_num, const std::string& expected, int timeout_ms = 1000) {
    auto messages = receive_messages_from_client(clients[client_num].get(), timeout_ms);
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
  std::vector<std::unique_ptr<client::Client>> clients;
  static constexpr int test_port_ = 8081; // Use different port for testing
};


TEST_F(MultipleSendTest, CheckSendReliability)
{
  // std::cerr<<"send message from "<<clients.size()-1<<std::endl;
  clients.back()->send_message("/create CheckSendReliability");
  EXPECT_TRUE(wait_for_response(clients.size()-1, "Channel created"));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  #pragma omp parallel for schedule(dynamic)
  for(int i=0; i<CLIENT_COUNT; i++)
  {
    // std::cerr<<"socket_fd of "<<i<<" = "<<clients[i]->get_socket_fd()<<std::endl;
    // clients[i]->send_message("/create CheckSendReliability");
    clients[i]->send_message("/join CheckSendReliability");
    // std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  #pragma omp parallel for schedule(dynamic)
  for(int i=0; i<CLIENT_COUNT; i++)
  {
    for(int j=0; j<MSG_PER_CLIENT; j++)
    {
      std::string msg_str = "this ";
      msg_str += std::to_string(i);
      msg_str += " ";
      msg_str += std::to_string(j);
      msg_str += " ";
      msg_str += "here";
      // std::cerr<<"try send from "<<i<<std::endl;
      clients[i]->send_message(msg_str);
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  #pragma omp parallel for schedule(dynamic)
  for(int i=0; i<CLIENT_COUNT; i++)
  {
    auto str_vec = receive_messages_from_client(clients[i].get(), 5000);
    std::string final = "";
    for(auto& s: str_vec)
    {
      final += s;
    }

    // Efficient message check: parse all messages in one pass using manual parsing
    std::vector<std::vector<bool>> received(CLIENT_COUNT, std::vector<bool>(MSG_PER_CLIENT, false));
    size_t pos = 0;
    while (pos < final.size()) {
      // Look for the exact prefix
      size_t start = final.find("this ", pos);
      if (start == std::string::npos) break;
      size_t num1_start = start + 5;
      size_t num1_end = final.find(' ', num1_start);
      if (num1_end == std::string::npos) break;
      size_t num2_start = num1_end + 1;
      size_t num2_end = final.find(' ', num2_start);
      if (num2_end == std::string::npos) break;
      size_t here_start = num2_end + 1;
      if (here_start + 3 >= final.size()) break;
      if (final.compare(here_start, 4, "here") == 0) {
        // Try to parse the numbers
        try {
          int sender = std::stoi(final.substr(num1_start, num1_end - num1_start));
          int msgidx = std::stoi(final.substr(num2_start, num2_end - num2_start));
          if (sender >= 0 && sender < CLIENT_COUNT && msgidx >= 0 && msgidx < MSG_PER_CLIENT) {
            received[sender][msgidx] = true;
          }
        } catch (...) {}
        pos = here_start + 4;
      } else {
        pos = num2_end + 1;
      }
    }

    for(int k=0; k<CLIENT_COUNT; k++)
    {
      if(k==i) continue;
      for(int j=0; j<MSG_PER_CLIENT; j++)
      {
        EXPECT_TRUE(received[k][j]) <<
        "Failed to receive message from client " << k <<" (message #"<<j<<") on client "<<i;
      }
    }
  }
}

}

// Test runner
int main(int argc, char** argv) {
  // Parse command line arguments for client count and messages per client
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--client-count" || arg == "-c") {
      if (i + 1 < argc) {
        CLIENT_COUNT = std::stoi(argv[++i]);
      }
    } else if (arg == "--msg-per-client" || arg == "-m") {
      if (i + 1 < argc) {
        MSG_PER_CLIENT = std::stoi(argv[++i]);
      }
    } else if (arg.find("--msg_per_client=") == 0) {
      MSG_PER_CLIENT = std::stoi(arg.substr(17));
    }
  }

  std::cout << "Running test with " << CLIENT_COUNT << " clients and "
            << MSG_PER_CLIENT << " messages per client" << std::endl;

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
