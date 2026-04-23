#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace gc {

constexpr std::size_t MAX_PAYLOAD_SIZE = 256;
constexpr std::size_t PACKET_WIRE_SIZE = 271; // 269 + 2-byte checksum

enum class MessageType : uint8_t {
    MSG_TEXT  = 0x01,
    MSG_JOIN  = 0x02,
    MSG_LEAVE = 0x03,
    MSG_LIST  = 0x04,
    MSG_MEDIA = 0x05,
    MSG_ERROR = 0xFF
};

#pragma pack(push, 1)
struct ChatPacket {
    uint8_t type {0};
    uint16_t groupID {0};
    uint32_t senderID {0};
    uint32_t timestamp {0};
    uint16_t payloadSize {0};
    char payload[MAX_PAYLOAD_SIZE] {};
};
#pragma pack(pop)

static_assert(sizeof(ChatPacket) == 269, "ChatPacket must stay fixed at 269 bytes.");

struct PacketEnvelope {
    ChatPacket packet {};
    uint16_t checksum {0};
};

std::string now_string();
uint32_t unix_time_now();
uint16_t compute_checksum(const ChatPacket& packet);
PacketEnvelope to_network_envelope(const ChatPacket& hostPacket);
PacketEnvelope from_network_envelope(const PacketEnvelope& networkEnvelope);
bool verify_checksum(const PacketEnvelope& hostEnvelope);

bool send_all(int fd, const void* data, std::size_t len);
bool recv_all(int fd, void* data, std::size_t len);

bool send_packet(int fd, const ChatPacket& hostPacket);
bool recv_packet(int fd, ChatPacket& outHostPacket);

ChatPacket make_packet(MessageType type, uint16_t groupID, uint32_t senderID, const std::string& payload);
std::string payload_as_string(const ChatPacket& packet);

std::string message_type_name(uint8_t type);

} // namespace gc
