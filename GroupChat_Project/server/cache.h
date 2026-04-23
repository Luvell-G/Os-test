#pragma once

#include <cstdint>
#include <list>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "server/memory_pager.h"
#include "shared/metrics.h"
#include "shared/protocol.h"

namespace gc {

class LRUMessageCache {
public:
    struct Entry {
        std::uint64_t messageId {0};
        ChatPacket packet {};
        std::uint32_t insertedAt {0};
    };

    LRUMessageCache(std::size_t capacity, std::uint32_t ttlSeconds, Metrics& metrics, MemoryPager& pager, std::uint16_t groupID);

    void insert(const ChatPacket& packet);
    std::vector<ChatPacket> recent_messages();
    void flush_expired();
    std::vector<ChatPacket> dump_all();

private:
    void purge_expired_locked(std::uint32_t now);
    void touch_locked(std::list<Entry>::iterator it);

    std::size_t capacity_;
    std::uint32_t ttlSeconds_;
    Metrics& metrics_;
    MemoryPager& pager_;
    std::uint16_t groupID_;
    std::shared_mutex mutex_;
    std::list<Entry> entries_;
    std::unordered_map<std::uint64_t, std::list<Entry>::iterator> index_;
    std::uint64_t nextId_ {1};
};

} // namespace gc
