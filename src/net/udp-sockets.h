#ifndef UDP_SOCKETS_H
#define UDP_SOCKETS_H

#include <string>
#include <netinet/in.h>

int create_udp_socket();
int bind_udp_socket(int sockfd, int port);
ssize_t send_udp_message(int sockfd, const std::string &ip, int port, const std::string &message);
ssize_t receive_udp_message(int sockfd, std::string &message, std::string &sender_ip, int &sender_port);
void close_udp_socket(int sockfd);

#endif