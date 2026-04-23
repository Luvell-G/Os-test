#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "server/cache.h"
#include "server/memory_pager.h"
#include "shared/logger.h"
#include "shared/metrics.h"
#include "shared/protocol.h"

namespace gc {

class GroupManager {
public:
    GroupManager(Metrics& metrics, Logger& logger, MemoryPager& pager);

    void join_group(std::uint16_t groupID, int clientFd, std::uint32_t senderID, const std::string& token, bool& ok, std::string& reason);
    void leave_group(std::uint16_t groupID, int clientFd);
    std::vector<ChatPacket> history_for(std::uint16_t groupID);
    std::vector<std::uint16_t> list_groups();
    void add_message(const ChatPacket& packet);
    void add_media(const ChatPacket& packet);
    std::vector<int> members_of(std::uint16_t groupID);
    void remove_client_from_all(int clientFd);
    void flush_all_to_disk();

private:
    struct GroupState {
        explicit GroupState(std::uint16_t gid, Metrics& m, MemoryPager& pager)
            : groupID(gid), cache(50, 3600, m, pager, gid) {}
        std::uint16_t groupID;
        mutable std::shared_mutex mutex;
        std::unordered_set<int> members;
        LRUMessageCache cache;
        std::vector<ChatPacket> mediaFrames;
        std::string token;
    };

    std::shared_ptr<GroupState> get_or_create(std::uint16_t groupID);

    Metrics& metrics_;
    Logger& logger_;
    MemoryPager& pager_;
    std::mutex groupsMutex_;
    std::unordered_map<std::uint16_t, std::shared_ptr<GroupState>> groups_;
};

} // namespace gc
