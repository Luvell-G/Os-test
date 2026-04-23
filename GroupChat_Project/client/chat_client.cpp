#include "client/chat_client.h"

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

#include "shared/protocol.h"

namespace gc {

ChatClient::ChatClient(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

ChatClient::~ChatClient() {
    running_ = false;
    if (sockfd_ >= 0) {
        ::shutdown(sockfd_, SHUT_RDWR);
        ::close(sockfd_);
    }
    if (receiver_.joinable()) {
        receiver_.join();
    }
}

bool ChatClient::connect_to_server() {
    sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid host: " << host_ << std::endl;
        return false;
    }

    if (::connect(sockfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        return false;
    }

    running_ = true;
    receiver_ = std::thread([this] { receiver_loop(); });
    return true;
}

void ChatClient::print_help() const {
    std::cout
        << "Commands:\n"
        << "  /join <groupID> [token]\n"
        << "  /leave <groupID>\n"
        << "  /switch <groupID>\n"
        << "  /list\n"
        << "  /media <text pretending to be bytes>\n"
        << "  /quit\n"
        << "  anything else = send text to current group\n";
}

void ChatClient::receiver_loop() {
    while (running_) {
        ChatPacket packet {};
        if (!recv_packet(sockfd_, packet)) {
            if (running_) {
                std::cout << "\n[server disconnected]\n";
            }
            running_ = false;
            break;
        }

        if (localSenderID_ == 0 && packet.senderID != 0) {
            localSenderID_ = packet.senderID;
        }

        std::cout << "\n[" << message_type_name(packet.type)
                  << " group=" << packet.groupID
                  << " sender=" << packet.senderID
                  << " ts=" << packet.timestamp << "] "
                  << payload_as_string(packet) << "\n> " << std::flush;
    }
}

void ChatClient::run() {
    print_help();
    std::cout << "> " << std::flush;
    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> " << std::flush;
            continue;
        }

        if (line == "/quit") {
            running_ = false;
            break;
        }

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        ChatPacket packet {};
        if (command == "/join") {
            int group = 0;
            std::string token;
            iss >> group;
            iss >> token;
            currentGroup_ = static_cast<std::uint16_t>(group);
            packet = make_packet(MessageType::MSG_JOIN, currentGroup_, localSenderID_, token);
        } else if (command == "/leave") {
            int group = 0;
            iss >> group;
            packet = make_packet(MessageType::MSG_LEAVE, static_cast<std::uint16_t>(group), localSenderID_, "");
        } else if (command == "/switch") {
            int group = 0;
            iss >> group;
            currentGroup_ = static_cast<std::uint16_t>(group);
            packet = make_packet(MessageType::MSG_JOIN, currentGroup_, localSenderID_, "");
        } else if (command == "/list") {
            packet = make_packet(MessageType::MSG_LIST, 0, localSenderID_, "");
        } else if (command == "/media") {
            std::string payload;
            std::getline(iss, payload);
            if (!payload.empty() && payload[0] == ' ') {
                payload.erase(0, 1);
            }
            packet = make_packet(MessageType::MSG_MEDIA, currentGroup_, localSenderID_, payload);
        } else {
            packet = make_packet(MessageType::MSG_TEXT, currentGroup_, localSenderID_, line);
        }

        if (!send_packet(sockfd_, packet)) {
            std::cout << "Failed to send packet.\n";
            running_ = false;
            break;
        }

        std::cout << "> " << std::flush;
    }
}

} // namespace gc
