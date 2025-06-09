#ifndef TEST_CLIENT_WRAPPER_H
#define TEST_CLIENT_WRAPPER_H

#include <string>
#include <chrono>
#include <atomic> 

#include "../../src/client/chat-client.h"
#include <memory> 
namespace tt::chat::test {

struct TestClientStats {
    long long messages_sent = 0;
    long long messages_received = 0;
    long long bytes_sent = 0;
    long long bytes_received = 0;
    std::chrono::duration<double> connection_time_taken{};
    std::chrono::duration<double> send_time_taken{};
    bool connection_successful = false;
    std::string error_message = "";
    int client_id = 0;
};

class TestClient {
public:
    TestClient(int id,
                      const std::string& server_ip, int server_port,
                      int num_messages_to_send, int message_size_bytes,
                      bool listen_for_replies, int client_think_time_ms,
                      const std::string& common_channel_name);
    ~TestClient();

    TestClient(const TestClient&) = delete;
    TestClient& operator=(const TestClient&) = delete;

    void run_test();
    const TestClientStats& get_stats() const;

private:
    TestClientStats stats_;
    int client_id_; 
    std::string server_ip_param_; 
    int server_port_param_;
    int num_messages_to_send_param_;
    int message_size_bytes_param_;
    bool listen_for_replies_param_;
    int client_think_time_ms_param_;
    std::string common_channel_name_param_;


    std::unique_ptr<tt::chat::client::Client> actual_client_;

    bool initialize_and_connect_();
};

} 
#endif 