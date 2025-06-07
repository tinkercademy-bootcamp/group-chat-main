#ifndef TEST_CLIENT_WRAPPER_H
#define TEST_CLIENT_WRAPPER_H

#include <string>
#include <chrono>
#include <atomic> 

namespace tt::chat::test {

struct TestClientStats {
    long long messages_sent = 0;
    long long messages_received = 0;
    long long bytes_sent = 0;
    long long bytes_received = 0;
    bool connection_successful = false;
    std::string error_message = "";
    int client_id = 0;
};

class TestClient {
public:
    // Constructor will be added later
    // Destructor will be added later

    // Placeholder methods
    // void run();
    // const TestClientStats& get_stats();

private:
    TestClientStats stats_;
    int client_id_; // To be initialized by constructor
};

} 
#endif 