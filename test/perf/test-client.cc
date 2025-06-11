#include "test-client.h"
#include <algorithm> 
#include <iostream>  
#include <thread>

#include <vector>
#include <numeric>
#include <iomanip>
#include <string>
#include <cstring>
#include <unistd.h>
// #define // LOG_TEST_INFO(client_id, msg) std::cout << "[Client " << client_id << " INFO] " << msg << std::endl
// #define // LOG_TEST_ERROR(client_id, msg) std::cerr << "[Client " << client_id << " ERROR] " << msg << std::endl

namespace tt::chat::test {

// Define static constexpr members
constexpr const char* TestClient::MSG_PREFIX;
constexpr size_t TestClient::TIMESTAMP_STR_LEN;

// --- Static Helper Implementations ---
std::string TestClient::format_test_message(int client_id, long long msg_seq,
                                                   const std::chrono::time_point<std::chrono::steady_clock>& timestamp,
                                                   size_t desired_payload_size, const std::string& payload_char) {
    auto ns_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp.time_since_epoch());

    std::ostringstream oss;
    oss << MSG_PREFIX
        << "SID=" << client_id << "::"
        << "SEQ=" << msg_seq << "::"
        << "TS=" << ns_since_epoch.count() << "::" // Send nanoseconds directly
        << "PL=";

    std::string header = oss.str();
    std::string full_message = header;

    if (desired_payload_size > 0) {
        std::string payload(desired_payload_size, payload_char.empty() ? 'X' : payload_char[0]);
        full_message += payload;
    }
    // No specific total size enforcement here, payload size is key.
    // Server might have max message length.
    return full_message;
}

bool TestClient::parse_test_message(const std::string& msg_str,
                                           int& out_sender_id,
                                           long long& out_msg_seq,
                                           std::chrono::time_point<std::chrono::steady_clock>& out_send_timestamp) {
    if (msg_str.rfind(MSG_PREFIX, 0) != 0) { // starts_with C++20
        return false; // Not our test message
    }

    std::istringstream iss(msg_str.substr(std::string(MSG_PREFIX).length()));
    std::string segment;

    try {
        // SID=id
        if (!std::getline(iss, segment, '=') || segment != "SID") return false;
        if (!std::getline(iss, segment, ':')) return false; // up to "::"
        out_sender_id = std::stoi(segment);
        if (!std::getline(iss, segment, ':')) return false; // consume the second ':' of "::"

        // SEQ=seq
        if (!std::getline(iss, segment, '=') || segment != "SEQ") return false;
        if (!std::getline(iss, segment, ':')) return false;
        out_msg_seq = std::stoll(segment);
        if (!std::getline(iss, segment, ':')) return false;

        // TS=timestamp_ns
        if (!std::getline(iss, segment, '=') || segment != "TS") return false;
        if (!std::getline(iss, segment, ':')) return false; // value up to "::"
        long long ts_ns = std::stoll(segment);
        out_send_timestamp = std::chrono::time_point<std::chrono::steady_clock>(std::chrono::nanoseconds(ts_ns));
        // The rest is payload
    } catch (const std::exception& e) {
        // // LOG_TEST_ERROR(-1, "Parse error: " << e.what() << " on msg: " << msg_str);
        return false; // Parsing failed
    }
    return true;
}


TestClient::TestClient(int id, const std::string& server_ip, int server_port, int num_messages_to_send, int message_size_bytes,
    bool listen_for_replies, int client_think_time_ms, const std::string& common_channel_name, int total_test_clients)


    : client_id_(id),
      server_ip_param_(server_ip),
      server_port_param_(server_port),
      num_messages_to_send_param_(num_messages_to_send),
      message_size_bytes_param_(std::max(1, message_size_bytes)),
      listen_for_replies_param_(listen_for_replies),
      client_think_time_ms_param_(client_think_time_ms),
      common_channel_name_param_(common_channel_name),
      total_test_clients_param_(total_test_clients) {

    stats_.client_id = id;
    // LOG_TEST_INFO(client_id_, "Constructed. Payload size: " << message_size_bytes_param_);
  }


