#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

// #include "server/chat-server.h"
#include "server/epoll-server.h"
int main() {
    const int kPort = 8060;
    std::cout<<"server started"<<std::endl;
    // Logging setup 
    constexpr const char* LOG_FILE = "server.log";
    auto new_logger = spdlog::basic_logger_mt("server-logs", LOG_FILE, true);
    spdlog::set_default_logger(new_logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);

    std::cout<<"server moving"<<std::endl;
    tt::chat::server::EpollServer server(kPort);

    std::cout<<"Hello"<<std::endl;
    #ifdef UDP_ENABLED
        std::cout<<"udp enabled"<<std::endl;
    #else
        std::cout<<"udp is not enabled"<<std::endl;
    #endif
    std::cout<<"Hello"<<std::endl;
    server.run();

    return 0;
}