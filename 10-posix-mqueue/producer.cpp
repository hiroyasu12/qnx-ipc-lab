#include "protocol.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fcntl.h>

int main(int argc, char* argv[])
{
    const char* mode = argc == 1 ? "send" : argv[1];
    if (argc > 2 || (std::strcmp(mode, "send") != 0 && std::strcmp(mode, "full") != 0 &&
        std::strcmp(mode, "retry") != 0 && std::strcmp(mode, "cleanup") != 0)) {
        std::cerr << "usage: producer [send|full|retry|cleanup]\n"; return 1;
    }
    if (std::strcmp(mode, "cleanup") == 0) {
        if (mq_unlink(QUEUE_NAME) == -1 && errno != ENOENT) {
            std::perror("mq_unlink"); return 1;
        }
        std::cout << "[producer] cleanup done\n"; return 0;
    }
    const bool retry = std::strcmp(mode, "retry") == 0;
    const bool full = std::strcmp(mode, "full") == 0;
    mq_attr attr{};
    attr.mq_maxmsg = 2;
    attr.mq_msgsize = static_cast<long>(sizeof(AddRequest));
    const int flags = O_WRONLY | O_NONBLOCK | (retry ? 0 : O_CREAT | O_EXCL);
    const mqd_t queue = mq_open(QUEUE_NAME, flags, 0600, &attr);
    if (queue == static_cast<mqd_t>(-1)) { std::perror("mq_open (start mqueue; cleanup stale queue)"); return 1; }
    bool passed = true;
    if (mq_getattr(queue, &attr) == -1 || !valid_queue(attr)) {
        std::cerr << "[producer] unexpected queue attributes\n"; passed = false;
    }
    const AddRequest request{7, 5};
    for (int i = 1; passed && i <= (full ? 3 : 1); ++i) {
        int status;
        do { status = mq_send(queue, reinterpret_cast<const char*>(&request), sizeof(request), 0); }
        while (status == -1 && errno == EINTR);
        if (full && i == 3) {
            passed = status == -1 && errno == EAGAIN;
            std::cout << "[producer] third send EAGAIN=" << passed << std::endl;
        } else if (status == -1) {
            std::perror("mq_send"); passed = false;
        } else {
            std::cout << "[producer] mq_send accepted #" << i
                      << "; calculation not confirmed" << std::endl;
        }
    }
    if (mq_close(queue) == -1) { std::perror("mq_close"); passed = false; }
    // Keep even failed experiments available for inspection; explicit cleanup is provided.
    std::cout << (passed ? "PASS" : "FAIL") << std::endl;
    return passed ? 0 : 1;
}