TestClient::~TestClient() {
    // LOG_TEST_INFO(client_id_, "TestClient destructor called. keep_running_: " << keep_running_.load());
    keep_running_ = false; // 1. Signal listener thread to stop.

    if (actual_client_ && actual_client_->get_socket_fd() != -1) {
        // 2. Shutdown the socket. This will cause a blocked read() in the listener to return (often with error or 0).
        ::shutdown(actual_client_->get_socket_fd(), SHUT_RDWR);
    }

    if (listener_thread_.joinable()) {
        // 3. Wait for the listener thread to actually finish its execution.
        listener_thread_.join();
    }
    // LOG_TEST_INFO(client_id_, "TestClient destroyed.");
}

bool TestClient::initialize_and_connect_() {
    auto start_conn_time = std::chrono::steady_clock::now();
    try {
        actual_client_ = std::make_unique<tt::chat::client::Client>(
            server_port_param_, server_ip_param_
        );
        stats_.connection_successful = true;
        // LOG_TEST_INFO(client_id_, "Connection successful.");
    } catch (const std::runtime_error& e) {
        stats_.error_message = "Connection failed: " + std::string(e.what());
        stats_.connection_successful = false;
    } catch (...) { 
        stats_.error_message = "Connection failed: Unknown exception during Client construction.";
        stats_.connection_successful = false;
    }
    stats_.connection_time_taken = std::chrono::steady_clock::now() - start_conn_time;
    return stats_.connection_successful;
}

void TestClient::perform_initial_setup_0_() {
    if (!stats_.connection_successful || !actual_client_) return;

    try {
        // 1. Set username
        std::string name_cmd = "/name TestUser" + std::to_string(client_id_);
        actual_client_->send_message(name_cmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Small delay
        // 2. Creates a common channel
        if (!common_channel_name_param_.empty()) {
            std::string create_cmd = "/create " + common_channel_name_param_;
            actual_client_->send_message(create_cmd);
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Small delay
        }
        // 3. Join a common channel
        if (!common_channel_name_param_.empty()) {
            std::string join_cmd = "/join " + common_channel_name_param_;
            actual_client_->send_message(join_cmd);
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Small delay
        }
        // LOG_TEST_INFO(client_id_, "Initial setup commands sent.");
    } catch (const std::runtime_error& e) {
        stats_.error_message = "Initial setup send failed: " + std::string(e.what());
        keep_running_ = false; // Stop the test if setup fails
    }
}

void TestClient::perform_initial_setup_() {
    if (!stats_.connection_successful || !actual_client_) return;

    try {
        // 1. Set username
        std::string name_cmd = "/name TestUser" + std::to_string(client_id_);
        actual_client_->send_message(name_cmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Small delay
        // 2. Join a common channel
        if (!common_channel_name_param_.empty()) {
            std::string join_cmd = "/join " + common_channel_name_param_;
            actual_client_->send_message(join_cmd);
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Small delay
        }
        // LOG_TEST_INFO(client_id_, "Initial setup commands sent.");
    } catch (const std::runtime_error& e) {
        stats_.error_message = "Initial setup send failed: " + std::string(e.what());
        keep_running_ = false; // Stop the test if setup fails
    }
}
void TestClient::execute_send_phase_() {
    if (!stats_.connection_successful || !actual_client_ || !keep_running_) return;

    // LOG_TEST_INFO(client_id_, "Starting send phase for " << num_messages_to_send_param_ << " messages.");
    auto send_phase_start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < num_messages_to_send_param_ && keep_running_; ++i) {
        auto send_timestamp = std::chrono::steady_clock::now();
        std::string message_to_send = format_test_message(client_id_, i, send_timestamp, message_size_bytes_param_);

        try {
            actual_client_->send_message(message_to_send);
            stats_.messages_sent++;
            stats_.bytes_sent += message_to_send.length();
        } catch (const std::runtime_error& e) {
            // ... (error handling) ...
            stats_.error_message = "Send phase: Send failed on message " + std::to_string(i) + ": " + std::string(e.what());
            // LOG_TEST_ERROR(client_id_, stats_.error_message);
            keep_running_ = false;
            break;
        }

        if (client_think_time_ms_param_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(client_think_time_ms_param_));
        }
    }
    stats_.send_phase_duration = std::chrono::steady_clock::now() - send_phase_start_time;
    // LOG_TEST_INFO(client_id_, "Send phase completed. Messages sent: " << stats_.messages_sent
                            //    << ". Duration: " << stats_.send_phase_duration.count() << "s.");
}

