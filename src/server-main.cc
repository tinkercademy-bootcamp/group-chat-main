#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

// #include "server/chat-server.h"
#include "server/epoll-server.h"
int main() {
    const int kPort = 8080;

    // Logging setup
    constexpr const char* LOG_FILE = "server.log";
    auto new_logger = spdlog::basic_logger_mt("server-logs", LOG_FILE, true);
    spdlog::set_default_logger(new_logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);

    tt::chat::server::EpollServer server(kPort);
    server.run();

    return 0;
}