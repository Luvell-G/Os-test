#include "server/memory_pager.h"

namespace gc {

MemoryPager::MemoryPager(std::size_t pageCount, Metrics& metrics)
    : capacity_(pageCount), metrics_(metrics) {}

void MemoryPager::touch_or_load(std::uint64_t pageKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(pageKey);
    if (it != index_.end()) {
        lru_.splice(lru_.begin(), lru_, it->second);
        it->second = lru_.begin();
        return;
    }

    metrics_.inc_page_fault();
    if (index_.size() >= capacity_ && !lru_.empty()) {
        auto victim = lru_.back();
        lru_.pop_back();
        index_.erase(victim);
    }
    lru_.push_front(pageKey);
    index_[pageKey] = lru_.begin();
}

void MemoryPager::release_page(std::uint64_t pageKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(pageKey);
    if (it == index_.end()) {
        return;
    }
    lru_.erase(it->second);
    index_.erase(it);
}

} // namespace gc
