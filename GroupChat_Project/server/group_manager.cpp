#include "server/group_manager.h"

#include <algorithm>
#include <sstream>

namespace gc {

GroupManager::GroupManager(Metrics& metrics, Logger& logger, MemoryPager& pager)
    : metrics_(metrics), logger_(logger), pager_(pager) {}

std::shared_ptr<GroupManager::GroupState> GroupManager::get_or_create(std::uint16_t groupID) {
    std::lock_guard<std::mutex> lock(groupsMutex_);
    auto it = groups_.find(groupID);
    if (it != groups_.end()) {
        return it->second;
    }
    auto group = std::make_shared<GroupState>(groupID, metrics_, pager_);
    groups_[groupID] = group;
    return group;
}

void GroupManager::join_group(std::uint16_t groupID, int clientFd, std::uint32_t senderID, const std::string& token, bool& ok, std::string& reason) {
    auto group = get_or_create(groupID);
    std::unique_lock<std::shared_mutex> lock(group->mutex);
    if (!group->token.empty() && group->token != token) {
        ok = false;
        reason = "invalid group token";
        return;
    }
    if (group->token.empty() && !token.empty()) {
        group->token = token;
    }
    group->members.insert(clientFd);
    ok = true;
    reason = "joined";
    logger_.append_line(now_string() + " JOIN sender=" + std::to_string(senderID) +
                        " group=" + std::to_string(groupID) +
                        " fd=" + std::to_string(clientFd));
}

void GroupManager::leave_group(std::uint16_t groupID, int clientFd) {
    auto group = get_or_create(groupID);
    std::unique_lock<std::shared_mutex> lock(group->mutex);
    group->members.erase(clientFd);
    logger_.append_line(now_string() + " LEAVE group=" + std::to_string(groupID) +
                        " fd=" + std::to_string(clientFd));
}

std::vector<ChatPacket> GroupManager::history_for(std::uint16_t groupID) {
    auto group = get_or_create(groupID);
    std::shared_lock<std::shared_mutex> groupLock(group->mutex);
    return group->cache.recent_messages();
}

std::vector<std::uint16_t> GroupManager::list_groups() {
    std::vector<std::uint16_t> out;
    std::lock_guard<std::mutex> lock(groupsMutex_);
    out.reserve(groups_.size());
    for (const auto& [gid, _] : groups_) {
        (void)_;
        out.push_back(gid);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void GroupManager::add_message(const ChatPacket& packet) {
    auto group = get_or_create(packet.groupID);
    std::unique_lock<std::shared_mutex> groupLock(group->mutex);
    group->cache.insert(packet);
    logger_.append_line(now_string() + " TEXT sender=" + std::to_string(packet.senderID) +
                        " group=" + std::to_string(packet.groupID) +
                        " payload=\"" + payload_as_string(packet) + "\"");
}

void GroupManager::add_media(const ChatPacket& packet) {
    auto group = get_or_create(packet.groupID);
    std::unique_lock<std::shared_mutex> groupLock(group->mutex);
    group->mediaFrames.push_back(packet);
    if (group->mediaFrames.size() > 20) {
        group->mediaFrames.erase(group->mediaFrames.begin());
    }
    logger_.append_line(now_string() + " MEDIA sender=" + std::to_string(packet.senderID) +
                        " group=" + std::to_string(packet.groupID) +
                        " bytes=" + std::to_string(packet.payloadSize));
}

std::vector<int> GroupManager::members_of(std::uint16_t groupID) {
    auto group = get_or_create(groupID);
    std::shared_lock<std::shared_mutex> lock(group->mutex);
    return std::vector<int>(group->members.begin(), group->members.end());
}

void GroupManager::remove_client_from_all(int clientFd) {
    std::lock_guard<std::mutex> guard(groupsMutex_);
    for (auto& [_, group] : groups_) {
        std::unique_lock<std::shared_mutex> lock(group->mutex);
        group->members.erase(clientFd);
    }
}

void GroupManager::flush_all_to_disk() {
    std::lock_guard<std::mutex> guard(groupsMutex_);
    for (auto& [gid, group] : groups_) {
        auto packets = group->cache.dump_all();
        for (const auto& packet : packets) {
            logger_.append_line(now_string() + " FLUSH group=" + std::to_string(gid) +
                                " sender=" + std::to_string(packet.senderID) +
                                " payload=\"" + payload_as_string(packet) + "\"");
        }
    }
}

} // namespace gc
