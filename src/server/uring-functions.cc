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

void EpollServer::submit_recv(int client_fd) {
  auto* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    SPDLOG_ERROR("Failed to get SQE for recv");
    return;
  }
  
  auto* ctx = new IoUringContext{IO_RECV, client_fd, new char[1024], 1024, nullptr, nullptr};
  io_uring_prep_recv(sqe, client_fd, ctx->buffer, 1024, 0);
  io_uring_sqe_set_data(sqe, ctx);
  io_uring_submit(&ring_);
}

void EpollServer::submit_send(int client_fd, const std::string& message) {
  auto* sqe = io_uring_get_sqe(&ring_);
  if (!sqe) {
    SPDLOG_ERROR("Failed to get SQE for send");
    return;
  }
  
  auto* buffer = new char[message.size()];
  memcpy(buffer, message.c_str(), message.size());
  
  auto* ctx = new IoUringContext{IO_SEND, client_fd, buffer, message.size(), nullptr, nullptr};
  io_uring_prep_send(sqe, client_fd, buffer, message.size(), 0);
  io_uring_sqe_set_data(sqe, ctx);
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
        case IO_ACCEPT: handle_accept_completion(cqe->res, ctx); break;
        case IO_RECV:   handle_recv_completion(cqe->res, ctx); break;
        case IO_SEND:   handle_send_completion(cqe->res, ctx); break;
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
    submit_recv(result);
  }
  
  submit_accept(); // Continue accepting
  delete ctx->client_addr;
  delete ctx->addr_len;
  delete ctx;
}

void EpollServer::handle_recv_completion(int result, IoUringContext* ctx) {
  if (result <= 0) {
    handle_client_disconnect(ctx->client_fd);
  } else {
    std::string msg(ctx->buffer, result);
    parse_client_command(ctx->client_fd, msg);
    submit_recv(ctx->client_fd); // Continue receiving
  }
  
  delete[] ctx->buffer;
  delete ctx;
}

void EpollServer::handle_send_completion(int result, IoUringContext* ctx) {
  if (result < 0) {
    SPDLOG_ERROR("Send failed for client {}: {}", ctx->client_fd, strerror(-result));
  }
  
  delete[] ctx->buffer;
  delete ctx;
}

void EpollServer::handle_client_disconnect(int client_fd) {
  SPDLOG_INFO("Client {} disconnected from uring world!", client_fd);
  
  if (auto it = client_channels_.find(client_fd); it != client_channels_.end()) {
    // channel_mgr_->leave_channel(it->second, client_fd);
    client_channels_.erase(it);
  }
  
  client_usernames_.erase(client_fd);
  usernames_.erase(client_fd);
  close(client_fd);
}

int EpollServer::send_message_uring(int client_sock, const std::string& message) {
  submit_send(client_sock, message);
  return message.size();
}
#endif

}