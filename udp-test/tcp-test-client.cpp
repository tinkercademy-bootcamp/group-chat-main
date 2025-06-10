// C++ program to illustrate the client application in the
// socket programming
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

int main()
{
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // sending connection request
    connect(clientSocket, (struct sockaddr*)&serverAddress,
    sizeof(serverAddress));
    
    long long start = rdtsc();
    std::cout << "Start time: " << start << std::endl;

    // sending data
    // for (int i=0; i<100; i++) {
    const char* message = "Hello, server!";
    send(clientSocket, message, strlen(message), 0);

    // std::cout << "Message sent to server: " << message << std::endl;
    // receiving data
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    // std::cout << "Message from server: " << buffer << std::endl;

    long long end = rdtsc();
    // std::cout << "End time: " << end << std::endl;
    std::cout << (end - start) << std::endl;

    // closing socket
    close(clientSocket);

    return 0;
}