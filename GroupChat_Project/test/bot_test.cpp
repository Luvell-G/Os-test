#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "shared/protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int connect_client(std::uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

}

int main(int argc, char* argv[]) {
    std::uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<std::uint16_t>(std::stoi(argv[1]));
    }

    const int a = connect_client(port);
    const int b = connect_client(port);

    if (a < 0 || b < 0) {
        std::cerr << "Could not connect test clients.\n";
        return EXIT_FAILURE;
    }

    gc::send_packet(a, gc::make_packet(gc::MessageType::MSG_JOIN, 7, 0, ""));
    gc::send_packet(b, gc::make_packet(gc::MessageType::MSG_JOIN, 7, 0, ""));
    gc::send_packet(a, gc::make_packet(gc::MessageType::MSG_TEXT, 7, 0, "hello from bot A"));

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    gc::ChatPacket packet {};
    bool sawText = false;
    for (int i = 0; i < 3; ++i) {
        bool got = gc::recv_packet(b, packet);
        if (!got) {
            std::cerr << "No packet received by bot B.\n";
            return EXIT_FAILURE;
        }
        std::cout << "Bot B received: " << gc::payload_as_string(packet) << '\n';
        if (gc::payload_as_string(packet).find("hello from bot A") != std::string::npos) {
            sawText = true;
            break;
        }
    }
    if (!sawText) {
        std::cerr << "Bot B never saw the broadcast text.\n";
        return EXIT_FAILURE;
    }

    ::close(a);
    ::close(b);
    return EXIT_SUCCESS;
}
