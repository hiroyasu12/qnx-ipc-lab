#include "protocol.hpp"
#include "shared.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/dispatch.h>
#include <unistd.h>

namespace {
constexpr int SERVER_ARGUMENT_COUNT = 3;
constexpr std::size_t RECEIVE_BUFFER_SIZE = 256U;
using ReceiveBuffer = std::array<char, RECEIVE_BUFFER_SIZE>;
static_assert(RECEIVE_BUFFER_SIZE >= sizeof(struct _pulse));
static_assert(RECEIVE_BUFFER_SIZE >= sizeof(JoinRequest));

struct Configuration {
    std::uint32_t clients = 0U;
    std::uint32_t iterations = 0U;
};

struct Snapshot {
    std::uint64_t updates = 0U;
    std::uint64_t total = 0U;
};

struct Progress {
    std::uint32_t joined = 0U;
    std::uint32_t completed = 0U;
    Snapshot latest{};
};

bool parse_configuration(const int argument_count, char* const arguments[],
                          Configuration& configuration)
{
    bool valid = (argument_count == SERVER_ARGUMENT_COUNT);
    if (valid) {
        valid = parse_count(arguments[1], MAX_CLIENTS, configuration.clients);
    }
    if (valid) {
        valid = parse_count(arguments[2], MAX_ITERATIONS, configuration.iterations);
    }
    if (!valid) {
        std::fprintf(stderr, "Usage: %s clients(1|2) iterations(1..10000000)\n",
                     arguments[0]);
    }
    return valid;
}

bool reply_error(const rcvid_t receive_id, const int error)
{
    const bool success = (MsgError(receive_id, error) == 0);
    if (!success) {
        std::perror("MsgError");
    }
    return success;
}

bool handle_join(const rcvid_t receive_id, const struct _msg_info& information,
                  const Configuration& configuration, Progress& progress)
{
    if ((information.srcmsglen != static_cast<decltype(information.srcmsglen)>(sizeof(JoinRequest))) ||
        (information.msglen < static_cast<decltype(information.msglen)>(sizeof(JoinRequest)))) {
        return reply_error(receive_id, EINVAL);
    }
    if (progress.joined >= configuration.clients) {
        return reply_error(receive_id, EBUSY);
    }
    const JoinReply reply{configuration.iterations};
    const bool success = (MsgReply(receive_id, 0, &reply, sizeof(reply)) == 0);
    if (success) {
        ++progress.joined;
        std::printf("JOIN client_pid=%ld participants=%u/%u\n",
                    static_cast<long>(information.pid), progress.joined, configuration.clients);
    } else {
        std::perror("MsgReply join");
    }
    return success;
}

bool handle_request(const rcvid_t receive_id, const ReceiveBuffer& buffer,
                     const struct _msg_info& information,
                     const Configuration& configuration, Progress& progress)
{
    std::uint16_t type = 0U;
    if (information.msglen < static_cast<decltype(information.msglen)>(sizeof(type))) {
        return reply_error(receive_id, EINVAL);
    }
    // Copy bytes into an aligned object; do not cast the receive buffer.
    std::memcpy(&type, buffer.data(), sizeof(type));
    bool success = false;
    if (type == _IO_CONNECT) {
        success = (MsgReply(receive_id, 0, nullptr, 0U) == 0);
        if (!success) {
            std::perror("MsgReply connect");
        }
    } else if (type == MSG_JOIN) {
        success = handle_join(receive_id, information, configuration, progress);
    } else {
        success = reply_error(receive_id, ENOSYS);
    }
    return success;
}

bool read_snapshot(SharedData& data, Snapshot& snapshot)
{
    bool success = check_pthread(pthread_mutex_lock(&data.mutex), "pthread_mutex_lock");
    if (success) {
        snapshot.updates = data.updates;
        snapshot.total = data.total;
        success = check_pthread(pthread_mutex_unlock(&data.mutex), "pthread_mutex_unlock");
    }
    return success;
}

bool handle_pulse(const ReceiveBuffer& buffer, SharedData& data,
                   const Configuration& configuration, Progress& progress)
{
    struct _pulse pulse{};
    std::memcpy(&pulse, buffer.data(), sizeof(pulse));
    bool success = true;
    if (pulse.code == _PULSE_CODE_DISCONNECT) {
        success = (ConnectDetach(pulse.scoid) == 0);
        if (!success) {
            std::perror("ConnectDetach");
        }
    } else if (pulse.code == PULSE_DONE) {
        // The client has unlocked and unmapped before sending this notification.
        success = read_snapshot(data, progress.latest);
        if (success) {
            ++progress.completed;
            std::printf("DONE client_pid=%d notifications=%u/%u snapshot updates=%llu total=%llu\n",
                        pulse.value.sival_int, progress.completed, configuration.clients,
                        static_cast<unsigned long long>(progress.latest.updates),
                        static_cast<unsigned long long>(progress.latest.total));
        }
    } else {
        // Other system pulses require no action in this finite experiment.
    }
    // A pulse never receives MsgReply.
    return success;
}

bool receive_notifications(const int channel_id, SharedData& data,
                            const Configuration& configuration, Progress& progress)
{
    bool success = true;
    while (success && (progress.completed < configuration.clients)) {
        ReceiveBuffer buffer{};
        struct _msg_info information{};
        const rcvid_t receive_id = MsgReceive(channel_id, buffer.data(), buffer.size(), &information);
        if (receive_id == -1) {
            const int receive_error = errno;
            if (receive_error != EINTR) {
                std::perror("MsgReceive");
                success = false;
            }
        } else if (receive_id == 0) {
            success = handle_pulse(buffer, data, configuration, progress);
        } else {
            success = handle_request(receive_id, buffer, information, configuration, progress);
        }
    }
    return success;
}

bool verify_final_snapshot(const Configuration& configuration, const Snapshot& snapshot)
{
    const std::uint64_t expected_updates =
        static_cast<std::uint64_t>(configuration.clients) * configuration.iterations;
    const std::uint64_t expected_total = expected_updates * ADD_RESULT;
    const bool success = (snapshot.updates == expected_updates) &&
                          (snapshot.total == expected_total);
    std::printf("FINAL expected_updates=%llu expected_total=%llu %s\n",
                static_cast<unsigned long long>(expected_updates),
                static_cast<unsigned long long>(expected_total), success ? "PASS" : "FAIL");
    return success;
}

bool run_server(SharedData& data, const Configuration& configuration)
{
    // Publish the service only after shared-memory initialization.
    name_attach_t* const attachment = name_attach(nullptr, SERVICE_NAME, 0);
    if (attachment == nullptr) {
        std::perror("name_attach");
        const bool destroyed = check_pthread(pthread_mutex_destroy(&data.mutex),
                                              "pthread_mutex_destroy");
        static_cast<void>(destroyed);
        return false;
    }
    std::printf("experiment=07-shared-memory build=%s %s PID=%ld clients=%u iterations=%u shm=%s\n",
                __DATE__, __TIME__, static_cast<long>(getpid()), configuration.clients,
                configuration.iterations, SHM_NAME);

    Progress progress{};
    bool success = receive_notifications(attachment->chid, data, configuration, progress);
    if (success) {
        success = verify_final_snapshot(configuration, progress.latest);
    }
    if (name_detach(attachment, 0) == -1) {
        std::perror("name_detach");
        success = false;
    }
    // On failure a client may still be using the mutex. Destroy only after all DONEs.
    if (progress.completed == configuration.clients) {
        if (!check_pthread(pthread_mutex_destroy(&data.mutex), "pthread_mutex_destroy")) {
            success = false;
        }
    }
    return success;
}
}

int main(int argc, char* argv[])
{
    Configuration configuration{};
    if (!parse_configuration(argc, argv, configuration)) {
        return EXIT_FAILURE;
    }
    if (std::setvbuf(stdout, nullptr, _IONBF, 0U) != 0) {
        std::fprintf(stderr, "setvbuf failed\n");
        return EXIT_FAILURE;
    }
    SharedMemory memory;
    bool success = memory.create();
    if (success) {
        success = initialize_shared_data(memory.data());
    }
    if (success) {
        success = run_server(memory.data(), configuration);
    }
    if (!memory.close()) {
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
