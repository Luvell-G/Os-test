#pragma once
#include <atomic>
#include <string>

namespace gc {

struct MetricsSnapshot {
    uint64_t messagesHandled {0};
    uint64_t cacheHits {0};
    uint64_t cacheMisses {0};
    uint64_t cacheEvictions {0};
    uint64_t pageFaults {0};
    uint64_t workerWakeups {0};
    uint64_t clientDisconnects {0};
};

class Metrics {
public:
    void inc_messages() { ++messagesHandled_; }
    void inc_cache_hit() { ++cacheHits_; }
    void inc_cache_miss() { ++cacheMisses_; }
    void inc_cache_eviction() { ++cacheEvictions_; }
    void inc_page_fault() { ++pageFaults_; }
    void inc_worker_wakeup() { ++workerWakeups_; }
    void inc_disconnect() { ++clientDisconnects_; }

    MetricsSnapshot snapshot() const;
    std::string to_string() const;

private:
    std::atomic<uint64_t> messagesHandled_ {0};
    std::atomic<uint64_t> cacheHits_ {0};
    std::atomic<uint64_t> cacheMisses_ {0};
    std::atomic<uint64_t> cacheEvictions_ {0};
    std::atomic<uint64_t> pageFaults_ {0};
    std::atomic<uint64_t> workerWakeups_ {0};
    std::atomic<uint64_t> clientDisconnects_ {0};
};

} // namespace gc
