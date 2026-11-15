#include "protocol.hpp"
#include "shared.hpp"

#include <cstdio>
#include <cstdlib>
#include <sys/dispatch.h>
#include <unistd.h>

namespace {
constexpr int CLIENT_ARGUMENT_COUNT = 2;
constexpr int INHERIT_PULSE_PRIORITY = -1;

bool join_server(const int connection_id, JoinReply& reply)
{
    const JoinRequest request{MSG_JOIN, 0U};
    const auto status = MsgSend(connection_id, &request, sizeof(request),
                               &reply, sizeof(reply));
    bool success = false;
    if (status == -1) {
        std::perror("MsgSend join");
    } else if ((status != 0) || (reply.iterations == 0U) ||
               (reply.iterations > MAX_ITERATIONS)) {
        std::fprintf(stderr, "Invalid join reply\n");
    } else {
        success = true;
    }
    return success;
}

bool wait_for_start(const char* const label, const std::uint32_t iterations)
{
    std::printf("client=%s READY iterations=%u; after ALL clients are READY, press Enter\n",
                label, iterations);
    const bool success = (std::getchar() == '\n');
    if (!success) {
        std::fprintf(stderr, "Expected Enter on this client's terminal\n");
    }
    return success;
}

bool update_shared_data(SharedData& data, const std::uint32_t iterations)
{
    bool success = true;
    for (std::uint32_t index = 0U; (index < iterations) && success; ++index) {
        success = check_pthread(pthread_mutex_lock(&data.mutex), "pthread_mutex_lock");
        if (success) {
            // Treat both fields as one update, protected by the same mutex.
            ++data.updates;
            data.total += ADD_RESULT;
            success = check_pthread(pthread_mutex_unlock(&data.mutex),
                                     "pthread_mutex_unlock");
        }
    }
    return success;
}

bool notify_completion(const int connection_id)
{
    const int process_id = static_cast<int>(getpid());
    const bool success = (MsgSendPulse(connection_id, INHERIT_PULSE_PRIORITY,
                                       PULSE_DONE, process_id) == 0);
    if (!success) {
        std::perror("MsgSendPulse DONE");
    }
    return success;
}

bool run_client(const int connection_id, const char* const label)
{
    JoinReply reply{};
    SharedMemory memory;

    bool success = join_server(connection_id, reply);
    if (success) {
        success = memory.attach();
    }
    if (success) {
        success = wait_for_start(label, reply.iterations);
    }
    if (success) {
        success = update_shared_data(memory.data(), reply.iterations);
    }
    // No shared-memory access after DONE; the server may now clean up.
    if (!memory.close()) {
        success = false;
    }
    if (success) {
        success = notify_completion(connection_id);
    }
    if (success) {
        std::printf("client=%s DONE queued updates=%u (no reply awaited)\n",
                    label, reply.iterations);
    }
    return success;
}
}

int main(int argc, char* argv[])
{
    if (argc != CLIENT_ARGUMENT_COUNT) {
        std::fprintf(stderr, "Usage: %s label\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (std::setvbuf(stdout, nullptr, _IONBF, 0U) != 0) {
        std::fprintf(stderr, "setvbuf failed\n");
        return EXIT_FAILURE;
    }
    std::printf("experiment=07-shared-memory build=%s %s PID=%ld client=%s\n",
                __DATE__, __TIME__, static_cast<long>(getpid()), argv[1]);
    const int connection_id = name_open(SERVICE_NAME, 0);
    if (connection_id == -1) {
        std::perror("name_open");
        return EXIT_FAILURE;
    }
    bool success = run_client(connection_id, argv[1]);
    if (name_close(connection_id) == -1) {
        std::perror("name_close");
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
