#include "epoll-server.h"
#include "../utils.h"
#include "../net/chat-sockets.h"

#include "channel_manager.h"

#include <spdlog/spdlog.h>
#include <fstream>

namespace tt::chat::server {

#ifdef IO_URING_ENABLED
void EpollServer::setup_io_uring() {
  int ret = io_uring_queue_init(QUEUE_DEPTH, &ring_, 0);
  check_error(ret < 0, "io_uring_queue_init failed");
}

void EpollServer::submit_accept() {
  auto* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    SPDLOG_ERROR("Failed to get SQE for accept");
    return;
  }
  auto* ctx = new IoUringContext{IO_ACCEPT, 0, nullptr, 0, new sockaddr_in(), new socklen_t(sizeof(sockaddr_in))};
  io_uring_prep_accept(sqe, listen_sock_, (sockaddr*)ctx->client_addr, ctx->addr_len, 0);
  io_uring_sqe_set_data(sqe, ctx);
  io_uring_submit(&ring_);
}

void EpollServer::submit_recv_length(int client_fd) {
  auto* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    SPDLOG_ERROR("Failed to get SQE for recv length");
    return;
  }
  
  // Allocate buffer for 20-byte length prefix
  auto* ctx = new IoUringContext{IO_RECV_LENGTH, client_fd, new char[20], 20, nullptr, nullptr};
  io_uring_prep_recv(sqe, client_fd, ctx->buffer, 20, 0);
  io_uring_sqe_set_data(sqe, ctx);
  io_uring_submit(&ring_);
}

void EpollServer::submit_recv_message(int client_fd, int message_length) {
  auto* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    SPDLOG_ERROR("Failed to get SQE for recv message");
    return;
  }
  
  // Allocate buffer for actual message
  auto* ctx = new IoUringContext{IO_RECV_MESSAGE, client_fd, new char[message_length], message_length, nullptr, nullptr};
  io_uring_prep_recv(sqe, client_fd, ctx->buffer, message_length, 0);
  io_uring_sqe_set_data(sqe, ctx);
  io_uring_submit(&ring_);
}

void EpollServer::submit_send(int client_fd, const std::string& message) {
  // First, send the length prefix
  std::string msg_sz_str = std::to_string(message.size());
  if (msg_sz_str.length() > 20) {
    SPDLOG_ERROR("Message size ({}) exceeds 20-character length prefix limit for client {}",
                 message.size(), client_fd);
    msg_sz_str = msg_sz_str.substr(msg_sz_str.length() - 20);
  }
  msg_sz_str = std::string(20 - msg_sz_str.length(), '0') + msg_sz_str;
  
  // Submit length prefix send
  auto* sqe_len = io_uring_get_sqe(&ring_);
  if (!sqe_len) {
    SPDLOG_ERROR("Failed to get SQE for send length");
    return;
  }
  
  auto* len_buffer = new char[20];
  memcpy(len_buffer, msg_sz_str.c_str(), 20);
  
  auto* ctx_len = new IoUringContext{IO_SEND_LENGTH, client_fd, len_buffer, 20, nullptr, nullptr};
  io_uring_prep_send(sqe_len, client_fd, len_buffer, 20, 0);
  io_uring_sqe_set_data(sqe_len, ctx_len);
  
  // Submit message send
  auto* sqe_msg = io_uring_get_sqe(&ring_);
  if (!sqe_msg) {
    SPDLOG_ERROR("Failed to get SQE for send message");
    delete[] len_buffer;
    delete ctx_len;
    return;
  }
  
  auto* msg_buffer = new char[message.size()];
  memcpy(msg_buffer, message.c_str(), message.size());
  
  auto* ctx_msg = new IoUringContext{IO_SEND_MESSAGE, client_fd, msg_buffer, message.size(), nullptr, nullptr};
  io_uring_prep_send(sqe_msg, client_fd, msg_buffer, message.size(), 0);
  io_uring_sqe_set_data(sqe_msg, ctx_msg);
  
  io_uring_submit(&ring_);
}

void EpollServer::handle_io_uring_events() {
  struct io_uring_cqe* cqe;
  unsigned head;
  unsigned count = 0;
  
  // Process multiple events in batch for better performance
  io_uring_for_each_cqe(&ring_, head, cqe) {
    auto* ctx = static_cast<IoUringContext*>(io_uring_cqe_get_data(cqe));
    if (ctx) {
      switch (ctx->op_type) {
        case IO_ACCEPT: 
          handle_accept_completion(cqe->res, ctx); 
          break;
        case IO_RECV_LENGTH:   
          handle_recv_length_completion(cqe->res, ctx); 
          break;
        case IO_RECV_MESSAGE:   
          handle_recv_message_completion(cqe->res, ctx); 
          break;
        case IO_SEND_LENGTH:   
        case IO_SEND_MESSAGE:   
          handle_send_completion(cqe->res, ctx); 
          break;
      }
    }
    count++;
  }
  
  io_uring_cq_advance(&ring_, count);
}

