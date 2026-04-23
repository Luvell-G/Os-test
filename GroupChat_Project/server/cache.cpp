#include "server/cache.h"

#include <algorithm>

namespace gc {

LRUMessageCache::LRUMessageCache(std::size_t capacity, std::uint32_t ttlSeconds, Metrics& metrics, MemoryPager& pager, std::uint16_t groupID)
    : capacity_(capacity), ttlSeconds_(ttlSeconds), metrics_(metrics), pager_(pager), groupID_(groupID) {}

void LRUMessageCache::touch_locked(std::list<Entry>::iterator it) {
    pager_.touch_or_load((static_cast<std::uint64_t>(groupID_) << 32U) | it->messageId);
    entries_.splice(entries_.begin(), entries_, it);
}

void LRUMessageCache::purge_expired_locked(std::uint32_t now) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (now - it->insertedAt > ttlSeconds_) {
            pager_.release_page((static_cast<std::uint64_t>(groupID_) << 32U) | it->messageId);
            index_.erase(it->messageId);
            it = entries_.erase(it);
            metrics_.inc_cache_eviction();
        } else {
            ++it;
        }
    }
}

void LRUMessageCache::insert(const ChatPacket& packet) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    purge_expired_locked(unix_time_now());

    if (entries_.size() >= capacity_ && !entries_.empty()) {
        auto& victim = entries_.back();
        pager_.release_page((static_cast<std::uint64_t>(groupID_) << 32U) | victim.messageId);
        index_.erase(victim.messageId);
        entries_.pop_back();
        metrics_.inc_cache_eviction();
    }

    Entry e {};
    e.messageId = nextId_++;
    e.packet = packet;
    e.insertedAt = unix_time_now();
    entries_.push_front(e);
    index_[e.messageId] = entries_.begin();
    pager_.touch_or_load((static_cast<std::uint64_t>(groupID_) << 32U) | e.messageId);
}

std::vector<ChatPacket> LRUMessageCache::recent_messages() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    purge_expired_locked(unix_time_now());
    std::vector<ChatPacket> result;
    result.reserve(entries_.size());
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        metrics_.inc_cache_hit();
        touch_locked(it);
        result.push_back(it->packet);
    }
    std::reverse(result.begin(), result.end());
    if (result.empty()) {
        metrics_.inc_cache_miss();
    }
    return result;
}

void LRUMessageCache::flush_expired() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    purge_expired_locked(unix_time_now());
}

std::vector<ChatPacket> LRUMessageCache::dump_all() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<ChatPacket> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) {
        result.push_back(entry.packet);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace gc
