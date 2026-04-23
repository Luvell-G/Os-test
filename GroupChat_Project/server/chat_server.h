#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "server/group_manager.h"
#include "server/thread_pool.h"
#include "shared/logger.h"
#include "shared/metrics.h"

namespace gc {

class ChatServer {
public:
    ChatServer(std::uint16_t port, SchedulePolicy policy, std::size_t workerCount);
    ~ChatServer();

    bool start();
    void run();
    void shutdown();

private:
    struct ClientInfo {
        std::uint32_t senderID {0};
        std::string ip;
    };

    void setup_signals();
    void accept_new_client();
    void remove_client(int fd);
    void process_task(const Task& task);
    void broadcast_to_group(std::uint16_t groupID, const ChatPacket& packet);
    void send_history(int clientFd, std::uint16_t groupID);
    void send_group_list(int clientFd, std::uint32_t senderID);
    void send_error(int clientFd, std::uint16_t groupID, std::uint32_t senderID, const std::string& msg);

    std::uint16_t port_;
    int serverFd_ {-1};
    std::atomic<bool> running_ {false};
    std::atomic<std::uint64_t> nextSeq_ {1};
    std::atomic<std::uint32_t> nextSenderID_ {1000};

    Metrics metrics_;
    Logger logger_ {"logs/chat_log.txt"};
    MemoryPager pager_ {64, metrics_};
    GroupManager groups_ {metrics_, logger_, pager_};
    std::unique_ptr<ThreadPool> pool_;

    std::mutex clientsMutex_;
    std::unordered_map<int, ClientInfo> clients_;
};

} // namespace gc
