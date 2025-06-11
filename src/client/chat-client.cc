#include "chat-client.h"
#include "../net/chat-sockets.h"
#include "../utils.h"
#include <iostream>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

tt::chat::client::Client::Client(int port, const std::string &server_address)
    : socket_{tt::chat::net::create_socket()} {
    #ifdef UDP_ENABLED
        if (socket_ < 0) {
            throw std::runtime_error("Failed to create UDP socket");
        }
        std::cout<<"loc 1"<<std::endl;
        server_addr_ = create_server_address(server_address, port);
        std::cout<<"loc 2"<<std::endl;
        // Send initial connection message to establish client with server
        std::string init_msg = "/connect";
        std::cout << "server IP: " << server_address << std::endl;
        std::cout << "port: " << port << std::endl;
        std::cout << "struct IP: " << inet_ntoa(server_addr_.sin_addr) << std::endl;
        std::cout << "family: " << server_addr_.sin_family << std::endl;
        std::cout << "port (raw): " << ntohs(server_addr_.sin_port) << std::endl;
        std::cout << "Sending to address: " 
          << inet_ntoa(server_addr_.sin_addr)
          << ":" << ntohs(server_addr_.sin_port) 
          << std::endl;

        errno = 0;
        ssize_t bytes_sent = sendto(socket_, init_msg.c_str(), init_msg.length(), 0,
                                    (sockaddr*)&server_addr_, sizeof(server_addr_));

        std::cout << "sendto returned: " << bytes_sent << std::endl;

        if (bytes_sent < 0) {
            perror("sendto");
            std::cerr << "errno: " << errno << std::endl;
            throw std::runtime_error("sendto failed.");
        }
        std::cout << "loc 3" << std::endl;
    #else
        sockaddr_in address = create_server_address(server_address, port);
        connect_to_server(socket_, address);
    #endif
}

void tt::chat::client::Client::send_message(const std::string &message) {
    #ifdef UDP_ENABLED
        ssize_t bytes_sent = sendto(socket_, message.c_str(), message.length(), 0,
                                (sockaddr*)&server_addr_, sizeof(server_addr_));
        if (bytes_sent < 0) {
            throw std::runtime_error("UDP send failed: " + std::string(strerror(errno)));
        }
    #else
        ssize_t bytes_sent = send(socket_, message.c_str(), message.length(), 0);
        if (bytes_sent < 0) {
            tt::chat::check_error(true, "Send failed on client socket.");
        }
    #endif
}

std::string tt::chat::client::Client::receive_message() {
    spdlog::info("UDP Read loop started");

    char buffer[2048];
    sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t bytes_received = recvfrom(socket_, buffer, sizeof(buffer) - 1, 0,
                                     (sockaddr*)&from_addr, &from_len);
    
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ""; // Non-blocking mode, no data available
        }
        throw std::runtime_error("UDP receive failed: " + std::string(strerror(errno)));
    }
    
    if (bytes_received == 0) {
        return "";
    }
    
    buffer[bytes_received] = '\0';
    return std::string(buffer);
}

int tt::chat::client::Client::get_socket_fd() const {
    return socket_;
}
tt::chat::client::Client::~Client() { close(socket_); }

sockaddr_in tt::chat::client::Client::create_server_address(
    const std::string &server_ip, int port) {
    using namespace tt::chat;
    sockaddr_in address = net::create_address(port);
    // Convert the server IP address to a binary format
    auto err_code = inet_pton(AF_INET, server_ip.c_str(), &address.sin_addr);
    check_error(err_code <= 0, "Invalid address/ Address not supported\n");
    return address;
}

#ifndef UDP_ENABLED
void tt::chat::client::Client::connect_to_server(
    int sock, sockaddr_in &server_address) {
    using namespace tt::chat;
    auto err_code =
        connect(sock, (sockaddr *)&server_address, sizeof(server_address));
    check_error(err_code < 0, "Connection Failed.\n");
}
#endif