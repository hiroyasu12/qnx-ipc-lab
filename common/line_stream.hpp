#pragma once
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unistd.h>

namespace line_stream {
inline bool write_all(int fd, const std::string& text)
{
    std::size_t offset = 0;
    while (offset < text.size()) {
        const ssize_t count = write(fd, text.data() + offset, text.size() - offset);
        if (count == -1 && errno == EINTR) continue;
        if (count <= 0) { std::perror("write"); return false; }
        offset += static_cast<std::size_t>(count);
        std::cout << "[stream] write bytes=" << count << std::endl;
    }
    return true;
}

inline bool calculate(const std::string& line)
{
    std::istringstream input(line);
    std::int64_t left{}, right{};
    if (!(input >> left >> right) || !(input >> std::ws).eof() ||
        left < std::numeric_limits<std::int32_t>::min() ||
        left > std::numeric_limits<std::int32_t>::max() ||
        right < std::numeric_limits<std::int32_t>::min() ||
        right > std::numeric_limits<std::int32_t>::max()) {
        std::cerr << "[stream] rejected invalid line\n"; return false;
    }
    std::cout << "[stream] line complete result=" << left + right << std::endl;
    return true;
}

inline bool read_lines(int fd, std::size_t chunk)
{
    std::array<char, 64> buffer{};
    if (chunk == 0 || chunk > buffer.size()) return false;
    std::string line;
    bool saw_line = false;
    for (;;) {
        const ssize_t count = read(fd, buffer.data(), chunk);
        if (count == -1 && errno == EINTR) continue;
        if (count == -1) { std::perror("read"); return false; }
        if (count == 0) {
            std::cout << "[stream] read=0 EOF" << std::endl;
            if (!line.empty()) std::cerr << "[stream] rejected unterminated line\n";
            return saw_line && line.empty();
        }
        std::cout << "[stream] read bytes=" << count << std::endl;
        for (ssize_t i = 0; i < count; ++i) {
            const char ch = buffer[static_cast<std::size_t>(i)];
            if (ch == '\n') {
                if (!calculate(line)) return false;
                saw_line = true;
                line.clear();
            } else {
                if (line.size() == 63) {
                    std::cerr << "[stream] rejected line longer than 63 bytes\n"; return false;
                }
                line.push_back(ch);
            }
        }
    }
}
inline bool close_fd(int fd)
{
    if (close(fd) == -1) { std::perror("close"); return false; }
    return true;
}
} // namespace line_stream
