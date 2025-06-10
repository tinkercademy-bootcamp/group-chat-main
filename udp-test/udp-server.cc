#include <iostream>
#include <string>
#include <cstring>      // For memset
#include <unistd.h>     // For close
#include <thread>       // For std::this_thread::sleep_for
#include <chrono>       // For std::chrono::seconds

// Socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // For sockaddr_in, IPPROTO_UDP, IP_MULTICAST_TTL, IP_MULTICAST_LOOP
#include <arpa/inet.h>  // For inet_addr

// --- Configuration ---
const std::string MULTICAST_GROUP_IP = "239.1.1.1"; // Must be consistent across sender and receiver
const int MULTICAST_PORT = 12345;                 // Must be consistent
const int MULTICAST_TTL = 32;                     // IMPORTANT: > 1 to cross TGW
// If your EC2 instance has multiple ENIs, specify the private IP of the one connected to the TGW
// Otherwise, "0.0.0.0" might work, but explicit is better for multi-homed hosts.
const std::string LOCAL_SEND_INTERFACE_IP = "172.31.30.215"; // e.g., "172.31.10.10"

int main() {
    int sock;
    struct sockaddr_in multicast_addr;
    int optval;

    // 1. Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 2. Set up the multicast destination address
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(MULTICAST_PORT);
    multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP_IP.c_str());
    if (multicast_addr.sin_addr.s_addr == INADDR_NONE) {
        std::cerr << "Invalid multicast group IP address." << std::endl;
        close(sock);
        return 1;
    }

    // 3. Set the Time-To-Live (TTL) for multicast packets
    // This is CRUCIAL for cross-VPC communication via TGW.
    // TGW acts as a router, so TTL must be greater than 1.
    optval = MULTICAST_TTL;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &optval, sizeof(optval)) < 0) {
        perror("Setting IP_MULTICAST_TTL failed");
        close(sock);
        return 1;
    }

    // 4. Set multicast loopback (optional)
    // If set to 1, the sender also receives its own multicast packets if it's also a member.
    optval = 0; // Usually set to 0 for senders to avoid receiving their own traffic
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval)) < 0) {
        perror("Setting IP_MULTICAST_LOOP failed");
        close(sock);
        return 1;
    }

    // 5. Specify the local interface for outgoing multicast packets (IMPORTANT on multi-homed hosts)
    // Use the private IP address of your EC2 instance's primary ENI if it's not "0.0.0.0".
    if (LOCAL_SEND_INTERFACE_IP != "0.0.0.0") {
        struct in_addr localInterface;
        localInterface.s_addr = inet_addr(LOCAL_SEND_INTERFACE_IP.c_str());
        if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &localInterface, sizeof(localInterface)) < 0) {
            perror("Setting IP_MULTICAST_IF failed");
            close(sock);
            return 1;
        }
    }


    std::string message_prefix = "Hello Multicast from Sender (VPC A)! Message #";
    char buffer[256];

    std::cout << "Multicast Sender started. Sending to " << MULTICAST_GROUP_IP << ":" << MULTICAST_PORT << std::endl;

    for (long long i = 1; i <= 10; ++i) { // Send 10 messages
        std::string message = message_prefix + std::to_string(i);
        strncpy(buffer, message.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0'; // Ensure null-termination

        ssize_t bytes_sent = sendto(sock, buffer, message.length(), 0,
                                    (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));

        if (bytes_sent < 0) {
            perror("sendto failed");
            // In a real app, implement retry logic or error handling
        } else {
            std::cout << "Sent " << bytes_sent << " bytes: '" << message << "'" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Send every 1 second
    }

    close(sock);
    std::cout << "Sender finished." << std::endl;

    return 0;
}