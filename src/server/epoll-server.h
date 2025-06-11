#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <string>
#include <liburing.h>

#ifdef IO_URING_ENABLED
    #define BACKLOG 10
    #define QUEUE_DEPTH 256
    #define BUFFER_SIZE 2048
#endif

enum IoOpType {
  IO_ACCEPT,
  IO_RECV,
  IO_SEND
};

struct IoUringContext {
  IoOpType op_type;
  int client_fd;
  char* buffer;
  size_t buffer_size;
  sockaddr_in* client_addr;  // Only used for accept operations
  socklen_t* addr_len;       // Only used for accept operations
  
  // Constructor for convenience
  IoUringContext(IoOpType type, int fd, char* buf, size_t size, 
                 sockaddr_in* addr = nullptr, socklen_t* len = nullptr)
    : op_type(type), client_fd(fd), buffer(buf), buffer_size(size), 
      client_addr(addr), addr_len(len) {}
};


namespace tt::chat::server {

    class ChannelManager;

    class EpollServer {
    public:
        explicit EpollServer(int port);
        ~EpollServer();
        void run();
        int send_message(int client_sock, const std::string& message);
        int send_message(int client_sock, const char* msg, size_t len, int flags);

    private:
        int listen_sock_;
        int epoll_fd_;
        struct io_uring ring_;


        static constexpr int kBufferSize = 1024;
        static constexpr int kMaxEvents = 64;


        std::unordered_map<int, std::string> client_usernames_;
        std::unordered_map<int, std::string> usernames_;
        std::unordered_set<std::string> username_set_;
        std::unique_ptr<ChannelManager> channel_mgr_;
        std::unordered_map<int, std::string> client_channels_;

        void setup_server_socket(int port);
        void handle_new_connection();
        void handle_client_data(int client_sock);
        void parse_client_command(int client_sock, const std::string& msg);

        void disconnect_client(int client_sock);

        void broadcast_message(const std::string &message, int sender_fd);
        void broadcast_to_channel(const std::string &channel, const std::string &msg, int sender_fd);

        // Command handlers
        void handle_name_command(int client_sock, const std::string& msg);
        void handle_create_command(int client_sock, const std::string& msg);
        void handle_join_command(int client_sock, const std::string& msg);
        void handle_list_command(int client_sock);
        void handle_users_command(int client_sock);
        void handle_channel_message(int client_sock, const std::string& msg);


        #ifdef IO_URING_ENABLED
            void setup_io_uring();
            void handle_io_uring_events();
            void submit_accept();
            void submit_recv(int client_fd);
            void submit_send(int client_fd, const std::string& message);
            void handle_accept_completion(int result, IoUringContext* ctx);
            void handle_recv_completion(int result, IoUringContext* ctx);
            void handle_send_completion(int result, IoUringContext* ctx);
            void handle_client_disconnect(int client_fd);
            int send_message_uring(int client_sock, const std::string& message);
            
        #else 
            void setup_epoll();
            void handle_epoll_events(struct epoll_event events[]);
        #endif
    };

}

#endif