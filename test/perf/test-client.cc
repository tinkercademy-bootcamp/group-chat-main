#include "test-client.h"
#include <algorithm> 
#include <iostream>  
#include <thread>

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
    keep_running_ = false; // Stop any ongoing operations
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

void TestClient::perform_initial_setup_() {
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


void TestClient::run_test() {
    auto scenario_start_time = std::chrono::high_resolution_clock::now();
    keep_running_ = true; // Set true at the start of run

    if (!initialize_and_connect_()) {
        keep_running_ = false; 
        stats_.total_run_duration = std::chrono::high_resolution_clock::now() - scenario_start_time;
        return; // Cannot proceed if connection failed
    }

    perform_initial_setup_();
    if (!keep_running_) { // If initial setup failed
         stats_.total_run_duration = std::chrono::high_resolution_clock::now() - scenario_start_time;
         return;
    }
    
}


const TestClientStats& TestClient::get_stats() const {
    return stats_;
}

}