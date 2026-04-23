cmake_minimum_required(VERSION 3.16)
project(GroupChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -pthread)

include_directories(${CMAKE_SOURCE_DIR})

add_library(groupchat_shared
    shared/protocol.cpp
    shared/logger.cpp
    shared/metrics.cpp
    server/cache.cpp
    server/memory_pager.cpp
    server/group_manager.cpp
    server/thread_pool.cpp
    server/chat_server.cpp
    client/chat_client.cpp
)

target_include_directories(groupchat_shared PUBLIC
    ${CMAKE_SOURCE_DIR}
)

add_executable(chat_server server/main.cpp)
target_link_libraries(chat_server PRIVATE groupchat_shared)

add_executable(chat_client client/main.cpp)
target_link_libraries(chat_client PRIVATE groupchat_shared)

add_executable(bot_test tests/bot_test.cpp)
target_link_libraries(bot_test PRIVATE groupchat_shared)
