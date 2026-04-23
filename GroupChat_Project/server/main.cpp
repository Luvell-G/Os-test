#include <cstdlib>
#include <iostream>
#include <string>

#include "server/chat_server.h"
#include "server/thread_pool.h"

int main(int argc, char* argv[]) {
    std::uint16_t port = 8080;
    std::string sched = "rr";
    std::size_t workers = 4;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--sched" && i + 1 < argc) {
            sched = argv[++i];
        } else if (arg == "--workers" && i + 1 < argc) {
            workers = static_cast<std::size_t>(std::stoul(argv[++i]));
        }
    }

    gc::ChatServer server(port, gc::parse_policy(sched), workers);
    if (!server.start()) {
        return EXIT_FAILURE;
    }
    server.run();
    return EXIT_SUCCESS;
}
