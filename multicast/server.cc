#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define MULTICAST_GROUP "224.1.1.1"
#define PORT 12345
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in addr;
    char message[BUFFER_SIZE];
    int counter = 0;
    time_t rawtime;
    struct tm *timeinfo;
    
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set up multicast address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);
    addr.sin_port = htons(PORT);
    
    // Set TTL for multicast packets
    int ttl = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("Setting TTL failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Multicast Server started\n");
    printf("Sending to group: %s:%d\n", MULTICAST_GROUP, PORT);
    printf("Press Ctrl+C to stop\n\n");
    
    // Send multicast messages every 2 seconds
    while (1) {
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        
        snprintf(message, BUFFER_SIZE, 
                "Message #%d from server at %02d:%02d:%02d", 
                ++counter, 
                timeinfo->tm_hour, 
                timeinfo->tm_min, 
                timeinfo->tm_sec);
        
        if (sendto(sockfd, message, strlen(message), 0, 
                   (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Send failed");
        } else {
            printf("Sent: %s\n", message);
        }
        
        sleep(2);
    }
    
    close(sockfd);
    return 0;
}