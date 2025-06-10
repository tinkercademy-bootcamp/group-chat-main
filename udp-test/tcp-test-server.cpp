// C++ program to show the example of server application in
// socket programming
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

using namespace std;

int main()
{
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(serverSocket, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    while (true) {
        // cout << "W/aiting for a connection..." << endl;
        // accepting connection request
        int clientSocket
            = accept(serverSocket, nullptr, nullptr);
        // cout << "Connection established." << endl;

        // recieving data
        char buffer[1024] = { 0 };
        recv(clientSocket, buffer, sizeof(buffer), 0);
        // cout << "Message from client: " << buffer
                //   << endl;

        send(clientSocket, "Hello from server",
             strlen("Hello from server"), 0);
        // cout << "Hello message sent." << endl;

        // closing the client socket.
        close(clientSocket);
    }

    close(serverSocket);

    return 0;
}