#include "protocol.hpp"
#include "log.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

namespace {
constexpr int WORK_SECONDS = 10;

struct Context {
    int chid;
};

void reply_error(rcvid_t rcvid, int error)
{
    if (MsgError(rcvid, error) == -1) {
        std::perror("MsgError");
    }
}

void* receive_loop(void* argument)
{
    const int chid = static_cast<const Context*>(argument)->chid;
    log_line("receiver ready chid=%d", chid);
    for (;;) {
        // Every receiver owns its buffer, info, rcvid, request and reply.
        std::array<char, 256> buffer{};
        struct _msg_info info{};
        const auto rcvid = MsgReceive(chid, buffer.data(), buffer.size(), &info);
        if (rcvid == -1) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("MsgReceive");
            std::exit(EXIT_FAILURE);
        }
        if (rcvid == 0) {
            struct _pulse pulse{};
            std::memcpy(&pulse, buffer.data(), sizeof(pulse));
            if (pulse.code == _PULSE_CODE_DISCONNECT) {
                if (ConnectDetach(pulse.scoid) == -1) {
                    std::perror("ConnectDetach");
                }
            } else if (pulse.code == _PULSE_CODE_UNBLOCK) {
                // The receiver owning this request will reply after its work.
                // Another receiver must not complete the same rcvid here.
                log_line("unblock request: completion deferred to owning receiver");
            }
            continue;
        }

        if (info.msglen < static_cast<decltype(info.msglen)>(sizeof(std::uint16_t))) {
            reply_error(rcvid, EINVAL);
            continue;
        }
        std::uint16_t type{};
        std::memcpy(&type, buffer.data(), sizeof(type));
        if (type == _IO_CONNECT) {
            if (MsgReply(rcvid, 0, nullptr, 0) == -1) {
                std::perror("MsgReply connect");
            }
            continue;
        }
        if (type != MSG_ADD) {
            reply_error(rcvid, ENOSYS);
            continue;
        }
        if (info.srcmsglen != static_cast<decltype(info.srcmsglen)>(sizeof(AddRequest)) ||
            info.msglen < static_cast<decltype(info.msglen)>(sizeof(AddRequest))) {
            reply_error(rcvid, EINVAL);
            continue;
        }
        AddRequest request{};
        std::memcpy(&request, buffer.data(), sizeof(request));
        log_line("BEGIN client_pid=%ld rcvid=%lld add=%d+%d work=%ds",
                 static_cast<long>(info.pid), static_cast<long long>(rcvid), request.a, request.b, WORK_SECONDS);

        // Simulate a slow operation. No application lock is held while waiting.
        struct timespec remaining{WORK_SECONDS, 0};
        while (nanosleep(&remaining, &remaining) == -1) {
            if (errno != EINTR) {
                std::perror("nanosleep");
                std::exit(EXIT_FAILURE);
            }
        }

        const AddReply reply{static_cast<std::int64_t>(request.a) + request.b};
        // This thread is the only application code that completes this request.
        if (MsgReply(rcvid, 0, &reply, sizeof(reply)) == -1) {
            std::perror("MsgReply add");
        } else {
            log_line("END client_pid=%ld rcvid=%lld result=%lld",
                     static_cast<long>(info.pid), static_cast<long long>(rcvid),
                     static_cast<long long>(reply.result));
        }
    }
}
}

int main(int argc, char* argv[])
{
    if (argc != 2 || (std::strcmp(argv[1], "1") != 0 &&
                      std::strcmp(argv[1], "2") != 0)) {
        std::fprintf(stderr, "Usage: %s 1|2\n", argv[0]);
        return 1;
    }
    const int receivers = argv[1][0] - '0';
    name_attach_t* attach = name_attach(nullptr, SERVICE_NAME, 0);
    if (attach == nullptr) {
        std::perror("name_attach");
        return 1;
    }
    Context context{attach->chid};
    log_line("experiment=06-concurrency build=%s %s receivers=%d name=%s work=%ds",
             __DATE__, __TIME__, receivers, SERVICE_NAME, WORK_SECONDS);
    if (receivers == 2) {
        pthread_t worker{};
        // The main thread also receives, so only one extra thread is needed.
        const int error = pthread_create(&worker, nullptr, receive_loop,
                                        &context);
        if (error != 0) {
            std::fprintf(stderr, "pthread_create: %s\n", std::strerror(error));
            name_detach(attach, 0);
            return 1;
        }
    }
    receive_loop(&context);
    // Ctrl+C terminates the process; the OS reclaims its channel and threads.
}