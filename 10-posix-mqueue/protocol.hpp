#pragma once
#include <cstdint>
#include <mqueue.h>

inline constexpr char QUEUE_NAME[] = "/qnx-ipc-lab-add-10";
struct AddRequest { std::int32_t left; std::int32_t right; };
inline bool valid_queue(const mq_attr& attr)
{
    return attr.mq_maxmsg == 2 && attr.mq_msgsize == static_cast<long>(sizeof(AddRequest));
}
