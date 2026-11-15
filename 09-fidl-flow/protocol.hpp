#pragma once
#include <cstdint>
#include <sys/iomsg.h>

// Handwritten local ABI, not a CommonAPI binding or portable wire format.
inline constexpr char SERVICE_NAME[] = "qnx-ipc-lab-fidl";
inline constexpr std::uint16_t ADD_METHOD = _IO_MAX + 1;
inline constexpr std::uint16_t UNKNOWN_METHOD = _IO_MAX + 2;
struct AddRequest {
    std::uint16_t method;
    std::uint16_t reserved;
    std::int32_t left;
    std::int32_t right;
};
struct AddReply { std::int64_t result; };
