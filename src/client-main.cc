// client-main.cc
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory> // For std::unique_ptr

#include <ncurses.h>
#include <spdlog/spdlog.h>

// Forward declare or include your client class header
#include "client/chat-client.h"

// Global ncurses windows and synchronization primitives
WINDOW* g_chat_win = nullptr;
WINDOW* g_input_win = nullptr;
std::mutex g_ncurses_mutex;
std::atomic<bool> g_client_running{true}; // Initialize to true

void read_loop(int client_socket_fd) {
    char buffer[2048];
    SPDLOG_INFO("Read loop started for FD {}", client_socket_fd);
    if (client_socket_fd < 0) { 
      g_client_running = false; 
      return; 
    }

    while (g_client_running) {
        ssize_t n = read(client_socket_fd, buffer, sizeof(buffer) - 1);
        if (!g_client_running) break;

        if (n > 0) {
            std::string msg(buffer, n);
            std::lock_guard<std::mutex> lock(g_ncurses_mutex);
            if (g_chat_win) {
                wprintw(g_chat_win, "Server: %s\n", msg.c_str()); // Simple display
                wrefresh(g_chat_win);
            }
        } else if (n == 0) { /* server closed */ g_client_running = false; break; }
        else { /* read error */ if (errno == EINTR) continue; g_client_running = false; break; }
    }
    SPDLOG_INFO("Read loop terminated.");
}

int main(int argc, char* argv[]) {
    const char* server_ip = "127.0.0.1";
    int port = 8080;

    if (argc > 1) server_ip = argv[1];
    if (argc > 2) {
        try { port = std::stoi(argv[2]); }
        catch (const std::exception& e) {
            std::cerr << "Invalid port: " << argv[2] << ". Using default " << port << std::endl;
        }
    }

    // --- Init ncurses ---
    initscr();
    if (has_colors()) {
        start_color();
        // Basic color pairs (can be expanded later)
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    }
    cbreak(); // Line buffering off
    echo(); 

    int terminal_rows, terminal_cols;
    getmaxyx(stdscr, terminal_rows, terminal_cols);
    int chat_win_height = terminal_rows - 3;

    g_chat_win = newwin(chat_win_height, terminal_cols, 0, 0);
    g_input_win = newwin(3, terminal_cols, chat_win_height, 0);

    if (!g_chat_win || !g_input_win) {
        if (isendwin() == FALSE) endwin();
        SPDLOG_CRITICAL("Failed to create ncurses windows.");
        std::cerr << "Error: Failed to create ncurses windows." << std::endl;
        // chat_client_ptr destructor will handle socket close
        return EXIT_FAILURE;
    }

    scrollok(g_chat_win, TRUE);
    box(g_input_win, 0, 0);
    keypad(g_input_win, TRUE); // Enable function keys for input window

    wrefresh(g_chat_win);
    wrefresh(g_input_win);

    spdlog::set_level(spdlog::level::info);
    SPDLOG_INFO("Client application starting...");

    
    SPDLOG_INFO("Attempting to connect to server {}:{}", server_ip, port);

    std::unique_ptr<tt::chat::client::Client> chat_client_ptr;
    try {
        chat_client_ptr = std::make_unique<tt::chat::client::Client>(port, server_ip);
        SPDLOG_INFO("Successfully connected to the server.");
    } catch (const std::runtime_error& e) {
        SPDLOG_CRITICAL("Failed to connect to server: {}", e.what());
        std::cerr << "Error: Failed to connect to server: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        SPDLOG_CRITICAL("An unknown error occurred during client connection.");
        std::cerr << "An unknown error occurred during client connection." << std::endl;
        return EXIT_FAILURE;
    }

    int client_socket_fd = chat_client_ptr->get_socket_fd();
    std::thread reader_thread(read_loop, client_socket_fd);
    
    {
        std::lock_guard<std::mutex> lock(g_ncurses_mutex);
        if (g_chat_win) {
            wprintw(g_chat_win, "Ncurses UI initialized. Type /quit to exit.\n");
            wrefresh(g_chat_win);
        }
    }

    char input_buffer[512];
    while (g_client_running) { // Use the atomic flag
        {
            std::lock_guard<std::mutex> lock(g_ncurses_mutex);
            if (!g_input_win) { g_client_running = false; break; }
            werase(g_input_win);
            box(g_input_win, 0, 0);
            mvwprintw(g_input_win, 1, 2, "> ");
            wrefresh(g_input_win);
        }

        int get_result = wgetnstr(g_input_win, input_buffer, sizeof(input_buffer) - 1);

        if (!g_client_running) break; // Check after wgetnstr in case of signal

        if (get_result == ERR) {
            // Basic error handling for now, can be refined
            std::lock_guard<std::mutex> lock(g_ncurses_mutex);
            if (g_chat_win) {
                wprintw(g_chat_win, "--- Error reading input ---\n");
                wrefresh(g_chat_win);
            }
            g_client_running = false; // Exit on error
            break;
        }

        std::string message_str(input_buffer);
        if (message_str == "/quit") {
            g_client_running = false;
            break;
        }

        if (!message_str.empty()) {
            try {
                chat_client_ptr->send_message(message_str);
                // Still echo locally, or wait for server to echo back via read_loop later
                std::lock_guard<std::mutex> lock(g_ncurses_mutex);
                if (g_chat_win) {
                    wprintw(g_chat_win, "Sent to server: %s\n", message_str.c_str());
                    wrefresh(g_chat_win);
                }
            } catch (const std::runtime_error& e) {
                std::lock_guard<std::mutex> lock(g_ncurses_mutex);
                if (g_chat_win) {
                    wprintw(g_chat_win, "--- Error sending message: %s ---\n", e.what());
                    wrefresh(g_chat_win);
                }
                g_client_running = false; // Assume connection lost
                break;
            }
        }
    }

    // After input loop finishes (due to /quit or error)
    SPDLOG_INFO("Main loop ended. Signaling reader thread to stop.");
    g_client_running = false; // Signal reader thread

    // Shutdown socket to unblock read() in reader_thread
    if (client_socket_fd >= 0) {
        if (shutdown(client_socket_fd, SHUT_RDWR) == -1 && errno != ENOTCONN) {
             SPDLOG_WARN("Socket shutdown failed: {}", strerror(errno));
        }
    }

    if (reader_thread.joinable()) {
        SPDLOG_DEBUG("Joining reader thread...");
        reader_thread.join();
        SPDLOG_DEBUG("Reader thread joined.");
    }

    if (g_input_win) { delwin(g_input_win); g_input_win = nullptr; }
    if (g_chat_win) { delwin(g_chat_win); g_chat_win = nullptr; }
    if (isendwin() == FALSE) endwin();

    SPDLOG_INFO("Client application finished.");
    return EXIT_SUCCESS;
}