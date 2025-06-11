#include "udp-sockets.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>

int create_udp_socket() {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

int bind_udp_socket(int sockfd, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    return bind(sockfd, (sockaddr*)&addr, sizeof(addr));
}

ssize_t send_udp_message(int sockfd, const std::string &ip, int port, const std::string &message) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    return sendto(sockfd, message.c_str(), message.size(), 0, (sockaddr*)&addr, sizeof(addr));
}

ssize_t receive_udp_message(int sockfd, std::string &message, std::string &sender_ip, int &sender_port) {
    char buffer[1024];
    sockaddr_in sender_addr;
    socklen_t len = sizeof(sender_addr);
    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (sockaddr*)&sender_addr, &len);
    if (n > 0) {
        buffer[n] = '\0';
        message = buffer;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sender_addr.sin_addr), ip, INET_ADDRSTRLEN);
        sender_ip = ip;
        sender_port = ntohs(sender_addr.sin_port);
    }
    return n;
}

void close_udp_socket(int sockfd) {
    close(sockfd);
}
