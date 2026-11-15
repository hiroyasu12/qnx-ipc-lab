#ifndef QNX_IPC_LAB_08_PROTOCOL_HPP
#define QNX_IPC_LAB_08_PROTOCOL_HPP

#include <cstdint>
#include <devctl.h>
#include <type_traits>

inline constexpr char DEVICE_PATH[] = "/dev/qnx-ipc-lab-add";

// Local QNX x86_64 ABI shared by the client and resource manager.
struct AddData {
    std::int32_t left;
    std::int32_t right;
    std::int64_t result;
};

static_assert(std::is_trivially_copyable_v<AddData>);
static_assert(sizeof(AddData) == 16U);
inline constexpr int DCMD_LAB_ADD = __DIOTF(_DCMD_MISC, 1, AddData);
inline constexpr int DCMD_LAB_UNKNOWN = __DIOTF(_DCMD_MISC, 2, AddData);

#endif // QNX_IPC_LAB_08_PROTOCOL_HPP
