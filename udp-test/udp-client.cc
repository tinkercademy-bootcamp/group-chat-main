#include <iostream>
#include <string>
#include <cstring>      // For memset
#include <unistd.h>     // For close
#include <thread>       // For std::this_thread::sleep_for
#include <chrono>       // For std::chrono::seconds

// Socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // For sockaddr_in, IPPROTO_UDP, IP_ADD_MEMBERSHIP, IP_DROP_MEMBERSHIP
#include <arpa/inet.h>  // For inet_addr

// --- Configuration ---
const std::string MULTICAST_GROUP_IP = "239.1.1.1"; // Must be consistent with sender
const int MULTICAST_PORT = 12345;                 // Must be consistent with sender
// Listen on "0.0.0.0" to receive on all interfaces.
// If you have multiple ENIs and want to bind to a specific one, use its private IP.
const std::string LISTEN_BIND_IP = "0.0.0.0"; // e.g., "172.31.20.20"

int main() {
    int sock;
    struct sockaddr_in local_addr;
    struct ip_mreq mreq; // Multicast group request structure
    char buffer[1024];

    // 1. Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 2. Allow multiple sockets to use the same PORT number (crucial for multicast receivers)
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR failed");
        close(sock);
        return 1;
    }

    // 3. Bind the socket to the local address and port
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(MULTICAST_PORT);
    local_addr.sin_addr.s_addr = inet_addr(LISTEN_BIND_IP.c_str());

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Binding socket failed");
        close(sock);
        return 1;
    }

    // 4. Join the multicast group
    // The multicast group IP address you want to join
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP_IP.c_str());
    if (mreq.imr_multiaddr.s_addr == INADDR_NONE) {
        std::cerr << "Invalid multicast group IP address." << std::endl;
        close(sock);
        return 1;
    }

    // The interface on which to join the multicast group.
    // Use INADDR_ANY (0.0.0.0) for the default interface, or specify the private IP of your ENI.
    mreq.imr_interface.s_addr = inet_addr(LISTEN_BIND_IP.c_str()); // Or inet_addr("YOUR_EC2_PRIVATE_IP_HERE");

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Joining multicast group failed");
        close(sock);
        return 1;
    }

    std::cout << "Multicast Receiver started. Listening on " << LISTEN_BIND_IP << ":" << MULTICAST_PORT
              << ", joined group " << MULTICAST_GROUP_IP << std::endl;

    while (true) {
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);

        ssize_t bytes_received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                        (struct sockaddr*)&sender_addr, &sender_len);

        if (bytes_received < 0) {
            perror("recvfrom failed");
            // In a real application, handle error gracefully
        } else {
            buffer[bytes_received] = '\0'; // Null-terminate the received data
            std::cout << "Received " << bytes_received << " bytes from "
                      << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port)
                      << ": '" << buffer << "'" << std::endl;
        }
    }

    // Note: In a real application, you'd add signal handling for graceful shutdown.
    // On shutdown, you would also call:
    // setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(sock);
    return 0;
}