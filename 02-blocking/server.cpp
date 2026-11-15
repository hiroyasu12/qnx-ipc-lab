#include "protocol.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <unistd.h>

int main()
{
    // 名前を登録し、受信チャネルを作る。
    name_attach_t* attach = name_attach(nullptr, SERVICE_NAME, 0);
    if (attach == nullptr) {
        std::perror("name_attach");
        return 1;
    }

    std::cout << "[server] PID=" << getpid()
              << " chid=" << attach->chid
              << " name=" << SERVICE_NAME << std::endl;

    int exitCode = 0;

    for (;;) {
        // 要求だけでなく、OSから届くパルスも入る大きさにする。
        std::array<char, 256> buffer{};
        struct _msg_info info{};

        std::cout << "[server] waiting in MsgReceive" << std::endl;

        const auto rcvid = MsgReceive(
            attach->chid, buffer.data(), buffer.size(), &info);

        if (rcvid == -1) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("MsgReceive");
            exitCode = 1;
            break;
        }

        // rcvid == 0ならパルス。通常メッセージのように返信しない。
        if (rcvid == 0) {
            struct _pulse pulse{};
            std::memcpy(&pulse, buffer.data(), sizeof(pulse));

            if (pulse.code == _PULSE_CODE_DISCONNECT) {
                if (ConnectDetach(pulse.scoid) == -1) {
                    std::perror("ConnectDetach");
                }
            }
            continue;
        }

        // 以下はrcvid > 0：返信可能な通常メッセージ。
        if (info.msglen < static_cast<decltype(info.msglen)>(sizeof(std::uint16_t))) {
            MsgError(rcvid, EINVAL);
            continue;
        }

        std::uint16_t type{};
        std::memcpy(&type, buffer.data(), sizeof(type));

        // 接続処理に伴うシステムメッセージへの応答。
        if (type == _IO_CONNECT) {
            MsgReply(rcvid, 0, nullptr, 0);
            continue;
        }

        if (type != MSG_ADD) {
            MsgError(rcvid, ENOSYS);
            continue;
        }

        if (info.srcmsglen != static_cast<decltype(info.srcmsglen)>(sizeof(AddRequest)) ||
            info.msglen < static_cast<decltype(info.msglen)>(sizeof(AddRequest))) {
            MsgError(rcvid, EINVAL);
            continue;
        }

        AddRequest request{};
        std::memcpy(&request, buffer.data(), sizeof(request));

        // int32同士の加算をint64で行う。
        const AddReply reply{
            static_cast<std::int64_t>(request.a) + request.b
        };

        std::cout << "[server] received: "
                  << request.a << " + " << request.b
                  << " rcvid=" << rcvid << std::endl;

        //2-A code block
        std::printf("[server] request accepted: rcvid=%d; sleep 20s before reply\n", rcvid);
        std::fflush(stdout);
        sleep(20);

        // この直後に、既存のMsgReply()を残す
        // この要求の送り手に返信する。
        if (MsgReply(rcvid, 0, &reply, sizeof(reply)) == -1) {
            std::perror("MsgReply");
        } else {
            std::cout << "[server] replied: "
                      << reply.result << std::endl;
            // 既存のMsgReply()と、そのエラー確認の後
            //2-B code block
            std::printf("[server] replied; sleep 20s before next receive\n");
            std::fflush(stdout);

            sleep(20);
        }
    }

    name_detach(attach, 0);
    return exitCode;
}