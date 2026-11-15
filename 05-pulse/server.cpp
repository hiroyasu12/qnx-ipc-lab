#include "protocol.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

namespace {
constexpr int PULSE_TIMER = _PULSE_CODE_MINAVAIL;
constexpr int TIMER_VALUE = 42;
constexpr int INTERVAL_SECONDS = 2;

void reply_error(rcvid_t rcvid, int error)
{
    if (MsgError(rcvid, error) == -1) {
        std::perror("MsgError");
    }
}
}

int main()
{
    name_attach_t* attach = name_attach(nullptr, SERVICE_NAME, 0);
    if (attach == nullptr) {
        std::perror("name_attach");
        return 1;
    }

    // 自分の受信チャネルへ、タイマー通知用の接続を作る。
    // side channelにして、標準入出力などのFD番号との衝突を避ける。
    const int timer_coid = ConnectAttach(
        0, 0, attach->chid, _NTO_SIDE_CHANNEL, 0);
    if (timer_coid == -1) {
        std::perror("ConnectAttach");
        name_detach(attach, 0);
        return 1;
    }

    struct sigevent event{};
    SIGEV_PULSE_INT_INIT(&event, timer_coid, SIGEV_PULSE_PRIO_INHERIT,
                         PULSE_TIMER, TIMER_VALUE);
    timer_t timer_id{};
    if (timer_create(CLOCK_MONOTONIC, &event, &timer_id) == -1) {
        std::perror("timer_create");
        ConnectDetach(timer_coid);
        name_detach(attach, 0);
        return 1;
    }

    struct itimerspec period{};
    period.it_value.tv_sec = INTERVAL_SECONDS;    // 最初の満了まで
    period.it_interval.tv_sec = INTERVAL_SECONDS; // 以後の周期
    if (timer_settime(timer_id, 0, &period, nullptr) == -1) {
        std::perror("timer_settime");
        timer_delete(timer_id);
        ConnectDetach(timer_coid);
        name_detach(attach, 0);
        return 1;
    }

    std::cout << "[server] experiment=05-pulse PID=" << getpid()
              << " chid=" << attach->chid << " name=" << SERVICE_NAME
              << " interval=" << INTERVAL_SECONDS << "s"
              << " timer_code=" << PULSE_TIMER
              << " timer_value=" << TIMER_VALUE << std::endl;

    unsigned long received_pulses = 0;
    int exit_code = 0;
    for (;;) {
        std::array<char, 256> buffer{};
        struct _msg_info info{};
        const auto rcvid = MsgReceive(
            attach->chid, buffer.data(), buffer.size(), &info);
        if (rcvid == -1) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("MsgReceive");
            exit_code = 1;
            break;
        }

        if (rcvid == 0) {
            struct _pulse pulse{};
            std::memcpy(&pulse, buffer.data(), sizeof(pulse));
            if (pulse.code == PULSE_TIMER) {
                ++received_pulses;
                std::cout << "[server] timer pulse: rcvid=0 code="
                          << static_cast<int>(pulse.code)
                          << " value=" << pulse.value.sival_int
                          << " received=" << received_pulses
                          << " (no reply)" << std::endl;
            } else if (pulse.code == _PULSE_CODE_DISCONNECT) {
                std::cout << "[server] disconnect pulse: rcvid=0"
                          << " (no reply)" << std::endl;
                if (ConnectDetach(pulse.scoid) == -1) {
                    std::perror("ConnectDetach disconnect");
                }
            } else if (pulse.code == _PULSE_CODE_UNBLOCK) {
                // 通知自体には返信せず、まだ解除待ちの元要求だけを完了する。
                const auto pending = static_cast<rcvid_t>(pulse.value.sival_int);
                struct _msg_info pending_info{};
                if (MsgInfo(pending, &pending_info) != -1 &&
                    (pending_info.flags & _NTO_MI_UNBLOCK_REQ)) {
                    reply_error(pending, -1);
                }
            } else {
                std::cout << "[server] other pulse: rcvid=0 code="
                          << static_cast<int>(pulse.code)
                          << " (no reply)" << std::endl;
            }
            continue; // パルスにはMsgReplyしない。
        }

        // rcvid > 0：通常メッセージ。要求を処理し、返信で完了させる。
        if (info.msglen < static_cast<decltype(info.msglen)>(sizeof(std::uint16_t))) {
            reply_error(rcvid, EINVAL);
            continue;
        }
        std::uint16_t type{};
        std::memcpy(&type, buffer.data(), sizeof(type));
        if (type == _IO_CONNECT) {
            if (MsgReply(rcvid, 0, nullptr, 0) == -1) {
                std::perror("MsgReply connect");
            }
            continue;
        }
        if (type != MSG_ADD) {
            reply_error(rcvid, ENOSYS);
            continue;
        }
        if (info.srcmsglen != static_cast<decltype(info.srcmsglen)>(sizeof(AddRequest)) ||
            info.msglen < static_cast<decltype(info.msglen)>(sizeof(AddRequest))) {
            reply_error(rcvid, EINVAL);
            continue;
        }
        AddRequest request{};
        std::memcpy(&request, buffer.data(), sizeof(request));
        const AddReply reply{static_cast<std::int64_t>(request.a) + request.b};
        std::cout << "[server] add request: rcvid=" << rcvid
                  << " " << request.a << " + " << request.b << std::endl;
        if (MsgReply(rcvid, 0, &reply, sizeof(reply)) == -1) {
            std::perror("MsgReply add");
        } else {
            std::cout << "[server] replied: result=" << reply.result << std::endl;
        }
    }

    // 通知元を止めてから接続・チャネルを解放する。
    if (timer_delete(timer_id) == -1) {
        std::perror("timer_delete");
        exit_code = 1;
    }
    if (ConnectDetach(timer_coid) == -1) {
        std::perror("ConnectDetach timer");
        exit_code = 1;
    }
    if (name_detach(attach, 0) == -1) {
        std::perror("name_detach");
        exit_code = 1;
    }
    return exit_code;
}
