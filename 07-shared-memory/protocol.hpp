#ifndef QNX_IPC_LAB_07_PROTOCOL_HPP
#define QNX_IPC_LAB_07_PROTOCOL_HPP

#include <cstdint>
#include <pthread.h>
#include <sys/iomsg.h>
#include <sys/neutrino.h>

inline constexpr char SERVICE_NAME[] = "qnx-ipc-lab-shared";
inline constexpr char SHM_NAME[] = "/qnx-ipc-lab-shared";
inline constexpr std::uint16_t MSG_JOIN = static_cast<std::uint16_t>(_IO_MAX + 1);
inline constexpr int PULSE_DONE = _PULSE_CODE_MINAVAIL;
inline constexpr std::uint64_t ADD_RESULT = 12U; // 7 + 5 per update
inline constexpr std::uint32_t MAX_CLIENTS = 2U;
inline constexpr std::uint32_t MAX_ITERATIONS = 10000000U;

struct JoinRequest {
    std::uint16_t type;
    std::uint16_t reserved;
};

struct JoinReply {
    std::uint32_t iterations;
};

// Map the same object with MAP_SHARED in every process. Never copy this mutex.
// No process-local pointers belong in this structure.
struct SharedData {
    pthread_mutex_t mutex;
    std::uint64_t updates;
    std::uint64_t total;
};

#endif // QNX_IPC_LAB_07_PROTOCOL_HPP
