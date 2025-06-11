#include "epoll-server.h"
#include "../utils.h"
#include "../net/chat-sockets.h"

#include "channel_manager.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <cctype> // Add this include for std::isspace

namespace tt::chat::server {
void split_message(const std::string& msg,std::string& msg_type,std::string& msg_content){
    size_t first_whitespace = msg.find_first_of(" \t\r\f\v\n");
    if(first_whitespace==std::string::npos){
      msg_type=msg;
      msg_content="";
      return;
    }
    std::string command_args=msg.substr(first_whitespace);
    size_t first_content_character_substring=command_args.find_first_not_of(" \t\r\f\v\n");
    size_t last_content_character_substring=command_args.find_last_not_of(" \t\r\f\v\n");
    msg_type=msg.substr(0,first_whitespace);
    if(first_content_character_substring==std::string::npos){
      msg_content="";
      return;
    }

    msg_content=command_args.substr(first_content_character_substring,
                last_content_character_substring-first_content_character_substring+1);
}
EpollServer::EpollServer(int port) {
  setup_server_socket(port);

  epoll_fd_ = epoll_create1(0);
  check_error(epoll_fd_ < 0, "epoll_create1 failed");

  // Initialize the ChannelManager
  channel_mgr_ = std::make_unique<ChannelManager>();

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_sock_;
  check_error(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_sock_, &ev) < 0,
              "epoll_ctl listen_sock");
}

EpollServer::~EpollServer() {
  close(listen_sock_);
  close(epoll_fd_);
}

void EpollServer::setup_server_socket(int port) {
  listen_sock_ = net::create_socket();
  sockaddr_in address = net::create_address(port);
  address.sin_addr.s_addr = INADDR_ANY;

  int opt = 1;
  setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  check_error(bind(listen_sock_, (sockaddr *)&address, sizeof(address)) < 0, "bind failed");
  check_error(listen(listen_sock_, 10) < 0, "listen failed");
}

void EpollServer::handle_new_connection() {
  sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);
  int client_sock = accept(listen_sock_, (sockaddr *)&client_addr, &addrlen);
  check_error(client_sock < 0, "accept failed");

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = client_sock;
  check_error(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_sock, &ev) < 0, "epoll_ctl client_sock");

  client_usernames_[client_sock] = "user_" + std::to_string(client_sock);  // temporary username
  SPDLOG_INFO("New connection: {}", client_usernames_[client_sock]);

}

void EpollServer::handle_name_command(int client_sock, const std::string& desired_name) {
  std::string trimmed_name = desired_name;

  if (trimmed_name.empty()) {
    send_message(client_sock, "Username cannot be created.\n");
    SPDLOG_WARN("Client {} attempted to set an empty username.", client_sock);
    return;
  }
  if (username_set_.count(trimmed_name)) {
    send_message(client_sock, "Duplicate usernames are not allowed.\n");
    SPDLOG_WARN("Client {} attempted to duplicate a username.", client_sock);
    return;
  }
  // Remove old username from set if exists
  if (usernames_.count(client_sock)) {
    username_set_.erase(usernames_[client_sock]);
  }
  usernames_[client_sock] = trimmed_name;
  username_set_.insert(trimmed_name);
  std::string welcome = "Welcome, " + trimmed_name + "!\n";
  send_message(client_sock, welcome.c_str(), welcome.size(), 0);
  SPDLOG_INFO("Client {} assigned username '{}'", client_sock, trimmed_name);
  }

void EpollServer::handle_client_data(int client_sock) {
  char buffer[1024];
  ssize_t len = read(client_sock, buffer, sizeof(buffer));
  if (len <= 0) {
    return;
  }

  std::string msg(buffer, len);
  parse_client_command(client_sock, msg);
}

void EpollServer::parse_client_command(int client_sock, const std::string& msg){
  std::string msg_type,msg_content;
  split_message(msg,msg_type,msg_content);
  if(msg_type=="/name"){
    handle_name_command(client_sock, msg_content);
  }
  else if(msg_type=="/create"){
    handle_create_command(client_sock, msg_content);
  } 
  else if (msg_type=="/join") {
    handle_join_command(client_sock, msg_content);
  } else if (msg_type=="/list") {
    handle_list_command(client_sock);
  } else if (msg_type == "/users") {
    handle_users_command(client_sock);
  }
   else if(msg_type=="/message"){
    //handle_channel_message(client_sock, msg);
  }
  //add command for message also
}

