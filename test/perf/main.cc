#include <iostream>
#include "test-client.h"
#include <vector>
#include <memory>
#include <string>
#include <chrono>
void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <server_ip> <server_port> <num_clients> "
              << "<messages_per_client> <message_size_bytes> [listen_replies (0 or 1)] [think_time_ms (0+)] [channel_name]" << std::endl;
    std::cerr << "Example: " << prog_name << " 127.0.0.1 8080 10 100 64 1 10 testchannel" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        print_usage(argv[0]);
        return 1;
    }

    std::string server_ip = argv[1];
    int server_port = 0;
    int num_clients = 0;
    int messages_per_client = 0; // Can be 0 if only listening
    int message_size_bytes = 0;
    bool listen_replies = false;
    int think_time_ms = 0;
    std::string channel_name = "testchannel"; // Default common channel

    try {
        server_port = std::stoi(argv[2]);
        num_clients = std::stoi(argv[3]);
        messages_per_client = std::stoi(argv[4]);
        message_size_bytes = std::stoi(argv[5]); // Must be > 0 if messages_per_client > 0
        if (argc > 6) listen_replies = (std::stoi(argv[6]) == 1);
        if (argc > 7) think_time_ms = std::stoi(argv[7]);
        if (argc > 8) channel_name = argv[8];
    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    if (num_clients <= 0 || (messages_per_client > 0 && message_size_bytes <= 0)) {
        std::cerr << "Error: Num clients must be > 0. If sending messages, message size must be > 0." << std::endl;
        return 1;
    }
    if (think_time_ms < 0) think_time_ms = 0;

    std::vector<std::unique_ptr<tt::chat::test::TestClient>> clients_wrappers;
    std::vector<std::thread> client_threads;
    for (int i = 0; i < num_clients; ++i) {
        clients_wrappers.emplace_back(std::make_unique<tt::chat::test::TestClient>(
            i, server_ip, server_port, messages_per_client, message_size_bytes,
            listen_replies, think_time_ms, channel_name
        ));
    }
    for (auto& client_wrapper : clients_wrappers) {
        client_threads.emplace_back(&tt::chat::test::TestClient::run_test, client_wrapper.get());
    }
    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    std::cout << "All clients have completed their tests." << std::endl;
    for (const auto& client_wrapper : clients_wrappers) {
        const auto& stats = client_wrapper->get_stats();
        std::cout << "Client " << stats.client_id << " Stats:" << std::endl;
        std::cout << "  Messages Sent: " << stats.messages_sent << std::endl;
        std::cout << "  Messages Received: " << stats.messages_received << std::endl;
        std::cout << "  Bytes Sent: " << stats.bytes_sent << std::endl;
        std::cout << "  Bytes Received: " << stats.bytes_received << std::endl;
        std::cout << "  Connection Time: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(stats.connection_time_taken).count() 
                  << " ms" << std::endl;
        std::cout << "  Send Time: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(stats.send_time_taken).count() 
                  << " ms" << std::endl;
        if (!stats.error_message.empty()) {
            std::cerr << "  Error: " << stats.error_message << std::endl;
        }
    }
    std::cout << "All clients have completed their tests." << std::endl;


    return 0;
}
  