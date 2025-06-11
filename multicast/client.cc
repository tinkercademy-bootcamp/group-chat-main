#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MULTICAST_GROUP "224.1.1.1"
#define PORT 12345
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    char buffer[BUFFER_SIZE];
    int bytes_received;
    char *client_name = "Client";
    
    // Get client name from command line argument
    if (argc > 1) {
        client_name = argv[1];
    }
    
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Allow multiple clients to bind to the same port
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // Set up address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    // Bind socket to port
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    // Join multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Joining multicast group failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("%s started and joined multicast group %s:%d\n", 
           client_name, MULTICAST_GROUP, PORT);
    printf("Waiting for messages... Press Ctrl+C to stop\n\n");
    
    // Receive multicast messages
    while (1) {
        bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
        
        if (bytes_received < 0) {
            perror("Receive failed");
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("[%s] Received: %s\n", client_name, buffer);
    }
    
    // Leave multicast group
    if (setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Leaving multicast group failed");
    }
    
    close(sockfd);
    return 0;
}