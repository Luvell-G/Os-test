#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>

#include "shared/metrics.h"

namespace gc {

class MemoryPager {
public:
    MemoryPager(std::size_t pageCount, Metrics& metrics);

    void touch_or_load(std::uint64_t pageKey);
    void release_page(std::uint64_t pageKey);

private:
    std::size_t capacity_;
    Metrics& metrics_;
    std::mutex mutex_;
    std::list<std::uint64_t> lru_;
    std::unordered_map<std::uint64_t, std::list<std::uint64_t>::iterator> index_;
};

} // namespace gc
