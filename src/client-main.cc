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

#include "client/chat-client.h"

std::atomic<bool> g_client_running{true};

void read_loop(int client_socket_fd) {
    spdlog::info("Read loop started for FD {}", client_socket_fd);
    if (client_socket_fd < 0) {
        spdlog::error("Read loop received invalid socket FD. Terminating read loop.");
        g_client_running = false;
        return;
    }

    const int LENGTH_PREFIX_SIZE = 20;

    while (g_client_running) {
        char len_buf[LENGTH_PREFIX_SIZE + 1];
        len_buf[LENGTH_PREFIX_SIZE] = '\0';

        ssize_t n_len = recv(client_socket_fd, len_buf, LENGTH_PREFIX_SIZE, 0);

        if (!g_client_running) { 
            break;
        }
        if (n_len <= 0) {
            std::cout << "--- Server closed connection or read error on length ---" << std::endl;
            if (n_len < 0) { 
                std::cerr << "Read length error: " << strerror(errno) << std::endl;
            }
            g_client_running = false;
            break;
        }

        int msg_len = atoi(len_buf);
        if (msg_len <= 0) {
            spdlog::warn("Received invalid message length from server: '{}'. Skipping message.", len_buf);
            continue; 
        }

        std::vector<char> buffer(msg_len);

        ssize_t n_msg = recv(client_socket_fd, buffer.data(), msg_len, 0);

        if (!g_client_running) {
            break;
        }
        if (n_msg <= 0) {
            std::cout << "--- Server closed connection or read error on message body ---" << std::endl;
            if (n_msg < 0) { 
                std::cerr << "Read message body error: " << strerror(errno) << std::endl;
            }
            g_client_running = false; 
            break;
        }

        std::string received_msg(buffer.data(), msg_len);
        std::cout << received_msg << std::endl;
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

        if (input_line == "/help") {
            std::cout <<
                "Available commands:\n"
                "/list                - List available channels\n"
                "/create <name>       - Create a new channel\n"
                "/join <name>         - Join a channel\n"
                "/users               - List users in current channel\n"
                "/dm @user <message>  - Send a private message\n"
                "/sendfile <filename> - Upload file\n"
                "/help                - Show this help message\n"
                "/message <message>   - Send a message to channel\n"
                "/quit                - Exit the chat client\n";
            std::cout << "> " << std::flush;
            continue;
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