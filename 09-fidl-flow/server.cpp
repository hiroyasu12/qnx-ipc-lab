#include "protocol.hpp"
#include "stub.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

int main(int argc, char* argv[])
{
    const bool once = argc == 2 && std::strcmp(argv[1], "once") == 0;
    if (argc > 2 || (argc == 2 && !once)) {
        std::cerr << "usage: server [once]\n"; return 1;
    }
    name_attach_t* attach = name_attach(nullptr, SERVICE_NAME, 0);
    if (attach == nullptr) { std::perror("name_attach"); return 1; }
    int result = 0;
    std::cout << "[server] name=" << SERVICE_NAME << std::endl;
    for (;;) {
        std::array<char, 256> buffer{};
        struct _msg_info info{};
        const rcvid_t rcvid = MsgReceive(attach->chid, buffer.data(), buffer.size(), &info);
        if (rcvid == -1) {
            if (errno == EINTR) continue;
            std::perror("MsgReceive"); result = 1; break;
        }
        if (rcvid == 0) {
            struct _pulse pulse{};
            std::memcpy(&pulse, buffer.data(), sizeof(pulse));
            if (pulse.code == _PULSE_CODE_DISCONNECT && ConnectDetach(pulse.scoid) == -1)
                std::perror("ConnectDetach");
            continue;
        }
        std::uint16_t type{};
        if (info.msglen >= static_cast<decltype(info.msglen)>(sizeof(type))) {
            std::memcpy(&type, buffer.data(), sizeof(type));
            if (type == _IO_CONNECT) {
                if (MsgReply(rcvid, 0, nullptr, 0) == -1) std::perror("MsgReply connect");
                continue;
            }
        }
        std::cout << "[server] MsgReceive application request" << std::endl;
        dispatch_add(rcvid, buffer.data(), static_cast<std::size_t>(info.msglen),
                     static_cast<std::size_t>(info.srcmsglen));
        if (once) break;
    }
    if (name_detach(attach, 0) == -1) { std::perror("name_detach"); result = 1; }
    return result;
}
