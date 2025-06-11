#include "epoll-server.h"
#include "../utils.h"
#include "../net/chat-sockets.h"

#include "channel_manager.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <cctype> 

namespace tt::chat::server {

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

void EpollServer::assign_username(int client_sock, const std::string& desired_name) {
  std::string trimmed_name = desired_name;
  // Remove leading/trailing whitespace
  trimmed_name.erase(0, trimmed_name.find_first_not_of(" \t\n\r"));
  trimmed_name.erase(trimmed_name.find_last_not_of(" \t\n\r") + 1);

  if (trimmed_name.empty()) {
    send_message(client_sock, "Username cannot be created.\n");
    SPDLOG_WARN("Client {} attempted to set an empty username.", client_sock);
    return;
  }
  if (username_set_.count(trimmed_name)) {
    send_message(client_sock, "Duplicate usernames are not allowed.\n");
    return;
  }
  // Remove old username from set if exists
  if (usernames_.count(client_sock)) {
    username_set_.erase(usernames_[client_sock]);
  }
  usernames_[client_sock] = trimmed_name;
  username_set_.insert(trimmed_name);
  std::string welcome = "Welcome, " + trimmed_name + "!\n";
  send_message(client_sock, welcome);
  SPDLOG_INFO("Client {} assigned username '{}'", client_sock, trimmed_name);
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
  if (msg.rfind("/name", 0) == 0 && msg.size() > 5 && std::isspace(msg[5])) {
      handle_name_command(client_sock, msg);
  } 
  else if (msg.rfind("/name", 0) == 0) {
    send_message(client_sock, "Username cannot be created.\n");
  }
  else if (msg.rfind("/create ", 0) == 0) {
    handle_create_command(client_sock, msg);
  } else if (msg.rfind("/join ", 0) == 0) {
    handle_join_command(client_sock, msg);
  } else if (msg.rfind("/list", 0) == 0) {
    handle_list_command(client_sock);
  } else if (msg.rfind("/sendfile ", 0) == 0) {
    handle_sendfile_command(client_sock, msg);
  } else if (msg == "/users") {
    handle_users_command(client_sock);
  } else if (msg.rfind("/dm ", 0) == 0) {
    handle_private_msg_command(client_sock, msg);
  } else if (msg.rfind("/message ", 0) == 0) {
    handle_channel_message(client_sock, msg);
  } else if (msg.rfind("/bigmsg ", 0) == 0) {
    std::string num = msg.substr(8);
    std::string bmsg = "/message ";
    int len = stoi(num);
    for(int i=0; i<len; i++) 
      bmsg += 'a';
    handle_channel_message(client_sock, bmsg);
  } else {
    send_message(client_sock, "Invalid Command.");
  }
}

void EpollServer::handle_name_command(int client_sock, const std::string& msg) {
  std::string name = msg.substr(6);
  // Remove leading/trailing whitespace
  name.erase(0, name.find_first_not_of(" \t\n\r"));
  name.erase(name.find_last_not_of(" \t\n\r") + 1);
  if (name.empty()) {
    send_message(client_sock, "Username cannot be created.\n");
    return;
  }
  assign_username(client_sock, name);
}

void EpollServer::handle_create_command(int client_sock, const std::string& msg) {
  std::string ch = msg.substr(8);
  // Remove leading/trailing whitespace
  ch.erase(0, ch.find_first_not_of(" \t\n\r"));
  ch.erase(ch.find_last_not_of(" \t\n\r") + 1);

  if(ch.empty() || ch[0] == ' ') {
    send_message(client_sock, "The channel name cannot be empty and cannot begin with a white space.\n");
    return;
  }
  if (channel_mgr_->has_channel(ch)) {
    send_message(client_sock, "Duplicate channel names are not allowed.\n");
    return;
  }
  channel_mgr_->create_channel(ch);
  if(client_channels_.find(client_sock) != client_channels_.end()) {
    channel_mgr_->join_channel(ch, client_channels_[client_sock], client_sock);
  }
  else {
    channel_mgr_->join_channel(ch, "", client_sock);
  }
  client_channels_[client_sock] = ch;
  send_message(client_sock, "Channel created.\n");
}

void EpollServer::handle_join_command(int client_sock, const std::string& msg) {
  std::string ch = msg.substr(6);
  if (!channel_mgr_->has_channel(ch)) {
    send_message(client_sock, "Channel not found.\n");
    return;
  }
  if(client_channels_.find(client_sock) != client_channels_.end()) {
    channel_mgr_->join_channel(ch, client_channels_[client_sock], client_sock);
  }
  else {
    channel_mgr_->join_channel(ch, "", client_sock);
  }
  client_channels_[client_sock] = ch;
  send_message(client_sock, "Joined channel.\n");
}

void EpollServer::handle_list_command(int client_sock) {
  auto list = channel_mgr_->list_channels();
  std::string out = "Channels:\n";
  for (auto &ch : list) out += "- " + ch + "\n";
  send_message(client_sock, out.c_str());
}

void EpollServer::handle_sendfile_command(int client_sock, const std::string& msg) {
  std::string filename = msg.substr(10);
  std::ofstream file("uploads/" + filename, std::ios::binary);
  char filebuf[1024];
  ssize_t n;
  while ((n = read(client_sock, filebuf, sizeof(filebuf))) > 0) {
      file.write(filebuf, n);
      if (n < 1024) break;
  }
  file.close();
  send_message(client_sock, "Upload done\n");
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

void EpollServer::handle_private_msg_command(int client_sock, const std::string& msg) {
  size_t space_pos = msg.find(' ', 5);
  if (space_pos != std::string::npos) {
    std::string recipient = msg.substr(5, space_pos - 5);
    std::string uname;
    if(usernames_.count(client_sock)) {
      uname = usernames_[client_sock];
    }
    else {
      uname = "user_" + std::to_string(client_sock);
    }
    std::string dm = "[DM] " + uname + ": " + msg.substr(space_pos + 1);

    int target_fd = -1;
    for (const auto& [fd, uname] : usernames_) {
      if (uname == recipient) {
        target_fd = fd;
        break;
      }
    }

    if (target_fd != -1)
      send_message(target_fd, dm.c_str());
    else
      send_message(client_sock, "User not found.\n");
  }
}

void EpollServer::handle_channel_message(int client_sock, const std::string& msg) {
  std::string ch = client_channels_[client_sock];
  if (ch.empty()) {
    send_message(client_sock, "You are not in a channel. Use /join first.\n");
    return;
  }
  std::string uname;
  if(usernames_.count(client_sock)) {
    uname = usernames_[client_sock];
  }
  else {
    uname = client_usernames_[client_sock];
  }
  std::string full_msg = "[" + ch + "] " + uname + ": " + msg.substr(9);
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
