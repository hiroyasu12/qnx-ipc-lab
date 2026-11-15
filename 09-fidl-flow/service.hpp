#pragma once
#include <cstdint>

inline std::int64_t add(std::int32_t left, std::int32_t right)
{
    return static_cast<std::int64_t>(left) + static_cast<std::int64_t>(right);
}
