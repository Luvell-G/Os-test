#include <cstdlib>
#include <iostream>
#include <string>

#include "client/chat_client.h"

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::uint16_t port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
    }

    gc::ChatClient client(host, port);
    if (!client.connect_to_server()) {
        return EXIT_FAILURE;
    }
    client.run();
    return EXIT_SUCCESS;
}