void EpollServer::handle_accept_completion(int result, IoUringContext* ctx) {
  if (result < 0) {
    SPDLOG_ERROR("Accept failed: {}", strerror(-result));
  } else {
    client_usernames_[result] = "user_" + std::to_string(result);
    SPDLOG_INFO("New connection: {}", client_usernames_[result]);
    submit_recv_length(result); // Start by reading length prefix
  }
  
  submit_accept(); // Continue accepting
  delete ctx->client_addr;
  delete ctx->addr_len;
  delete ctx;
}

void EpollServer::handle_recv_length_completion(int result, IoUringContext* ctx) {
  if (result <= 0) {
    SPDLOG_INFO("Client {} disconnected during length read", ctx->client_fd);
    handle_client_disconnect(ctx->client_fd);
    delete[] ctx->buffer;
    delete ctx;
    return;
  }
  
  if (result < 20) {
    SPDLOG_WARN("Client {} sent incomplete length prefix: {} bytes", ctx->client_fd, result);
    handle_client_disconnect(ctx->client_fd);
    delete[] ctx->buffer;
    delete ctx;
    return;
  }
  
  // Parse the length
  std::string len_str(ctx->buffer, 20);
  int msg_length = std::atoi(len_str.c_str());
  
  if (msg_length <= 0 || msg_length > MAX_MESSAGE_SIZE) {
    SPDLOG_WARN("Client {} sent invalid message length: {}", ctx->client_fd, msg_length);
    handle_client_disconnect(ctx->client_fd);
    delete[] ctx->buffer;
    delete ctx;
    return;
  }
  
  SPDLOG_DEBUG("Client {} sending message of length {}", ctx->client_fd, msg_length);
  
  // Now read the actual message
  submit_recv_message(ctx->client_fd, msg_length);
  
  delete[] ctx->buffer;
  delete ctx;
}

void EpollServer::handle_recv_message_completion(int result, IoUringContext* ctx) {
  if (result <= 0) {
    SPDLOG_INFO("Client {} disconnected during message read", ctx->client_fd);
    handle_client_disconnect(ctx->client_fd);
    delete[] ctx->buffer;
    delete ctx;
    return;
  }
  
  if (result < ctx->buffer_size) {
    SPDLOG_WARN("Client {} sent incomplete message: {} of {} bytes", 
                ctx->client_fd, result, ctx->buffer_size);
    handle_client_disconnect(ctx->client_fd);
    delete[] ctx->buffer;
    delete ctx;
    return;
  }
  
  // Process the complete message
  std::string msg(ctx->buffer, result);
  SPDLOG_INFO("Received from client {}: length={} message='{}'", ctx->client_fd, result, msg);
  parse_client_command(ctx->client_fd, msg);
  
  // Continue reading next message (start with length prefix)
  submit_recv_length(ctx->client_fd);
  
  delete[] ctx->buffer;
  delete ctx;
}

void EpollServer::handle_send_completion(int result, IoUringContext* ctx) {
  if (result < 0) {
    SPDLOG_ERROR("Send failed for client {}: {}", ctx->client_fd, strerror(-result));
    handle_client_disconnect(ctx->client_fd);
  } else if (result < ctx->buffer_size) {
    SPDLOG_WARN("Incomplete send to client {}: {} of {} bytes", 
                ctx->client_fd, result, ctx->buffer_size);
  }
  
  delete[] ctx->buffer;
  delete ctx;
}

void EpollServer::handle_client_disconnect(int client_fd) {
  SPDLOG_INFO("Client {} disconnected from uring world!", client_fd);
  
  // Remove from channel
  // if (auto it = client_channels_.find(client_fd); it != client_channels_.end()) {
  //   channel_mgr_->leave_channel(it->second, client_fd);
  //   client_channels_.erase(it);
  // }
  
  // Remove username tracking
  // if (usernames_.count(client_fd)) {
  //   username_set_.erase(usernames_[client_fd]);
  //   usernames_.erase(client_fd);
  // }
  // client_usernames_.erase(client_fd);
  
  // close(client_fd);
}

int EpollServer::send_message_uring(int client_sock, const std::string& message) {
  submit_send(client_sock, message);
  return message.size();
}
#endif

}