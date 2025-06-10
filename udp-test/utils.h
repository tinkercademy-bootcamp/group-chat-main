#include <bits/stdc++.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <stdint.h>

#define PORT     8080 
#define MAXLINE  1024 

unsigned long long rdtsc() {
    unsigned long long a, d;
    asm volatile("mfence");
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    a = (d << 32) | a;
    asm volatile("mfence");
    return a;
}

// static inline uint64_t read_cycle_counter() {
//     uint64_t value;
//     asm volatile("mrs %0, cntvct_el0" : "=r"(value));
//     return value;
// }