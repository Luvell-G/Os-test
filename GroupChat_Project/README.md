# GroupChat

C++ Operating Systems final project implementation for a **multi-group client-server chat system** with:

- fixed-size binary `ChatPacket` protocol
- TCP sockets
- per-group **LRU message cache** with TTL
- fixed **thread pool**
- pluggable **Round Robin** or **Shortest Job First** scheduling
- file-based logging
- simulated **virtual memory / paging** through a bounded global page pool
- optional-style **media packet** support using `MSG_MEDIA`

This matches the design doc’s server model, packet format, cache policy, and RR/SJF selection at startup. The design doc specifies a multi-threaded TCP server, 269-byte fixed `ChatPacket`, per-group LRU cache with TTL, and startup-selectable RR/SJF scheduling. fileciteturn1file1 fileciteturn1file9

## Project structure

```text
GroupChat/
├── client/
├── server/
├── shared/
├── tests/
├── logs/
├── CMakeLists.txt
└── README.md
```

The project spec also recommends this same overall layout: client/server/shared/tests/logs/CMake. fileciteturn2file6 fileciteturn2file12

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run server

```bash
./build/chat_server --port 8080 --sched rr --workers 4
```

or

```bash
./build/chat_server --port 8080 --sched sjf --workers 4
```

The design doc calls for a fixed worker pool of `N=4` and startup selection with `--sched rr` or `--sched sjf`. fileciteturn1file9 fileciteturn1file10

## Run client

```bash
./build/chat_client --host 127.0.0.1 --port 8080
```

Commands inside the client:

```text
/join <groupID> [token]
/leave <groupID>
/switch <groupID>
/list
/media <simulated-bytes>
/quit
```

Anything else is sent as text to the current group.

## Demo flow

Terminal 1:

```bash
./build/chat_server --port 8080 --sched rr
```

Terminal 2:

```bash
./build/chat_client --host 127.0.0.1 --port 8080
/join 1
hello group 1
```

Terminal 3:

```bash
./build/chat_client --host 127.0.0.1 --port 8080
/join 1
```

The second client should get:
- join acknowledgement
- recent message history replay from cache
- future broadcasts for that group

That behavior follows the project requirement that a new user joining a group must receive recent message history from cache. fileciteturn1file1 fileciteturn2file11

## What is implemented

### Required chat features
- join or create groups
- broadcast text packets to all members of a group
- timestamps, sender IDs, and group IDs in each packet
- active group listing
- switch groups by joining another group
- recent-history replay on join

These are all listed in the project description and design doc. fileciteturn1file2 fileciteturn2file4

### Cache + TTL
Each group has its own `LRUMessageCache`:
- capacity 50 messages
- TTL 3600 seconds
- lazy expiration
- MRU/LRU ordering with `std::list + std::unordered_map`

That matches the design doc’s cache section. fileciteturn1file9

### Scheduling
- `RR`: FIFO queue
- `SJF`: priority queue ordered by `payloadSize`
- simple aging in SJF to reduce starvation

That matches the design doc’s task queue and scheduling comparison. fileciteturn1file9 fileciteturn1file10

### Synchronization
- `std::mutex` for global client/task structures
- `std::shared_mutex` per group
- lock ordering kept consistent by taking group lock before cache operations

That follows the design doc’s synchronization choice and resource-ordering deadlock prevention approach. fileciteturn1file9

### Memory-management simulation
A small `MemoryPager` tracks cached messages as simulated pages in a fixed-size global page pool:
- cache touch = page access
- first touch / eviction pressure = simulated page fault
- global LRU page replacement

This is the project’s required virtual-memory simulation idea, but kept separated from the networking code so the architecture stays understandable. The project spec explicitly asks for a fixed memory pool with LRU-based page-fault handling. fileciteturn2file6 fileciteturn2file10

### File system + logging
`logs/chat_log.txt` records:
- joins / leaves
- text messages
- media sends
- shutdown metrics

The spec requires file-based logs and cached-message persistence using file I/O. fileciteturn1file2 fileciteturn1file10

## Notes on optional features
Included in a manageable way:
- `MSG_MEDIA` packet handling for simulated audio/video chunks
- token-based join payload for optional verification
- XOR checksum added on the wire around the fixed 269-byte packet

The design doc marks media simulation, token verification, and checksum support as optional extras, so they are implemented lightly instead of turning this into a full multimedia streaming system. fileciteturn1file10

## Known limitations
- Uses `select()` instead of `poll/epoll`
- The server uses one event loop + one worker pool, rather than a full production reactor architecture
- Message history is replayed as original packets, which is simple and presentation-friendly
- Media is simulated as raw payload bytes/string data, which fits the assignment’s placeholder-media guidance

That last point matches the project text telling students to prefer placeholder `std::vector`-style media simulation before real OpenCV/FFmpeg streaming. fileciteturn1file2

## Suggested presentation talking points
1. **Networking:** TCP socket client/server model.
2. **Scheduling:** RR vs. SJF worker dispatch.
3. **Synchronization:** `mutex`, `shared_mutex`, condition variable in thread pool.
4. **Caching:** per-group LRU + TTL for message history.
5. **Virtual memory simulation:** fixed page pool with LRU page replacement.
6. **Deadlock prevention:** resource ordering instead of banker’s algorithm.
