#include "server/chat_server.h"

#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace gc {
namespace {
std::atomic<bool>* g_running_ptr = nullptr;

void signal_handler(int) {
    if (g_running_ptr) {
        g_running_ptr->store(false);
    }
}
} // namespace

ChatServer::ChatServer(std::uint16_t port, SchedulePolicy policy, std::size_t workerCount)
    : port_(port) {
    pool_ = std::make_unique<ThreadPool>(workerCount, policy,
        [this](const Task& task) { process_task(task); }, metrics_);
}

ChatServer::~ChatServer() {
    shutdown();
}

void ChatServer::setup_signals() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);
    g_running_ptr = &running_;
}

bool ChatServer::start() {
    setup_signals();
    serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        perror("socket");
        return false;
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        return false;
    }
    if (::listen(serverFd_, 32) < 0) {
        perror("listen");
        return false;
    }

    running_ = true;
    std::cout << "Server listening on port " << port_ << std::endl;
    return true;
}

void ChatServer::accept_new_client() {
    sockaddr_in clientAddr {};
    socklen_t len = sizeof(clientAddr);
    int clientFd = ::accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
    if (clientFd < 0) {
        return;
    }

    char ip[INET_ADDRSTRLEN] {};
    inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));

    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_[clientFd] = ClientInfo { nextSenderID_++, ip };
    std::cout << "Accepted client fd=" << clientFd << " ip=" << ip
              << " senderID=" << clients_[clientFd].senderID << std::endl;
}

void ChatServer::remove_client(int fd) {
    groups_.remove_client_from_all(fd);
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(fd);
    }
    ::close(fd);
    metrics_.inc_disconnect();
}

void ChatServer::broadcast_to_group(std::uint16_t groupID, const ChatPacket& packet) {
    auto members = groups_.members_of(groupID);
    for (int memberFd : members) {
        if (!send_packet(memberFd, packet)) {
            remove_client(memberFd);
        }
    }
}

void ChatServer::send_history(int clientFd, std::uint16_t groupID) {
    auto history = groups_.history_for(groupID);
    for (const auto& packet : history) {
        if (!send_packet(clientFd, packet)) {
            remove_client(clientFd);
            return;
        }
    }
}

void ChatServer::send_group_list(int clientFd, std::uint32_t senderID) {
    auto groups = groups_.list_groups();
    std::string payload = "groups:";
    for (std::size_t i = 0; i < groups.size(); ++i) {
        payload += (i == 0 ? " " : ",");
        payload += std::to_string(groups[i]);
    }
    auto response = make_packet(MessageType::MSG_LIST, 0, senderID, payload);
    send_packet(clientFd, response);
}

void ChatServer::send_error(int clientFd, std::uint16_t groupID, std::uint32_t senderID, const std::string& msg) {
    auto packet = make_packet(MessageType::MSG_ERROR, groupID, senderID, msg);
    send_packet(clientFd, packet);
}

void ChatServer::process_task(const Task& task) {
    metrics_.inc_messages();

    std::uint32_t senderID = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(task.clientFd);
        if (it == clients_.end()) {
            return;
        }
        senderID = it->second.senderID;
    }

    switch (static_cast<MessageType>(task.packet.type)) {
        case MessageType::MSG_JOIN: {
            bool ok = false;
            std::string reason;
            groups_.join_group(task.packet.groupID, task.clientFd, senderID, payload_as_string(task.packet), ok, reason);
            if (!ok) {
                send_error(task.clientFd, task.packet.groupID, senderID, reason);
                remove_client(task.clientFd);
                return;
            }
            auto ack = make_packet(MessageType::MSG_JOIN, task.packet.groupID, senderID, "joined group " + std::to_string(task.packet.groupID));
            send_packet(task.clientFd, ack);
            send_history(task.clientFd, task.packet.groupID);
            break;
        }
        case MessageType::MSG_LEAVE: {
            groups_.leave_group(task.packet.groupID, task.clientFd);
            auto ack = make_packet(MessageType::MSG_LEAVE, task.packet.groupID, senderID, "left group " + std::to_string(task.packet.groupID));
            send_packet(task.clientFd, ack);
            break;
        }
        case MessageType::MSG_LIST: {
            send_group_list(task.clientFd, senderID);
            break;
        }
        case MessageType::MSG_TEXT: {
            ChatPacket packet = task.packet;
            packet.senderID = senderID;
            groups_.add_message(packet);
            broadcast_to_group(packet.groupID, packet);
            break;
        }
        case MessageType::MSG_MEDIA: {
            ChatPacket packet = task.packet;
            packet.senderID = senderID;
            groups_.add_media(packet);
            broadcast_to_group(packet.groupID, packet);
            break;
        }
        default:
            send_error(task.clientFd, task.packet.groupID, senderID, "unknown packet type");
            break;
    }
}

void ChatServer::run() {
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverFd_, &readfds);
        int maxfd = serverFd_;

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (const auto& [fd, _] : clients_) {
                FD_SET(fd, &readfds);
                if (fd > maxfd) {
                    maxfd = fd;
                }
            }
        }

        timeval timeout {};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        const int ready = ::select(maxfd + 1, &readfds, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            break;
        }
        if (ready == 0) {
            continue;
        }

        if (FD_ISSET(serverFd_, &readfds)) {
            accept_new_client();
        }

        std::vector<int> readyClients;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (const auto& [fd, _] : clients_) {
                if (FD_ISSET(fd, &readfds)) {
                    readyClients.push_back(fd);
                }
            }
        }

        for (int fd : readyClients) {
            ChatPacket packet {};
            if (!recv_packet(fd, packet)) {
                std::cerr << "Disconnect or invalid packet on fd=" << fd << std::endl;
                remove_client(fd);
                continue;
            }

            Task task {};
            task.clientFd = fd;
            task.packet = packet;
            task.sequence = nextSeq_++;
            task.arrivalTime = unix_time_now();
            task.estimatedCost = packet.payloadSize;
            pool_->enqueue(task);
        }
    }
    shutdown();
}

void ChatServer::shutdown() {
    if (!running_.exchange(false)) {
        return;
    }

    groups_.flush_all_to_disk();
    logger_.append_line(now_string() + " SHUTDOWN " + metrics_.to_string());

    if (serverFd_ >= 0) {
        ::close(serverFd_);
        serverFd_ = -1;
    }

    std::vector<int> fds;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& [fd, _] : clients_) {
            fds.push_back(fd);
        }
        clients_.clear();
    }
    for (int fd : fds) {
        ::close(fd);
    }
}

} // namespace gc
