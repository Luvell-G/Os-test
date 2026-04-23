#include "shared/metrics.h"

#include <sstream>

namespace gc {

MetricsSnapshot Metrics::snapshot() const {
    return MetricsSnapshot {
        messagesHandled_.load(),
        cacheHits_.load(),
        cacheMisses_.load(),
        cacheEvictions_.load(),
        pageFaults_.load(),
        workerWakeups_.load(),
        clientDisconnects_.load()
    };
}

std::string Metrics::to_string() const {
    const auto s = snapshot();
    std::ostringstream oss;
    oss << "messages=" << s.messagesHandled
        << " cache_hits=" << s.cacheHits
        << " cache_misses=" << s.cacheMisses
        << " cache_evictions=" << s.cacheEvictions
        << " page_faults=" << s.pageFaults
        << " worker_wakeups=" << s.workerWakeups
        << " disconnects=" << s.clientDisconnects;
    return oss.str();
}

} // namespace gc
