#ifndef TEST_CLIENT_WRAPPER_H
#define TEST_CLIENT_WRAPPER_H

#include <string>
#include <chrono>
#include <atomic> 

#include "../../src/client/chat-client.h"
#include <memory> 
#include <thread>
namespace tt::chat::test {

struct TestClientStats {
    long long messages_sent = 0;
    long long messages_received = 0;
    long long bytes_sent = 0;
    long long bytes_received = 0;
    std::chrono::duration<double> connection_time_taken{};
    std::chrono::duration<double> send_phase_duration{};
    long long relevant_messages_received_for_latency = 0; // Messages that were sent by other test clients
    std::chrono::duration<double> listen_phase_duration{}; // How long the listener actually ran
    std::vector<std::chrono::duration<double>> latencies_ns; // Store individual latencies in nanoseconds
    std::chrono::duration<double> total_run_duration{};     // Total time for this client's test
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
                      const std::string& common_channel_name, int total_test_clients);
    ~TestClient();

    TestClient(const TestClient&) = delete;
    TestClient& operator=(const TestClient&) = delete;

    void run_test();
    const TestClientStats& get_stats() const;

    static bool parse_test_message(const std::string& msg_str,
        int& out_sender_id, long long& out_msg_seq, std::chrono::time_point<std::chrono::steady_clock>& out_send_timestamp);
    static std::string format_test_message(int client_id, long long msg_seq,
        const std::chrono::time_point<std::chrono::steady_clock>& timestamp,
        size_t desired_total_size, const std::string& payload_char = "X");

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

    int total_test_clients_param_; // To distinguish test messages



    std::unique_ptr<tt::chat::client::Client> actual_client_;

    std::atomic<bool> keep_running_{false};

    std::thread listener_thread_;

    bool initialize_and_connect_();
    void perform_initial_setup_();  // Sets name, creates and joins channel
    void perform_initial_setup_0_();
    void execute_send_phase_();
    void execute_listen_phase_(); 

    // Constants for message formatting
    static constexpr const char* MSG_PREFIX = "LATENCY_TEST_MSG::";
    static constexpr size_t TIMESTAMP_STR_LEN = 25; // Approximate length for nanoseconds timestamp string
                                                    // e.g., 1234567890123456789.123456
};

} 
#endif 