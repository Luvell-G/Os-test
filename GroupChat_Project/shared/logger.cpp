#include "shared/logger.h"

#include <fstream>

namespace gc {

Logger::Logger(std::string path) : path_(std::move(path)) {}

void Logger::append_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(path_, std::ios::app);
    out << line << '\n';
}

} // namespace gc
