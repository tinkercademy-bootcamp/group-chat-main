#include "epoll-server.h"
#include "../utils.h"
#include "../net/chat-sockets.h"

#include "channel_manager.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

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

  client_usernames_[client_sock] = fmt::format("user_{}", client_sock); // temporary username
  SPDLOG_INFO("New connection: {}", client_usernames_[client_sock]);

}

void EpollServer::assign_username(int client_sock, const std::string& desired_name) {


  usernames_[client_sock] = desired_name;
  // std::string welcome = "Welcome, " + desired_name + "!\n";

  std::string welcome = fmt::format("Welcome, {}!\n", desired_name);

  // std::ostringstream oss1;
  // oss1 << "Welcome, " << desired_name << "!\n";
  // std::string welcome = oss1.str();
  // send_message(client_sock, welcome.c_str(), welcome.size(), 0);

  SPDLOG_INFO("Client {} assigned username '{}'", client_sock, desired_name);
  }

void EpollServer::handle_client_data(int client_sock) {
  char buffer[1024];
  ssize_t len = read(client_sock, buffer, sizeof(buffer));
  if (len <= 0) {
    // cleanup
    return;
  }

  std::string msg(buffer, len);
  parse_client_command(client_sock, msg);
}

void EpollServer::  parse_client_command(int client_sock, const std::string& msg){
    if (msg.rfind("/name ", 0) == 0) {
    handle_name_command(client_sock, msg);
  } else if (msg.rfind("/create ", 0) == 0) {
    handle_create_command(client_sock, msg);
  } else if (msg.rfind("/join ", 0) == 0) {
    handle_join_command(client_sock, msg);
  } else if (msg.rfind("/list", 0) == 0) {
    handle_list_command(client_sock);
  } else if (msg == "/help") {
    handle_help_command(client_sock);
  } else if (msg.rfind("/sendfile ", 0) == 0) {
    handle_sendfile_command(client_sock, msg);
  } else if (msg == "/users") {
    handle_users_command(client_sock);
  } else if (msg.rfind("/msg ", 0) == 0) {
    handle_private_msg_command(client_sock, msg);
  } else {
    handle_channel_message(client_sock, msg);
  }
}

void EpollServer::handle_name_command(int client_sock, const std::string& msg) {
  assign_username(client_sock, msg.substr(6));
}

void EpollServer::handle_create_command(int client_sock, const std::string& msg) {
  std::string ch = msg.substr(8);
  if(ch == "" || ch[0] == ' ') {
    send_message(client_sock, "The channel name cannot be empty and cannot begin with a white space.\n");
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

void EpollServer::handle_help_command(int client_sock) {
  std::string help_text =
    "Available commands:\n"
    "/list                 - List available channels\n"
    "/create <name>       - Create a new channel\n"
    "/join <name>         - Join a channel\n"
    "/users               - List users in current channel\n"
    "/msg @user <message> - Send a private message\n"
    "/sendfile <filename> - Upload file\n"
    "/help                - Show this help message\n";
  send_message(client_sock, help_text.c_str());
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
  // std::string list = "Users in [" + ch + "]:\n";
  std::string list = fmt::format("Users in [{}]:\n",ch);
  for (int fd : channel_mgr_->get_members(ch)) {
      list += "- " + usernames_[fd] + "\n";
  }
  send_message(client_sock, list.c_str());
}

void EpollServer::handle_private_msg_command(int client_sock, const std::string& msg) {
  size_t space_pos = msg.find(' ', 5);
  if (space_pos != std::string::npos) {
    std::string recipient = msg.substr(5, space_pos - 5);
    // std::string dm = "[DM] " + usernames_[client_sock] + ": " + msg.substr(space_pos + 1);

    std::string dm = fmt::format("[DM] {}: {}", usernames_[client_sock], msg.substr(space_pos + 1));

    // std::ostringstream oss2;
    // oss2 << "[DM] " << usernames_[client_sock] << ": " << msg.substr(space_pos + 1);
    // std::string dm = oss2.str();

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

  // std::string full_msg = "[" + ch + "] " + usernames_[client_sock] + ": " + msg.substr(9);


 std::string full_msg = fmt::format("[{}] {}: {}", ch, usernames_[client_sock], msg.substr(9));

  // std::ostringstream oss3;
  // oss3 << "[" << ch << "] " << usernames_[client_sock] << ": " << msg.substr(9);
  // std::string full_msg = oss3.str();


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