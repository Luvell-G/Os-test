#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "shared/metrics.h"
#include "shared/protocol.h"

namespace gc {

enum class SchedulePolicy {
    RR,
    SJF
};

struct Task {
    int clientFd {-1};
    ChatPacket packet {};
    std::uint64_t sequence {0};
    std::uint32_t arrivalTime {0};
    std::uint16_t estimatedCost {0};
};

struct SJFComparator {
    bool operator()(const Task& a, const Task& b) const {
        const auto agedA = static_cast<int>(a.estimatedCost) - static_cast<int>((unix_time_now() - a.arrivalTime) / 2U);
        const auto agedB = static_cast<int>(b.estimatedCost) - static_cast<int>((unix_time_now() - b.arrivalTime) / 2U);
        if (agedA == agedB) {
            return a.sequence > b.sequence;
        }
        return agedA > agedB;
    }
};

class ThreadPool {
public:
    using Handler = std::function<void(const Task&)>;

    ThreadPool(std::size_t count, SchedulePolicy policy, Handler handler, Metrics& metrics);
    ~ThreadPool();

    void enqueue(Task task);

private:
    std::optional<Task> pop_task();

    SchedulePolicy policy_;
    Handler handler_;
    Metrics& metrics_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ {false};
    std::queue<Task> rrQueue_;
    std::priority_queue<Task, std::vector<Task>, SJFComparator> sjfQueue_;
    std::vector<std::thread> workers_;
};

SchedulePolicy parse_policy(const std::string& policyText);

} // namespace gc
