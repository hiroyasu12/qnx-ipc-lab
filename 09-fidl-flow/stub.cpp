#include "stub.hpp"
#include "protocol.hpp"
#include "service.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/neutrino.h>

bool dispatch_add(rcvid_t rcvid, const void* data, std::size_t received, std::size_t sent)
{
    int error = 0;
    std::uint16_t method{};
    if (received < sizeof(method)) error = EINVAL;
    else {
        std::memcpy(&method, data, sizeof(method));
        if (method != ADD_METHOD) error = ENOSYS;
        else if (sent != sizeof(AddRequest) || received < sizeof(AddRequest)) error = EINVAL;
    }
    if (error != 0) {
        std::cout << "[stub] rejected method=" << method << " error=" << error << std::endl;
        if (MsgError(rcvid, error) == -1) std::perror("MsgError");
        return false;
    }
    AddRequest request{};
    std::memcpy(&request, data, sizeof(request));
    if (request.reserved != 0) {
        if (MsgError(rcvid, EINVAL) == -1) std::perror("MsgError");
        return false;
    }
    std::cout << "[stub] dispatch add(" << request.left << ", " << request.right << ")" << std::endl;
    const AddReply reply{add(request.left, request.right)};
    std::cout << "[service] add result=" << reply.result << std::endl;
    std::cout << "[stub] MsgReply result=" << reply.result << std::endl;
    if (MsgReply(rcvid, 0, &reply, sizeof(reply)) == -1) {
        std::perror("MsgReply");
        return false;
    }
    return true;
}
