#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace gc {

class ChatClient {
public:
    ChatClient(std::string host, std::uint16_t port);
    ~ChatClient();

    bool connect_to_server();
    void run();

private:
    void receiver_loop();
    void print_help() const;

    std::string host_;
    std::uint16_t port_;
    int sockfd_ {-1};
    std::atomic<bool> running_ {false};
    std::thread receiver_;
    std::uint32_t localSenderID_ {0};
    std::uint16_t currentGroup_ {1};
};

} // namespace gc
