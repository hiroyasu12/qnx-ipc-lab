#include "protocol.hpp"
#include "log.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s label\n", argv[0]);
        return 1;
    }
    const char* label = argv[1];
    log_line("experiment=06-concurrency build=%s %s client=%s name_open BEGIN",
             __DATE__, __TIME__, label);
    const int coid = name_open(SERVICE_NAME, 0);
    if (coid == -1) {
        std::perror("name_open");
        return 1;
    }
    log_line("client=%s READY coid=%d; after BOTH clients are READY, press Enter",
             label, coid);
    const int input = std::getchar();
    if (input != '\n') {
        std::fprintf(stderr, "Expected Enter on this client's terminal\n");
        name_close(coid);
        return 1;
    }

    const AddRequest request{MSG_ADD, 0, 7, 5};
    AddReply reply{};
    log_line("client=%s MsgSend BEGIN add=7+5", label);
    const double start = monotonic_seconds();
    const int status = MsgSend(coid, &request, sizeof(request), &reply, sizeof(reply));
    const int saved_errno = errno;
    const double elapsed = monotonic_seconds() - start;
    int exit_code = 0;
    if (status == -1) {
        log_line("client=%s MsgSend ERROR errno=%d (%s) elapsed=%.3fs",
                 label, saved_errno, std::strerror(saved_errno), elapsed);
        exit_code = 1;
    } else {
        log_line("client=%s MsgSend END status=%d result=%lld elapsed=%.3fs",
                 label, status, static_cast<long long>(reply.result), elapsed);
        if (status != 0 || reply.result != 12) {
            std::fprintf(stderr, "Unexpected reply\n");
            exit_code = 1;
        }
    }
    if (name_close(coid) == -1) {
        std::perror("name_close");
        exit_code = 1;
    }
    return exit_code;
}