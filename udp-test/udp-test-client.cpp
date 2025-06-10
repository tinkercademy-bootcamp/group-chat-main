#include "utils.h"

int main() {
    int sockfd; 
    char buffer[MAXLINE]; 
    const char *hello = "Hello from client"; 
    struct sockaddr_in     servaddr; 
  
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
      
    int n;  
    socklen_t len; 

    long long start = rdtsc();
    // std::cout << "Start time: " << start << std::endl;
    for (int i=0; i<100; i++) {
        sendto(sockfd, (const char *)hello, strlen(hello), 
            MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
                sizeof(servaddr)); 
        // std::cout<<"Hello message sent."<<std::endl; 
            
        n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
                    MSG_WAITALL, (struct sockaddr *) &servaddr, 
                    &len); 
        buffer[n] = '\0'; 
    }
    // std::cout<<"Server :"<<buffer<<std::endl; 
    long long end = rdtsc();
    // std::cout << "End time: " << end << std::endl;
    std::cout << (end - start) << std::endl;
  
    close(sockfd); 
    return 0;
}