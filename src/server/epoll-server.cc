#include "epoll-server.h"
#include "../utils.h"
#include "../net/chat-sockets.h"

#include "channel_manager.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <cctype> 

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

void EpollServer::handle_name_command(int client_sock, const std::string& new_name) {
  if (new_name.empty()) {
    send_message(client_sock, "Username cannot be created.\n");
    SPDLOG_WARN("Client {} attempted to set an empty username.", client_sock);
    return;
  }
  if (username_set_.count(new_name)) {
    send_message(client_sock, "Duplicate usernames are not allowed.\n");
    SPDLOG_WARN("Client {} attempted to duplicate a username.", client_sock);
    return;
  }
  // Remove old username from set if exists
  if (usernames_.count(client_sock)) {
    username_set_.erase(usernames_[client_sock]);
  }

  usernames_[client_sock] = new_name;
  username_set_.insert(new_name);
  std::string welcome = "Welcome, " + new_name + "!\n";
  send_message(client_sock, welcome);
  SPDLOG_INFO("Client {} assigned username '{}'", client_sock, new_name);
  }

void EpollServer::handle_client_data(int client_sock) {
    char len_buf[20 + 1]; // +1 for null terminator
    len_buf[20] = '\0'; // Ensure null termination

    ssize_t total_received_len = 0;
    while (total_received_len < 20) {
        ssize_t received_bytes = recv(client_sock, len_buf + total_received_len, 20 - total_received_len, 0);
        if (received_bytes < 0) {
            if (errno == EINTR) continue; // Interrupted system call, retry
            SPDLOG_ERROR("Error reading message length from client {}: {}. Disconnecting.", client_sock, strerror(errno));
            return;
        }
        if (received_bytes == 0) { // Client disconnected gracefully
            SPDLOG_INFO("Client {} disconnected during length read. Disconnecting.", client_sock);
            return;
        }
        total_received_len += received_bytes;
    }

    int msg_length = atoi(len_buf);
    if (msg_length <= 0) {
        SPDLOG_WARN("Client {} sent invalid message length: '{}' (parsed as {}). Disconnecting.",
                    client_sock, len_buf, msg_length);
        send_message(client_sock, "Server: Invalid message format (length).\n"); // Try to send error
        return;
    }

    std::vector<char> buffer(msg_length);

    ssize_t received_bytes = recv(client_sock, buffer.data(), msg_length, 0);
    if(received_bytes < msg_length) {
      std::cout << received_bytes << " " << msg_length << "\n";
    }
    std::string msg(buffer.data(), msg_length); // Create string from the exact read bytes
    SPDLOG_INFO("Received from client {}: length={} message='{}'", client_sock, msg_length, msg);

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
  } else if (msg.rfind("/bigmsg ", 0) == 0) {
    std::string num = msg.substr(8);
    std::string bmsg = "";
    int len = stoi(num);
    for(int i=0; i<len; i++) 
      bmsg += 'a';
    handle_channel_message(client_sock, bmsg);
  } else if(msg_type=="/message"){
    handle_channel_message(client_sock, msg_content);
  } else{
    send_message(client_sock, "Invalid Command.\n");
    SPDLOG_WARN("Client {} entered an invalid command.", client_sock);
  }
}

void EpollServer::handle_create_command(int client_sock, const std::string& new_channel_name) {
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
  SPDLOG_INFO("Channel created.\n");
}

void EpollServer::handle_join_command(int client_sock, const std::string& new_channel_name) {
  if (!channel_mgr_->has_channel(new_channel_name)) {
    send_message(client_sock, "Channel not found.\n");
    SPDLOG_WARN("Client {} attempted to join a nonexisting channel.", client_sock);
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
  SPDLOG_INFO("Client {} joined channel '{}'.", client_sock, new_channel_name);
}

void EpollServer::handle_list_command(int client_sock) {
  auto list = channel_mgr_->list_channels();
  std::string out = "Channels:\n";
  for (auto &ch : list) out += "- " + ch + "\n";
  send_message(client_sock, out.c_str());
  SPDLOG_INFO("Channels listed.");
}

void EpollServer::handle_users_command(int client_sock) {
  // "/users non_empty" will not throw an error
  std::string ch = client_channels_[client_sock];
  std::string list = "Users in [" + ch + "]:\n";
  for (int fd : channel_mgr_->get_members(ch)) {
      // Only show users with assigned usernames
      if (usernames_.count(fd))
        list += "- " + usernames_[fd] + "\n";
  }
  send_message(client_sock, list.c_str());
  SPDLOG_INFO("Users in channel '{}' listed.",ch);
}

void EpollServer::handle_channel_message(int client_sock, const std::string& msg_content) {
  std::string ch = client_channels_[client_sock];
  if (ch.empty()) {
    send_message(client_sock, "You are not in a channel. Use /join first.\n");
    SPDLOG_WARN("User {} attempted to send a message without being in a channel",client_sock);
    return;
  }
  std::string uname;
  if(usernames_.count(client_sock)) {
    uname = usernames_[client_sock];
  }
  else {
    uname = client_usernames_[client_sock];
  }
  std::string full_msg = "[" + ch + "] " + uname + ": " + msg_content;
  broadcast_to_channel(ch, full_msg, client_sock);
  SPDLOG_INFO("User {} sent message on channel '{}'",client_sock,ch);
}

void EpollServer::broadcast_to_channel(const std::string &channel, const std::string &msg, int sender_fd) {
  for (int fd : channel_mgr_->get_members(channel)) {
    if (fd != sender_fd) {
      send_message(fd, msg.c_str());
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
    size_t total_sent = 0;
    
    ssize_t sent = send(client_sock, msg + total_sent, len - total_sent, flags);
    if (sent < 0) {
        if (errno == EINTR) return 0; 
        SPDLOG_ERROR("Failed to send to client {}: {} (errno: {})", client_sock, strerror(errno), errno);
        return -1;
    }
    if (sent == 0) { 
        SPDLOG_WARN("Client {} disconnected during send (0 bytes sent). Disconnecting.", client_sock);
        return 0;
    }
    total_sent += sent;
    
    return total_sent;
}

int EpollServer::send_message(int client_sock, const std::string& message) {
  std::string msg_sz_str = std::to_string(message.size());
  if (msg_sz_str.length() > 20) {
      SPDLOG_ERROR("Message size ({}) exceeds {}-character length prefix limit for client {}. This message might be truncated at receiver.",
                   message.size(), 20, client_sock);
      msg_sz_str = msg_sz_str.substr(msg_sz_str.length() - 20);
  }
  msg_sz_str = std::string(20 - msg_sz_str.length(), '0') + msg_sz_str;

  int bytes_sent_len = send_message(client_sock, msg_sz_str.c_str(), msg_sz_str.length(), 0);
  if (bytes_sent_len <= 0 || (size_t)bytes_sent_len != msg_sz_str.length()) {
      SPDLOG_ERROR("Failed to send full length prefix to client {}. Sent: {} (expected {}). Disconnecting.",
                   client_sock, bytes_sent_len, msg_sz_str.length());
      return -1;
  }

  int bytes_sent_msg = send_message(client_sock, message.c_str(), message.size(), 0);
  if (bytes_sent_msg <= 0 || (size_t)bytes_sent_msg != message.size()) {
      SPDLOG_ERROR("Failed to send full message body to client {}. Sent: {} (expected {}). Disconnecting.",
                   client_sock, bytes_sent_msg, message.size());
      return -1;
  }
  return bytes_sent_msg;
}

}
