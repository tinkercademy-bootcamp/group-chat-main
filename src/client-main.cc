#include <iostream>     
#include <string>
#include <thread>
#include <atomic>
#include <csignal>      
#include <memory>      
#include <vector>       

#include <spdlog/spdlog.h>
#include <unistd.h>     
#include <sys/socket.h> 
#include <errno.h>      

#include "client/chat-client.h" // Assuming Client.h has send_message & get_socket_fd

std::atomic<bool> g_client_running{true};


void read_loop(int client_socket_fd) {
    char buffer[2048];
    spdlog::info("Read loop started for FD {}", client_socket_fd);

    if (client_socket_fd < 0) {
        spdlog::error("Read loop received invalid socket FD. Terminating read loop.");
        g_client_running = false; // Signal main loop to exit
        return;
    }

    while (g_client_running) {
        ssize_t n = read(client_socket_fd, buffer, sizeof(buffer) - 1);

        if (!g_client_running) { // Check after read unblocks
            break;
        }

        if (n > 0) {
            // Null-terminate the received data before printing
            buffer[n] = '\0';
            std::cout << buffer << std::endl;
        } else if (n == 0) {
            std::cout << "--- Server closed connection ---" << std::endl;
            g_client_running = false; // Signal main loop to exit
            break;
        } else { // n < 0, read error
            if (errno == EINTR && g_client_running) {
                // Interrupted by a signal (e.g., SIGINT), but we're still running.
                continue;
            }
            if (g_client_running) { // Only print error if not intentionally shutting down
                std::cerr << "--- Read error: " << strerror(errno) << " ---" << std::endl;
            }
            g_client_running = false; // Signal main loop to exit
            break;
        }
    }
    spdlog::info("Read loop terminated for FD {}", client_socket_fd);
}


int main(int argc, char* argv[]) {
    // Basic command line argument parsing
    std::string server_ip = "127.0.0.1";
    int port = 8080;

    if (argc > 1) {
        server_ip = argv[1];
    }
    if (argc > 2) {
        try {
            port = std::stoi(argv[2]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[2] << ". Using default " << port << std::endl;
        }
    }

    spdlog::set_level(spdlog::level::info);
    spdlog::info("Command-line Chat Client starting to connect to {}:{}", server_ip, port);

    std::unique_ptr<tt::chat::client::Client> chat_client_ptr;
    try {
        chat_client_ptr = std::make_unique<tt::chat::client::Client>(port, server_ip);
        std::cout << "Connected to server. Type messages or '/quit' to exit." << std::endl;
    } catch (const std::runtime_error& e) {
        spdlog::critical("Failed to create or connect client: {}", e.what());
        std::cerr << "Error connecting to server: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        spdlog::critical("An unknown error occurred during client initialization.");
        std::cerr << "An unknown error occurred during client initialization." << std::endl;
        return EXIT_FAILURE;
    }


    int client_socket_fd = chat_client_ptr->get_socket_fd();
    std::thread reader_thread(read_loop, client_socket_fd);

    std::string input_line;
    std::cout << "> " << std::flush; // Initial prompt

    // Main input loop using std::getline
    while (g_client_running && std::getline(std::cin, input_line)) {
        if (!g_client_running) { // Check flag, in case signal interrupted getline somehow
            break;
        }

        if (input_line == "/quit") {
            g_client_running = false; // Signal threads to stop
            break;
        }

        if (!input_line.empty()) {
            try {
                chat_client_ptr->send_message(input_line);
            } catch (const std::runtime_error& e) {
                std::cerr << "--- Error sending message: " << e.what() << " ---" << std::endl;
                // Assume connection is lost if send fails
                g_client_running = false;
                break;
            }
        }
    }

    // Handle EOF (Ctrl+D) on std::cin or if loop exited due to g_client_running
    if (std::cin.eof() && g_client_running) {
        std::cout << "\nInput stream closed (EOF). Shutting down..." << std::endl;
        g_client_running = false;
    }


    spdlog::info("Client main loop terminated. Initiating shutdown sequence...");
    g_client_running = false; // Ensure flag is definitely false for reader_thread

    // Shutdown socket to unblock read() in reader_thread

    if (client_socket_fd >= 0) {
        spdlog::debug("Shutting down socket FD {} for read/write.", client_socket_fd);
        if (shutdown(client_socket_fd, SHUT_RDWR) == -1 && errno != ENOTCONN) {
            spdlog::warn("Socket shutdown failed for FD {}: {}", client_socket_fd, strerror(errno));
        }
    }

    if (reader_thread.joinable()) {
        spdlog::debug("Joining reader thread...");
        reader_thread.join();
        spdlog::debug("Reader thread joined.");
    }

    // chat_client_ptr (unique_ptr) will be destroyed here automatically,
    // calling Client's destructor which closes the socket.

    std::cout << "Chat client shut down." << std::endl;
    spdlog::info("Chat client shutdown complete.");
    return EXIT_SUCCESS;
}