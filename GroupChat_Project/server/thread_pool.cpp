#include "server/thread_pool.h"

namespace gc {

ThreadPool::ThreadPool(std::size_t count, SchedulePolicy policy, Handler handler, Metrics& metrics)
    : policy_(policy), handler_(std::move(handler)), metrics_(metrics) {
    for (std::size_t i = 0; i < count; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                auto task = pop_task();
                if (!task.has_value()) {
                    return;
                }
                metrics_.inc_worker_wakeup();
                handler_(*task);
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::enqueue(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (policy_ == SchedulePolicy::RR) {
            rrQueue_.push(std::move(task));
        } else {
            sjfQueue_.push(std::move(task));
        }
    }
    cv_.notify_one();
}

std::optional<Task> ThreadPool::pop_task() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
        return stop_ || !rrQueue_.empty() || !sjfQueue_.empty();
    });

    if (stop_ && rrQueue_.empty() && sjfQueue_.empty()) {
        return std::nullopt;
    }

    Task task {};
    if (policy_ == SchedulePolicy::RR) {
        task = rrQueue_.front();
        rrQueue_.pop();
    } else {
        task = sjfQueue_.top();
        sjfQueue_.pop();
    }
    return task;
}

SchedulePolicy parse_policy(const std::string& policyText) {
    if (policyText == "sjf") {
        return SchedulePolicy::SJF;
    }
    return SchedulePolicy::RR;
}

} // namespace gc
