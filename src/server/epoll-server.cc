#include "epoll-server.h"
#include "../utils.h"
#include "../net/chat-sockets.h"

#include "channel_manager.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <cctype> // Add this include for std::isspace

namespace tt::chat::server {

EpollServer::EpollServer(int port) {
  base_port = port;
  setup_server_socket(port);
  setup_udp_socket(); 

  epoll_fd_ = epoll_create1(0);
  check_error(epoll_fd_ < 0, "epoll_create1 failed");

  // Initialize the ChannelManager
  channel_mgr_ = std::make_unique<ChannelManager>();

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_sock_;
  check_error(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_sock_, &ev) < 0,
              "epoll_ctl listen_sock");

  ev.data.fd = udp_sock_;
  check_error(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, udp_sock_, &ev) < 0,
              "epoll_ctl multicast_sock");
}

EpollServer::~EpollServer() {
  close(listen_sock_);
  close(epoll_fd_);
}

void EpollServer::setup_udp_socket() {
    // 1. Create a single socket for all UDP communication
    udp_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    check_error(udp_sock_ < 0, "UDP socket creation failed");

    // 2. Allow port reuse, essential for multicast
    int reuse = 1;
    setsockopt(udp_sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 3. Bind to a port to receive both unicast and multicast messages
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(base_port); // Use the same base port as TCP for simplicity
    check_error(bind(udp_sock_, (sockaddr*)&addr, sizeof(addr)) < 0, "UDP bind failed");

    // 4. Join the multicast group to RECEIVE multicast packets
    multicast_group_ = "239.1.1.1";
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_group_.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    check_error(setsockopt(udp_sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0, "Joining multicast group failed");

    // 5. Set TTL for SENDING outgoing multicast packets
    int ttl = 1; // Confine to the local network
    setsockopt(udp_sock_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    // 6. Pre-build the destination address for sending multicast messages
    // (You should add `sockaddr_in multicast_addr_;` to your header)
    multicast_addr_.sin_family = AF_INET;
    multicast_addr_.sin_addr.s_addr = inet_addr(multicast_group_.c_str());
    multicast_addr_.sin_port = htons(base_port);

    SPDLOG_INFO("UDP socket setup on port {}. Multicast group {}:{}", base_port, multicast_group_, base_port);
}

void EpollServer::send_multicast(const std::string& message) {
    ssize_t sent = sendto(udp_sock_, message.c_str(), message.size(), 0,
                         (sockaddr*)&multicast_addr_, sizeof(multicast_addr_));
    if (sent < 0) {
        SPDLOG_ERROR("Failed to send multicast message: {}", strerror(errno));
    }
}

void EpollServer::setup_server_socket(int port) {
  listen_sock_ = net::create_socket();
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  int opt = 1;
  setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  check_error(bind(listen_sock_, (sockaddr *)&address, sizeof(address)) < 0, "TCP bind failed");
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


void EpollServer::handle_udp_data() {
    char buffer[1024];
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ssize_t len = recvfrom(udp_sock_, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &addr_len);

    if (len <= 0) {
        SPDLOG_ERROR("UDP recvfrom failed: {}", strerror(errno));
        return;
    }

    std::string msg(buffer, len);
    std::string client_id = std::string(inet_ntoa(client_addr.sin_addr)) + ":" + std::to_string(ntohs(client_addr.sin_port));

    // If client is new, they must send /connect first.
    if (udp_client_ids_.find(client_id) == udp_client_ids_.end()) {
        if (msg.rfind("/connect", 0) == 0) {
            handle_udp_connect(client_id, client_addr);
        } else {
            SPDLOG_WARN("Ignoring message from unknown UDP client {}. Must /connect first.", client_id);
        }
        return;
    }

    // Existing client, parse their command.
    int virtual_fd = udp_client_ids_[client_id];
    parse_client_command(virtual_fd, msg);
}


void EpollServer::handle_udp_connect(const std::string& client_id, const sockaddr_in& client_addr) {
  static int virtual_fd_counter = 10000;  // Start from a high number to avoid conflicts
  int virtual_fd = virtual_fd_counter++;
  
  udp_client_ids_[client_id] = virtual_fd;
  udp_fd_to_client_[virtual_fd] = client_id;
  
  client_usernames_[virtual_fd] = "user_" + std::to_string(virtual_fd);
  
  SPDLOG_INFO("New UDP connection from {}, assigned virtual fd {}", client_id, virtual_fd);
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
  send_message(client_sock, welcome.c_str(), welcome.size(), 0);
  SPDLOG_INFO("Client {} assigned username '{}'", client_sock, trimmed_name);
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
  } else if (msg == "/help") {
    handle_help_command(client_sock);
  } else if (msg.rfind("/sendfile ", 0) == 0) {
    handle_sendfile_command(client_sock, msg);
  } else if (msg == "/users") {
    handle_users_command(client_sock);
  } else if (msg.rfind("/dm ", 0) == 0) {
    handle_private_msg_command(client_sock, msg);
  } else if (msg.rfind("/message ", 0) == 0) {
    handle_channel_message(client_sock, msg);
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

void EpollServer::handle_help_command(int client_sock) {
  std::string help_text =
    "Available commands:\n"
    "/list                - List available channels\n"
    "/create <name>       - Create a new channel\n"
    "/join <name>         - Join a channel\n"
    "/users               - List users in current channel\n"
    "/dm @user <message>  - Send a private message\n"
    "/sendfile <filename> - Upload file\n"
    "/help                - Show this help message\n"
    "/message <message>   - Send a message to channel\n";
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
    std::string dm = "[DM] " + usernames_[client_sock] + ": " + msg.substr(space_pos + 1);

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

  std::string full_msg = "[" + ch + "] " + usernames_[client_sock] + ": " + msg.substr(9);
  broadcast_to_channel(ch, full_msg, client_sock);
}

void EpollServer::broadcast_to_channel(const std::string &channel, const std::string &msg, int sender_fd) {
    bool udp_multicast_sent = false;
    for (int fd : channel_mgr_->get_members(channel)) {
        if (fd == sender_fd) continue;

        if (udp_fd_to_client_.count(fd)) {
            // Member is a UDP client. Send the multicast message ONCE.
            if (!udp_multicast_sent) {
                send_multicast(msg);
                udp_multicast_sent = true;
            }
        } else {
            // Member is a TCP client. Send directly.
            send_message(fd, msg);
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
  std::cout<<"Server started with epoll"<<std::endl;
  epoll_event events[kMaxEvents];

  while (true) {
    int nfds = epoll_wait(epoll_fd_, events, kMaxEvents, -1);
    for (int i = 0; i < nfds; ++i) {
      int fd = events[i].data.fd;
      if (fd == listen_sock_) {
        handle_new_connection(); // New TCP connection
      } else if (fd == udp_sock_) {
        handle_udp_data(); // Data on the UDP socket
      } else {
        handle_client_data(fd); // Data from an existing TCP client
      }
    }
  }
}

// This is the overload that had the bug.
int EpollServer::send_message(int client_sock, const char* msg, size_t len, int flags) {
    if (udp_fd_to_client_.count(client_sock)) {
        // Destination is a UDP client, so send via multicast.
        // CORRECTED to use the parameters from this function
        send_multicast(std::string(msg, len));
        return len;
    } else {
        // Destination is a TCP client, send via unicast.
        ssize_t sent = send(client_sock, msg, len, flags);
        if (sent < 0) {
            SPDLOG_ERROR("Failed to send to TCP client {}: {}", client_sock, strerror(errno));
            return -1;
        }
        return sent;
    }
}

// This overload correctly calls the one above. It's fine.
int EpollServer::send_message(int client_sock, const std::string& message) {
    return send_message(client_sock, message.c_str(), message.size(), 0);
}

}