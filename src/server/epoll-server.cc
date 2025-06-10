#include "epoll-server.h"
#include "../utils.h"
#include "../net/chat-sockets.h"

#include "channel_manager.h"

#include <spdlog/spdlog.h>
#include <fstream>

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
  // Efficient duplicate username check using set
  if (usernames_set_.count(desired_name)) {
    std::string err = "Username already exists!\n";
    send(client_sock, err.c_str(), err.size(), 0);
    SPDLOG_WARN("Client {} tried to assign duplicate username '{}'", client_sock, desired_name);
    return;
  }
  usernames_[client_sock] = desired_name;
  usernames_set_.insert(desired_name);
  std::string welcome = "Welcome, " + desired_name + "!\n";
  send(client_sock, welcome.c_str(), welcome.size(), 0);
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
  if(msg[0] == '/') {
    if (msg.rfind("/name ", 0) == 0) {
      std::string name = msg.substr(6);
      if(name == "" || name[0] == ' ' || name[0] == '\t') {
        send(client_sock, "Username cannot be empty and cannot begin with a white space.\n", 63, 0);
        return;
      }
      assign_username(client_sock, name);
    } else if (msg.rfind("/create ", 0) == 0) {
      std::string ch = msg.substr(8);
      if(ch == "" || ch[0] == ' ' || ch[0] == '\t') {
        send(client_sock, "The channel name cannot be empty and cannot begin with a white space.\n", 71, 0);
        return;
      }
      // Check for duplicate channel name
      if (channel_mgr_->has_channel(ch)) {
        send(client_sock, "Channel name already exists!\n", 29, 0);
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
      send(client_sock, "Channel created.\n", 17, 0);
    } else if (msg.rfind("/join ", 0) == 0) {
      std::string ch = msg.substr(6);
      if(ch == "" || ch[0] == ' ' || ch[0] == '\t') {
        send(client_sock, "The channel name cannot be empty and cannot begin with a white space.\n", 71, 0);
        return;
      }
      if (!channel_mgr_->has_channel(ch)) {
        send(client_sock, "Channel not found.\n", 19, 0);
        return;
      }
      if(client_channels_.find(client_sock) != client_channels_.end()) {
        channel_mgr_->join_channel(ch, client_channels_[client_sock], client_sock);
      }
      else {
        channel_mgr_->join_channel(ch, "", client_sock);
      }
      client_channels_[client_sock] = ch;
      send(client_sock, "Joined channel.\n", 16, 0);
    } else if (msg.rfind("/list", 0) == 0) {
      auto list = channel_mgr_->list_channels();
      std::string out = "Channels:\n";
      for (auto &ch : list) out += "- " + ch + "\n";
      send(client_sock, out.c_str(), out.size(), 0);
    }
      else if (msg == "/help") {
      std::string help_text =
          "Available commands:\n"
          "/list                 - List available channels\n"
          "/create <name>       - Create a new channel\n"
          "/join <name>         - Join a channel\n"
          "/users               - List users in current channel\n"
          "/msg @user <message> - Send a private message\n"
          "/sendfile <filename> - Upload file\n"
          "/help                - Show this help message\n";
      send(client_sock, help_text.c_str(), help_text.size(), 0);
    } else if (msg.rfind("/sendfile ", 0) == 0) {
      std::string filename = msg.substr(10);
      std::ofstream file("uploads/" + filename, std::ios::binary);
      char filebuf[1024];
      ssize_t n;
      while ((n = read(client_sock, filebuf, sizeof(filebuf))) > 0) {
          file.write(filebuf, n);
          if (n < 1024) break; // crude end-of-file logic
      }
      file.close();
      send(client_sock, "Upload done\n", 12, 0);
      return;
    } else if (msg == "/users") {
      std::string ch = client_channels_[client_sock];
      std::string list = "Users in [" + ch + "]:\n";
      for (int fd : channel_mgr_->get_members(ch)) {
          list += "- " + usernames_[fd] + "\n";
      }
      send(client_sock, list.c_str(), list.size(), 0);
      return;
    } else if (msg.rfind("/msg ", 0) == 0) {
      size_t space_pos = msg.find(' ', 5);
      if (space_pos != std::string::npos) {
          std::string recipient = msg.substr(5, space_pos - 5);
          std::string dm = "[DM] " + usernames_[client_sock] + ": " + msg.substr(space_pos + 1);

          int target_fd = -1;
          for (const auto& [fd, uname] : usernames_) {
              if (uname == recipient) {
                  target_fd = fd;
                  break;
              }
          }

          if (target_fd != -1)
              send(target_fd, dm.c_str(), dm.size(), 0);
          else
              send(client_sock, "User not found.\n", 17, 0);
      }
      return;
    }
    else {
      send(client_sock, "Invalid command.\n", 18, 0);
    }
  } else {
    std::string user = usernames_[client_sock];
    std::string ch = client_channels_[client_sock];
    if (ch.empty()) {
      send(client_sock, "You are not in a channel. Use /join first.\n", 44, 0);
      return;
    }

    std::string full_msg = "[" + ch + "] " + usernames_[client_sock] + ": " + msg;

    broadcast_to_channel(ch, full_msg, client_sock);
  }

}

void EpollServer::broadcast_to_channel(const std::string &channel, const std::string &msg, int sender_fd) {
  for (int fd : channel_mgr_->get_members(channel)) {
    if (fd != sender_fd) {
      send(fd, msg.c_str(), msg.size(), 0);
    }
  }
}

void EpollServer::broadcast_message(const std::string &message, int sender_fd) {
  for (const auto &[fd, name] : client_usernames_) {
    if (fd != sender_fd) {
      send(fd, message.c_str(), message.size(), 0);
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


} // namespace tt::chat::server