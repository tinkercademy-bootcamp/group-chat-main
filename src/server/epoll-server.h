#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <string>


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

        void assign_username(int client_sock, const std::string &desired_name);
        void disconnect_client(int client_sock);

        void broadcast_message(const std::string &message, int sender_fd);
        void broadcast_to_channel(const std::string &channel, const std::string &msg, int sender_fd);

        // Command handlers
        void handle_name_command(int client_sock, const std::string& msg);
        void handle_create_command(int client_sock, const std::string& msg);
        void handle_join_command(int client_sock, const std::string& msg);
        void handle_list_command(int client_sock);
        void handle_help_command(int client_sock);
        void handle_sendfile_command(int client_sock, const std::string& msg);
        void handle_users_command(int client_sock);
        void handle_private_msg_command(int client_sock, const std::string& msg);
        void handle_channel_message(int client_sock, const std::string& msg);

    };

}

#endif