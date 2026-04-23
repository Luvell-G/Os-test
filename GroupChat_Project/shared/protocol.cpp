#include "shared/protocol.h"

#include <arpa/inet.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unistd.h>

namespace gc {

std::string now_string() {
    std::time_t t = std::time(nullptr);
    std::tm tm {};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

uint32_t unix_time_now() {
    return static_cast<uint32_t>(std::time(nullptr));
}

uint16_t compute_checksum(const ChatPacket& packet) {
    uint16_t acc = 0;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&packet);
    for (std::size_t i = 0; i < sizeof(ChatPacket); ++i) {
        acc ^= static_cast<uint16_t>(bytes[i]);
    }
    return acc;
}

PacketEnvelope to_network_envelope(const ChatPacket& hostPacket) {
    PacketEnvelope env {};
    env.packet.type = hostPacket.type;
    env.packet.groupID = htons(hostPacket.groupID);
    env.packet.senderID = htonl(hostPacket.senderID);
    env.packet.timestamp = htonl(hostPacket.timestamp);
    env.packet.payloadSize = htons(hostPacket.payloadSize);
    std::memcpy(env.packet.payload, hostPacket.payload, MAX_PAYLOAD_SIZE);
    env.checksum = htons(compute_checksum(hostPacket));
    return env;
}

PacketEnvelope from_network_envelope(const PacketEnvelope& networkEnvelope) {
    PacketEnvelope host {};
    host.packet.type = networkEnvelope.packet.type;
    host.packet.groupID = ntohs(networkEnvelope.packet.groupID);
    host.packet.senderID = ntohl(networkEnvelope.packet.senderID);
    host.packet.timestamp = ntohl(networkEnvelope.packet.timestamp);
    host.packet.payloadSize = ntohs(networkEnvelope.packet.payloadSize);
    std::memcpy(host.packet.payload, networkEnvelope.packet.payload, MAX_PAYLOAD_SIZE);
    host.checksum = ntohs(networkEnvelope.checksum);
    return host;
}

bool verify_checksum(const PacketEnvelope& hostEnvelope) {
    return compute_checksum(hostEnvelope.packet) == hostEnvelope.checksum;
}

bool send_all(int fd, const void* data, std::size_t len) {
    const auto* ptr = static_cast<const uint8_t*>(data);
    std::size_t sent = 0;
    while (sent < len) {
        const ssize_t n = ::send(fd, ptr + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool recv_all(int fd, void* data, std::size_t len) {
    auto* ptr = static_cast<uint8_t*>(data);
    std::size_t recvd = 0;
    while (recvd < len) {
        const ssize_t n = ::recv(fd, ptr + recvd, len - recvd, 0);
        if (n <= 0) {
            return false;
        }
        recvd += static_cast<std::size_t>(n);
    }
    return true;
}

bool send_packet(int fd, const ChatPacket& hostPacket) {
    PacketEnvelope env = to_network_envelope(hostPacket);
    return send_all(fd, &env, sizeof(env));
}

bool recv_packet(int fd, ChatPacket& outHostPacket) {
    PacketEnvelope networkEnv {};
    if (!recv_all(fd, &networkEnv, sizeof(networkEnv))) {
        return false;
    }
    PacketEnvelope host = from_network_envelope(networkEnv);
    if (!verify_checksum(host)) {
        return false;
    }
    if (host.packet.payloadSize > MAX_PAYLOAD_SIZE) {
        return false;
    }
    outHostPacket = host.packet;
    return true;
}

ChatPacket make_packet(MessageType type, uint16_t groupID, uint32_t senderID, const std::string& payload) {
    ChatPacket packet {};
    packet.type = static_cast<uint8_t>(type);
    packet.groupID = groupID;
    packet.senderID = senderID;
    packet.timestamp = unix_time_now();
    packet.payloadSize = static_cast<uint16_t>(std::min<std::size_t>(payload.size(), MAX_PAYLOAD_SIZE));
    std::memcpy(packet.payload, payload.data(), packet.payloadSize);
    return packet;
}

std::string payload_as_string(const ChatPacket& packet) {
    const std::size_t n = std::min<std::size_t>(packet.payloadSize, MAX_PAYLOAD_SIZE);
    return std::string(packet.payload, packet.payload + n);
}

std::string message_type_name(uint8_t type) {
    switch (static_cast<MessageType>(type)) {
        case MessageType::MSG_TEXT: return "MSG_TEXT";
        case MessageType::MSG_JOIN: return "MSG_JOIN";
        case MessageType::MSG_LEAVE: return "MSG_LEAVE";
        case MessageType::MSG_LIST: return "MSG_LIST";
        case MessageType::MSG_MEDIA: return "MSG_MEDIA";
        case MessageType::MSG_ERROR: return "MSG_ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace gc
