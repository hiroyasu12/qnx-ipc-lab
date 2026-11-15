#include "protocol.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

int main()
{
    const int coid = name_open(SERVICE_NAME, 0);
    if (coid == -1) {
        std::perror("name_open");
        return EXIT_FAILURE;
    }

    std::cout << "[client] PID=" << getpid()
              << " coid=" << coid << std::endl;

    const AddRequest request{MSG_ADD, 0, 7, 5};
    AddReply reply{};
    int exitCode = EXIT_SUCCESS;

    for (int attempt = 1; attempt <= 2; ++attempt) {
        // 前回の結果を残さない。送信失敗時には結果を使わない。
        reply = {};

        std::cout << "[client] attempt=" << attempt
                  << " calling MsgSend: 7 + 5"
                  << " timeout=3s" << std::endl;

        std::uint64_t timeout_ns = 3ULL * 1000 * 1000 * 1000;

        if (TimerTimeout(
                CLOCK_MONOTONIC,
                _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY,
                nullptr,
                &timeout_ns,
                nullptr) == -1) {
            std::perror("TimerTimeout");
            exitCode = EXIT_FAILURE;
            break;
        }

        // TimerTimeout成功後、別のAPI呼び出しを挟まない。
        const int status = MsgSend(
            coid,
            &request, sizeof(request),
            &reply, sizeof(reply));

        if (status == -1) {
            const int saved_errno = errno;

            std::fprintf(
                stderr,
                "[client] attempt=%d status=%d errno=%d (%s)\n",
                attempt, status, saved_errno,
                std::strerror(saved_errno));

            exitCode = EXIT_FAILURE;
            break;
        }

        std::cout << "[client] attempt=" << attempt
                  << " status=" << status
                  << " result=" << reply.result
                  << std::endl;
    }

    if (name_close(coid) == -1) {
        std::perror("name_close");
        exitCode = EXIT_FAILURE;
    }

    return exitCode;
}