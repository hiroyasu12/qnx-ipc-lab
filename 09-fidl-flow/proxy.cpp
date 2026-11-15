#include "proxy.hpp"
#include "protocol.hpp"
#include <cerrno>
#include <iostream>
#include <sys/neutrino.h>

bool CalculatorProxy::add(std::int32_t left, std::int32_t right,
                          std::int64_t& result) const
{
    const AddRequest request{ADD_METHOD, 0, left, right};
    AddReply reply{};
    std::cout << "[proxy] add -> MsgSend(" << left << ", " << right << ")" << std::endl;
    const auto status = MsgSend(coid_, &request, sizeof(request), &reply, sizeof(reply));
    if (status == -1) return false;
    if (status != 0) { errno = EPROTO; return false; }
    result = reply.result;
    std::cout << "[proxy] MsgSend returned result=" << result << std::endl;
    return true;
}
