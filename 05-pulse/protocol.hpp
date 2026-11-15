#pragma once

#include <cstdint>
#include <sys/iomsg.h>

inline constexpr char SERVICE_NAME[] = "qnx-ipc-lab-pulse";
inline constexpr std::uint16_t MSG_ADD = _IO_MAX + 1;

struct AddRequest {
    std::uint16_t type;
    std::uint16_t reserved;
    std::int32_t a;
    std::int32_t b;
};

struct AddReply {
    std::int64_t result;
};
