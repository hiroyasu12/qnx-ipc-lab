#include "protocol.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fcntl.h>

int main(int argc, char* argv[])
{
    const bool keep = argc == 2 && std::strcmp(argv[1], "one-keep") == 0;
    const bool two = argc == 2 && std::strcmp(argv[1], "two") == 0;
    if (argc > 2 || (argc == 2 && !keep && !two)) {
        std::cerr << "usage: consumer [one-keep|two]\n"; return 1;
    }
    const mqd_t queue = mq_open(QUEUE_NAME, O_RDONLY | O_NONBLOCK);
    if (queue == static_cast<mqd_t>(-1)) { std::perror("mq_open"); return 1; }
    mq_attr attr{};
    bool passed = mq_getattr(queue, &attr) == 0 && valid_queue(attr);
    if (!passed) std::cerr << "[consumer] unexpected queue attributes\n";
    // Validate mq_msgsize before using this fixed, sufficiently large buffer.
    std::array<char, sizeof(AddRequest)> buffer{};
    for (int i = 0; passed && i < (two ? 2 : 1); ++i) {
        ssize_t received;
        do { received = mq_receive(queue, buffer.data(), buffer.size(), nullptr); }
        while (received == -1 && errno == EINTR);
        if (received != static_cast<ssize_t>(sizeof(AddRequest))) {
            if (received == -1) std::perror("mq_receive");
            else std::cerr << "[consumer] rejected message length=" << received << '\n';
            passed = false; break;
        }
        AddRequest request{};
        std::memcpy(&request, buffer.data(), sizeof(request));
        const std::int64_t result = static_cast<std::int64_t>(request.left) + request.right;
        std::cout << "[consumer] mq_receive bytes=" << received << " result=" << result << std::endl;
        passed = result == 12;
    }
    if (mq_close(queue) == -1) { std::perror("mq_close"); passed = false; }
    if (!keep && mq_unlink(QUEUE_NAME) == -1) { std::perror("mq_unlink"); passed = false; }
    std::cout << (passed ? "PASS" : "FAIL") << (keep ? " (queue retained)" : "") << std::endl;
    return passed ? 0 : 1;
}
