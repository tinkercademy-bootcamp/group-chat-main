#include <iostream>
#include "test-client.h"
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <iomanip>
#include <thread>
#include <algorithm>
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

    int total_test_clients_for_wrapper = num_clients; 


    std::vector<std::unique_ptr<tt::chat::test::TestClient>> clients_wrappers;
    std::vector<std::thread> client_threads;

    auto overall_start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < num_clients; ++i) {
        clients_wrappers.emplace_back(std::make_unique<tt::chat::test::TestClient>(
            i, server_ip, server_port, messages_per_client, message_size_bytes,
            listen_replies, think_time_ms, channel_name, total_test_clients_for_wrapper
        ));
    }
    

    // Start all client test scenarios in threads
    for (size_t i = 0; i < clients_wrappers.size(); ++i) {
        client_threads.emplace_back(&tt::chat::test::TestClient::run_test, clients_wrappers[i].get());
        // Small stagger to avoid thundering herd on connect, optional
        if (num_clients > 30 && i < clients_wrappers.size() -1) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        else if(num_clients > 50 && i < clients_wrappers.size() -1) std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    auto overall_end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> total_test_duration = overall_end_time - overall_start_time;

    std::cout << "\n---------------- Test Wrapper Summary ----------------" << std::endl;
    std::cout << "Total actual test duration: " << std::fixed << std::setprecision(3)
              << total_test_duration.count() << " seconds" << std::endl;

    long long total_messages_sent_agg = 0;
    long long total_bytes_sent_agg = 0;
    long long total_messages_received_agg = 0;
    long long total_bytes_received_agg = 0;
    int successful_connections_agg = 0;
    double sum_client_total_run_duration = 0;
    std::vector<std::chrono::duration<double>> all_latencies_collected_ns;
    long long total_relevant_messages_for_latency_agg = 0;

    for (const auto& wrapper_ptr : clients_wrappers) {
        const auto& stats = wrapper_ptr->get_stats();
        total_messages_sent_agg += stats.messages_sent;
        total_bytes_sent_agg += stats.bytes_sent;
        total_messages_received_agg += stats.messages_received;
        total_bytes_received_agg += stats.bytes_received;
        if (stats.connection_successful) {
            successful_connections_agg++;
        }
        sum_client_total_run_duration += stats.total_run_duration.count();
        total_relevant_messages_for_latency_agg += stats.relevant_messages_received_for_latency;
        if (listen_replies) { // Only collect latencies if clients were listening
            // std::cout << "Client " << stats.client_id << " Latencies: " << stats.latencies_ns.size() << std::endl;
            
            all_latencies_collected_ns.insert(all_latencies_collected_ns.end(),
                                             stats.latencies_ns.begin(), stats.latencies_ns.end());
            // print all latencies collected so far
            // std::cout << "Client " << stats.client_id << " Collected Latencies (ns): ";
            // for (const auto& latency: all_latencies_collected_ns) {
            //     std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(latency).count() << " ";
            // }
        }
        if (!stats.error_message.empty()) {
            std::cout << "Client " << stats.client_id << " Error: " << stats.error_message << std::endl;
        }
    }

    std::cout << "Successful Connections: " << successful_connections_agg << "/" << clients_wrappers.size() << std::endl;
    std::cout << "Aggregate Messages Sent: " << total_messages_sent_agg << std::endl;
    std::cout << "Aggregate Bytes Sent: " << total_bytes_sent_agg << " ("
              << static_cast<double>(total_bytes_sent_agg) / (1024 * 1024) << " MB)" << std::endl;

    if (listen_replies) {
        std::cout << "Aggregate Messages Received: " << total_messages_received_agg << std::endl;
        std::cout << "Aggregate Bytes Received: " << total_bytes_received_agg << " ("
                  << static_cast<double>(total_bytes_received_agg) / (1024 * 1024) << " MB)" << std::endl;
    }

    if (total_test_duration.count() > 0.001 && total_messages_sent_agg > 0) {
        double overall_send_mps = static_cast<double>(total_messages_sent_agg) / total_test_duration.count();
        double overall_send_Bps = static_cast<double>(total_bytes_sent_agg) / total_test_duration.count();
        std::cout << "Overall Send Rate (across all clients, wall clock): " << overall_send_mps << " msgs/sec" << std::endl;
        std::cout << "Overall Send Data Rate: " << overall_send_Bps / 1024.0 << " KB/s" << std::endl;
    }
    if (listen_replies && total_test_duration.count() > 0.001 && total_messages_received_agg > 0) {
        double overall_recv_mps = static_cast<double>(total_messages_received_agg) / total_test_duration.count();
        double overall_recv_Bps = static_cast<double>(total_bytes_received_agg) / total_test_duration.count();
        std::cout << "Overall Receive Rate (across all clients, wall clock): " << overall_recv_mps << " msgs/sec" << std::endl;
        std::cout << "Overall Receive Data Rate: " << overall_recv_Bps / 1024.0 << " KB/s" << std::endl;
    }
    // Note: `client_wrappers` unique_ptrs will go out of scope here, calling destructors
    // which will join any remaining listener_threads.

if (listen_replies && !all_latencies_collected_ns.empty()) {
        std::cout << "--- Latency Statistics (End-to-End, nanoseconds) ---" << std::endl;
        std::cout << "Total Relevant Messages for Latency Measurement: " << total_relevant_messages_for_latency_agg << std::endl;

        std::sort(all_latencies_collected_ns.begin(), all_latencies_collected_ns.end());
        //print all contents in all_latencies_collected_ns
        // std::cout << "Collected Latencies (ns): ";
        // for (const auto& lat : all_latencies_collected_ns) {
        //     std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(lat).count() << " ";
        // }

        double sum_latency_ns = 0;
        for (const auto& lat_ns : all_latencies_collected_ns) {
            sum_latency_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(lat_ns).count();
        
        }
        double avg_latency_ns = sum_latency_ns / all_latencies_collected_ns.size();
        double avg_latency_ms = avg_latency_ns / 1e6; // Convert ns to ms

        double min_latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(all_latencies_collected_ns.front()).count();
        double max_latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(all_latencies_collected_ns.back()).count();

        double median_latency_ns = 0;
        if (!all_latencies_collected_ns.empty()) {
            size_t mid = all_latencies_collected_ns.size() / 2;
            if (all_latencies_collected_ns.size() % 2 == 0) {
                median_latency_ns = (std::chrono::duration_cast<std::chrono::nanoseconds>(all_latencies_collected_ns[mid - 1]).count() + 
                                std::chrono::duration_cast<std::chrono::nanoseconds>(all_latencies_collected_ns[mid]).count()) / 2.0;
        } else {
                median_latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(all_latencies_collected_ns[mid]).count();
            }
        }

        double p95_latency_ns = 0;
        if (!all_latencies_collected_ns.empty()) {
            p95_latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            all_latencies_collected_ns[static_cast<size_t>(all_latencies_collected_ns.size() * 0.95)]).count();
    }
        double p99_latency_ns = 0;
         if (!all_latencies_collected_ns.empty()) {
        p99_latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            all_latencies_collected_ns[static_cast<size_t>(std::min((size_t)(all_latencies_collected_ns.size() * 0.99), 
                                                                  all_latencies_collected_ns.size()-1))]).count();}


        std::cout << std::fixed << std::setprecision(5); // For ms output
        std::cout << "Min Latency:    " << min_latency_ns / 1e6 << " ms (" << min_latency_ns << " ns)" << std::endl;
        std::cout << "Avg Latency:    " << avg_latency_ms << " ms (" << avg_latency_ns << " ns)" << std::endl;
        std::cout << "Median Latency: " << median_latency_ns / 1e6 << " ms (" << median_latency_ns << " ns)" << std::endl;
        std::cout << "P95 Latency:    " << p95_latency_ns / 1e6 << " ms (" << p95_latency_ns << " ns)" << std::endl;
        std::cout << "P99 Latency:    " << p99_latency_ns / 1e6 << " ms (" << p99_latency_ns << " ns)" << std::endl;
        std::cout << "Max Latency:    " << max_latency_ns / 1e6 << " ms (" << max_latency_ns << " ns)" << std::endl;

    } else if (listen_replies) {
        std::cout << "--- Latency Statistics ---" << std::endl;
        std::cout << "No relevant messages received for latency calculation or listen_replies was false for all." << std::endl;
    }

    std::cout << "--------------------------------------------------------" << std::endl;

    return 0;
}


  