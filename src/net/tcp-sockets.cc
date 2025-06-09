#include "tcp-sockets.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>

int create_tcp_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    return 0;
}

int bind_tcp_socket(int sockfd, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    return bind(sockfd, (sockaddr*)&addr, sizeof(addr));
}

int listen_tcp_socket(int sockfd) {
    return listen(sockfd, 5);
}

int accept_tcp_connection(int sockfd) {
    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    return accept(sockfd, (sockaddr*)&client_addr, &len);
}

int connect_tcp_socket(int sockfd, const std::string &ip, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    return connect(sockfd, (sockaddr*)&addr, sizeof(addr));
}

ssize_t send_tcp_message(int sockfd, const std::string &message) {
    return send(sockfd, message.c_str(), message.size(), 0);
}

ssize_t receive_tcp_message(int sockfd, std::string &message) {
    char buffer[1024];
    ssize_t len = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if (len > 0) {
        buffer[len] = '\0';
        message = buffer;
    }
    return len;
}

void close_tcp_socket(int sockfd) {
    close(sockfd);
}
