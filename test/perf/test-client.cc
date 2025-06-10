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
#define LOG_TEST_INFO(client_id, msg) std::cout << "[Client " << client_id << " INFO] " << msg << std::endl
#define LOG_TEST_ERROR(client_id, msg) std::cerr << "[Client " << client_id << " ERROR] " << msg << std::endl

namespace tt::chat::test {

TestClient::TestClient(int id, const std::string& server_ip, int server_port, int num_messages_to_send, int message_size_bytes,
    bool listen_for_replies, int client_think_time_ms, const std::string& common_channel_name)


    : client_id_(id),
      server_ip_param_(server_ip),
      server_port_param_(server_port),
      num_messages_to_send_param_(num_messages_to_send),
      message_size_bytes_param_(std::max(1, message_size_bytes)),
      listen_for_replies_param_(listen_for_replies),
      client_think_time_ms_param_(client_think_time_ms),
      common_channel_name_param_(common_channel_name) {

    stats_.client_id = id;
  }


TestClient::~TestClient() {
    keep_running_ = false; // 1. Signal listener thread to stop.

    if (actual_client_ && actual_client_->get_socket_fd() != -1) {
        // 2. Shutdown the socket. This will cause a blocked read() in the listener to return (often with error or 0).
        ::shutdown(actual_client_->get_socket_fd(), SHUT_RDWR);
    }

    if (listener_thread_.joinable()) {
        // 3. Wait for the listener thread to actually finish its execution.
        listener_thread_.join();
    }
}

bool TestClient::initialize_and_connect_() {
    auto start_conn_time = std::chrono::high_resolution_clock::now();
    try {
        actual_client_ = std::make_unique<tt::chat::client::Client>(
            server_port_param_, server_ip_param_
        );
        stats_.connection_successful = true;
        LOG_TEST_INFO(client_id_, "Connection successful.");
    } catch (const std::runtime_error& e) {
        stats_.error_message = "Connection failed: " + std::string(e.what());
        stats_.connection_successful = false;
    } catch (...) { 
        stats_.error_message = "Connection failed: Unknown exception during Client construction.";
        stats_.connection_successful = false;
    }
    stats_.connection_time_taken = std::chrono::high_resolution_clock::now() - start_conn_time;
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
        LOG_TEST_INFO(client_id_, "Initial setup commands sent.");
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
        LOG_TEST_INFO(client_id_, "Initial setup commands sent.");
    } catch (const std::runtime_error& e) {
        stats_.error_message = "Initial setup send failed: " + std::string(e.what());
        keep_running_ = false; // Stop the test if setup fails
    }
}
void TestClient::execute_send_phase_() {
    if (!stats_.connection_successful || !actual_client_ || !keep_running_) return;

    std::string base_message_content(message_size_bytes_param_, 'A');
    
    auto start_send_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_messages_to_send_param_ && keep_running_; ++i) {
        std::string current_message = "C" + std::to_string(client_id_) + "S" + std::to_string(i) + "D:";
        size_t prefix_len = current_message.length();
        if (prefix_len < static_cast<size_t>(message_size_bytes_param_)) {
            current_message += base_message_content.substr(0, message_size_bytes_param_ - prefix_len);
        } else {
            current_message = current_message.substr(0, message_size_bytes_param_);
        }
        // Ensure exact size
        current_message.resize(message_size_bytes_param_, 'P');

        try {
            actual_client_->send_message(current_message);
            stats_.messages_sent++;
            stats_.bytes_sent += current_message.length();
        } catch (const std::runtime_error& e) {
            stats_.error_message = "Send failed: " + std::string(e.what());
            keep_running_ = false; // Stop sending for this client
            break;
        }

        if (client_think_time_ms_param_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(client_think_time_ms_param_));
        }
    }
    stats_.send_time_taken = std::chrono::high_resolution_clock::now() - start_send_time;
    LOG_TEST_INFO(client_id_, "Send phase completed. Messages sent: " << stats_.messages_sent);
}

void TestClient::execute_listen_phase_() {
    if (!stats_.connection_successful || !actual_client_ || !keep_running_) return;

    int socket_fd = actual_client_->get_socket_fd();
    if (socket_fd < 0) {
        stats_.error_message = "Listener: Invalid socket FD from actual_client.";
        LOG_TEST_ERROR(client_id_, stats_.error_message);
        keep_running_ = false;
        return;
    }


    char buffer[4096]; // Larger buffer for receiving
    auto listener_start_time = std::chrono::high_resolution_clock::now();

    LOG_TEST_INFO(client_id_, "Listener starting.");
    while (keep_running_) {
        LOG_TEST_INFO(client_id_, "Listener waiting for messages...");
        ssize_t bytes_read = ::read(socket_fd, buffer, sizeof(buffer) - 1);

        if (!keep_running_ && bytes_read <=0) { // Check if told to stop (and) read is unblocked/errored
             break;
        }

        if (bytes_read > 0) {

            stats_.messages_received++;
            stats_.bytes_received += bytes_read;
        } else if (bytes_read == 0) { // Server closed connection
            LOG_TEST_INFO(client_id_, "Listener: Server closed connection.");
            if (keep_running_) stats_.error_message = "Listener: Server closed connection unexpectedly.";
            keep_running_ = false;
            break;
        } else { // bytes_read < 0
            if (errno == EINTR && keep_running_) {
                continue; // Interrupted by signal, try again if still running
            }

            if (keep_running_) { // Only log error if we weren't expecting to stop
                stats_.error_message = "Listener: Read error: " + std::string(strerror(errno));
                LOG_TEST_ERROR(client_id_, stats_.error_message);
            }
            keep_running_ = false;
            break;
        }
    }
    stats_.listen_duration_actual = std::chrono::high_resolution_clock::now() - listener_start_time;
    LOG_TEST_INFO(client_id_, "Listener exiting. Messages received: " << stats_.messages_received);
}


void TestClient::run_test() {
    auto scenario_start_time = std::chrono::high_resolution_clock::now();
    keep_running_ = true; // Set true at the start of run

    if (!initialize_and_connect_()) {
        keep_running_ = false; 
        stats_.total_run_duration = std::chrono::high_resolution_clock::now() - scenario_start_time;
        return; // Cannot proceed if connection failed
    }
    //for the first client, create the channel such that all clients can join it
    if (client_id_ == 0 && !common_channel_name_param_.empty()) {
        perform_initial_setup_0_();
    } else {
        perform_initial_setup_();
    }
    if (!keep_running_) { // If initial setup failed
         stats_.total_run_duration = std::chrono::high_resolution_clock::now() - scenario_start_time;
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

    stats_.total_run_duration = std::chrono::high_resolution_clock::now() - scenario_start_time;
    
}
    



const TestClientStats& TestClient::get_stats() const {
    return stats_;
}

}