#pragma once
#include <mutex>
#include <string>

namespace gc {

class Logger {
public:
    explicit Logger(std::string path);
    void append_line(const std::string& line);

private:
    std::string path_;
    std::mutex mutex_;
};

} // namespace gc