void TestClient::execute_listen_phase_() {
    if (!stats_.connection_successful || !actual_client_ || !keep_running_) return;

    int socket_fd = actual_client_->get_socket_fd();
    if (socket_fd < 0) {
        stats_.error_message = "Listener: Invalid socket FD from actual_client.";
        // LOG_TEST_ERROR(client_id_, stats_.error_message);
        keep_running_ = false;
        return;
    }


    char buffer[4096]; // Larger buffer for receiving
    auto listener_start_time = std::chrono::steady_clock::now();
    std::string partial_message_buffer; // To handle messages split across TCP packets

    // LOG_TEST_INFO(client_id_, "Listener starting.");
    while (keep_running_) {
        // LOG_TEST_INFO(client_id_, "Listener waiting for messages...");
        ssize_t bytes_read = ::read(socket_fd, buffer, sizeof(buffer) - 1);

        if (!keep_running_ && bytes_read <=0) { // Check if told to stop (and) read is unblocked/errored
             break;
        }

        if (bytes_read > 0) {

            // stats_.messages_received++;
            stats_.bytes_received += bytes_read;
            buffer[bytes_read] = '\0'; // Null-terminate the received chunk
            partial_message_buffer += buffer; // Append to any previous partial message

            size_t msg_start_pos = 0;
            size_t msg_end_pos = 0;

            // Process all full messages in the buffer
            // Our formatted messages are easy to delimit if server sends them one by one.
            // If server might batch them or TCP packets split them, we need more robust framing.
            // For now, assume each read gives us one or more full test messages, or a part of one.
            // A simple delimiter might be newline if server adds it, or we rely on MSG_PREFIX.
            while((msg_end_pos = partial_message_buffer.find(MSG_PREFIX, msg_start_pos + 1)) != std::string::npos) {
                std::string current_msg_str = partial_message_buffer.substr(msg_start_pos, msg_end_pos - msg_start_pos);
                // Process current_msg_str
                int sender_id;
                long long msg_seq;
                std::chrono::time_point<std::chrono::steady_clock> send_timestamp;

                if (parse_test_message(current_msg_str, sender_id, msg_seq, send_timestamp)) {
                    stats_.messages_received++; // Count parsed valid test messages
                    if (sender_id != client_id_ && sender_id > 0 && sender_id <= total_test_clients_param_) {
                        // This message is from another test client, calculate latency
                        stats_.relevant_messages_received_for_latency++;
                        auto recv_timestamp = std::chrono::steady_clock::now();
                        auto latency = recv_timestamp - send_timestamp;
                        // stats_.latencies_ns.push_back(latency);
                        // LOG_TEST_INFO(client_id_, "Received test message from client " << sender_id
                            // << " with SEQ " << msg_seq << " at " << std::chrono::duration_cast<std::chrono::nanoseconds>(recv_timestamp.time_since_epoch()).count()
                            // << " ns, latency: " << std::chrono::duration_cast<std::chrono::nanoseconds>(latency).count()  << " ns");
                        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(latency).count();
                        stats_.latencies_ns.push_back(std::chrono::nanoseconds(latency_ns));
                        // print latency_ns 
                        for (const auto& latency : stats_.latencies_ns) {
                            // // LOG_TEST_INFO(client_id_, "Latency recorded: " << std::chrono::duration_cast<std::chrono::nanoseconds>(latency).count() << " ns");
                        }
                    }
                } else {
                    // It might be a server status message or a non-test chat message
                    // For this test, we might ignore them or count them separately
                     // // LOG_TEST_INFO(client_id_, "Received non-test message or parse error: " << current_msg_str.substr(0,100));
                }
                msg_start_pos = msg_end_pos;
            }
            // Process the last message in the buffer (or the only one if no more MSG_PREFIX found)
            if (msg_start_pos < partial_message_buffer.length()) {
                 std::string last_msg_chunk = partial_message_buffer.substr(msg_start_pos);
                 int sender_id;
                 long long msg_seq;
                 std::chrono::time_point<std::chrono::steady_clock> send_timestamp;
                 if (parse_test_message(last_msg_chunk, sender_id, msg_seq, send_timestamp)) {
                    stats_.messages_received++;
                    if (sender_id != client_id_ && sender_id > 0 && sender_id <= total_test_clients_param_) {
                        stats_.relevant_messages_received_for_latency++;
                        auto recv_timestamp = std::chrono::steady_clock::now();
                        auto latency = recv_timestamp - send_timestamp;
                        stats_.latencies_ns.push_back(latency);
                        // LOG_TEST_INFO(client_id_, "Received test message from client " << sender_id
                            // << " with SEQ " << msg_seq << " at " << std::chrono::duration_cast<std::chrono::nanoseconds>(recv_timestamp.time_since_epoch()).count()
                            // << " ns, latency: " << std::chrono::duration_cast<std::chrono::nanoseconds>(latency).count() << " ns");
                    }
                    partial_message_buffer.clear(); // Processed the whole buffer
                 } else {
                    // It's a partial message, keep it for the next read
                    partial_message_buffer = last_msg_chunk;
                 }
            } else {
                partial_message_buffer.clear(); // All processed
            }



        } else if (bytes_read == 0) { // Server closed connection
            // LOG_TEST_INFO(client_id_, "Listener: Server closed connection.");
            if (keep_running_) stats_.error_message = "Listener: Server closed connection unexpectedly.";
            keep_running_ = false;
            break;
        } else { // bytes_read < 0
            if (errno == EINTR && keep_running_) {
                continue; // Interrupted by signal, try again if still running
            }

            if (keep_running_) { // Only log error if we weren't expecting to stop
                stats_.error_message = "Listener: Read error: " + std::string(strerror(errno));
                // LOG_TEST_ERROR(client_id_, stats_.error_message);
            }
            keep_running_ = false;
            break;
        }
    }
    // print latencies collected so far
    for (const auto& latency : stats_.latencies_ns) {
        // LOG_TEST_INFO(client_id_, "Latency recorded: " << std::chrono::duration_cast<std::chrono::nanoseconds>(latency).count() << " ns");
    }
    stats_.listen_phase_duration = std::chrono::steady_clock::now() - listener_start_time;
    // LOG_TEST_INFO(client_id_, "Listener exiting. Messages received: " << stats_.messages_received);
}
 
 
void TestClient::run_test() {
    auto scenario_start_time = std::chrono::steady_clock::now();
    keep_running_ = true; // Set true at the start of run

    if (!initialize_and_connect_()) {
        keep_running_ = false; 
        stats_.total_run_duration = std::chrono::steady_clock::now() - scenario_start_time;
        return; // Cannot proceed if connection failed
    }
    //for the first client, create the channel such that all clients can join it
    if (client_id_ == 0 && !common_channel_name_param_.empty()) {
        perform_initial_setup_0_();
    } else {
        perform_initial_setup_();
    }
    if (!keep_running_) { // If initial setup failed
         stats_.total_run_duration = std::chrono::steady_clock::now() - scenario_start_time;
         return;
    }

    if (listen_for_replies_param_) {
        listener_thread_ = std::thread(&TestClient::execute_listen_phase_, this);
    }

    if (num_messages_to_send_param_ > 0) {
        execute_send_phase_();
    }

    // How long should the test run if listening?
    if (!listen_for_replies_param_ && num_messages_to_send_param_ > 0) {
         // If we only sent messages and are not listening, this client's main work is done.
         // But with listen_for_replies_param_ == false, it shouldn't have been started.
         keep_running_ = false; 
    }

    stats_.total_run_duration = std::chrono::steady_clock::now() - scenario_start_time;
    
}
    



const TestClientStats& TestClient::get_stats() const {
    return stats_;
}

}