void EpollServer::handle_create_command(int client_sock, const std::string& new_channel_name) {
  std::cout<<new_channel_name<<std::endl;
  if(new_channel_name.empty()) {
    send_message(client_sock, "Empty channel names are not allowed.\n");
    SPDLOG_WARN("Client {} attempted to create an empty channel name.", client_sock);
    return;
  }
  if (channel_mgr_->has_channel(new_channel_name)) {
    send_message(client_sock, "Duplicate channel names are not allowed.\n");
    SPDLOG_WARN("Client {} attempted to create a duplicate channel name.", client_sock);
    return;
  }
  channel_mgr_->create_channel(new_channel_name);
  if(client_channels_.find(client_sock) != client_channels_.end()) {
    channel_mgr_->join_channel(new_channel_name, client_channels_[client_sock], client_sock);
  }
  else {
    channel_mgr_->join_channel(new_channel_name, "", client_sock);
  }
  client_channels_[client_sock] = new_channel_name;
  send_message(client_sock, "Channel created.\n");
  SPDLOG_INFO("");
}

void EpollServer::handle_join_command(int client_sock, const std::string& new_channel_name) {
  if (!channel_mgr_->has_channel(new_channel_name)) {
    send_message(client_sock, "Channel not found.\n");
    return;
  }
  if(client_channels_.find(client_sock) != client_channels_.end()) {
    channel_mgr_->join_channel(new_channel_name, client_channels_[client_sock], client_sock);
  }
  else {
    channel_mgr_->join_channel(new_channel_name, "", client_sock);
  }
  client_channels_[client_sock] = new_channel_name;
  send_message(client_sock, "Joined channel.\n");
}

void EpollServer::handle_list_command(int client_sock) {
  auto list = channel_mgr_->list_channels();
  std::string out = "Channels:\n";
  for (auto &ch : list) out += "- " + ch + "\n";
  send_message(client_sock, out.c_str());
}

void EpollServer::handle_users_command(int client_sock) {
  std::string ch = client_channels_[client_sock];
  std::string list = "Users in [" + ch + "]:\n";
  for (int fd : channel_mgr_->get_members(ch)) {
      // Only show users with assigned usernames
      if (usernames_.count(fd))
        list += "- " + usernames_[fd] + "\n";
  }
  send_message(client_sock, list.c_str());
}

void EpollServer::handle_channel_message(int client_sock, const std::string& msg) {
  std::string ch = client_channels_[client_sock];
  if (ch.empty()) {
    send_message(client_sock, "You are not in a channel. Use /join first.\n");
    return;
  }

  std::string full_msg = "[" + ch + "] " + usernames_[client_sock] + ": " + msg;
  broadcast_to_channel(ch, full_msg, client_sock);
}

void EpollServer::broadcast_to_channel(const std::string &channel, const std::string &msg, int sender_fd) {
  for (int fd : channel_mgr_->get_members(channel)) {
    if (fd != sender_fd) {
      send_message(fd, msg.c_str());
    }
  }
}

void EpollServer::broadcast_message(const std::string &message, int sender_fd) {
  for (const auto &[fd, name] : client_usernames_) {
    if (fd != sender_fd) {
      send_message(fd, message.c_str());
    }
  }
}

void EpollServer::run() {
  SPDLOG_INFO("Server started with epoll");
  epoll_event events[kMaxEvents];

  while (true) {
    int nfds = epoll_wait(epoll_fd_, events, kMaxEvents, -1);
    for (int i = 0; i < nfds; ++i) {
      int fd = events[i].data.fd;
      if (fd == listen_sock_) {
        handle_new_connection();
      } else {
        handle_client_data(fd);
      }
    }
  }
}

int EpollServer::send_message(int client_sock, const char* msg, size_t len, int flags) {
  ssize_t sent = send(client_sock, msg, len, flags);
  if (sent < 0) {
    SPDLOG_ERROR("Failed to send to client {}: {}", client_sock, strerror(errno));
    return -1;
  }
  return sent;
}
int EpollServer::send_message(int client_sock, const std::string& message) {
  return send_message(client_sock, message.c_str(), message.size(), 0);
}

}