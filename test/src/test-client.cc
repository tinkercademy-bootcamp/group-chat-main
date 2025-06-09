#include "test-client.h"
#include <algorithm> 
#include <iostream>  

namespace tt::chat::test {

TestClient::TestClient(
 int id,
 const std::string& server_ip, int server_port,
 int num_messages_to_send,
  int message_size_bytes,
 bool listen_for_replies, 
 int client_think_time_ms,
const std::string& common_channel_name)

    : client_id_(id),
      server_ip_param_(server_ip),
      server_port_param_(server_port),
      num_messages_to_send_param_(num_messages_to_send),
      message_size_bytes_param_(std::max(1, message_size_bytes)),
      listen_for_replies_param_(listen_for_replies),
      client_think_time_ms_param_(client_think_time_ms),
      common_channel_name_param_(common_channel_name) {

    stats_.client_id = id;
  }


TestClient::~TestClient() {}

}