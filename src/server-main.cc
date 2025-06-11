#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// #include "server/chat-server.h"
#include "server/epoll-server.h"
int main() {
    const int kPort = 8080;

    tt::chat::server::EpollServer server(kPort);
    server.run();

    return 0;
}