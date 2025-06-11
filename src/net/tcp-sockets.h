#ifndef TCP_SOCKETS_H
#define TCP_SOCKETS_H

#include <string>
#include <netinet/in.h>


int create_tcp_socket();
int bind_tcp_socket(int sockfd, int port);
int listen_tcp_socket(int sockfd);
int accept_tcp_connection(int sockfd);
int connect_tcp_socket(int sockfd, const std::string &ip, int port);
ssize_t send_tcp_message(int sockfd, const std::string &message);
ssize_t receive_tcp_message(int sockfd, std::string &message);
void close_tcp_socket(int sockfd);

#endif