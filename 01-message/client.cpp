#include "protocol.hpp"

#include <cstdio>
#include <iostream>

#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <unistd.h>

int main()
{
    // 名前からサーバーへ接続し、コネクションIDを得る。
    const int coid = name_open(SERVICE_NAME, 0);
    if (coid == -1) {
        std::perror("name_open");
        return 1;
    }

    const AddRequest request{MSG_ADD, 0, 7, 5};
    AddReply reply{};

    std::cout << "[client] PID=" << getpid()
              << " coid=" << coid << std::endl;

    std::cout << "[client] calling MsgSend: 7 + 5"
              << std::endl;

    // 要求を送信し、返信が来るまでこのスレッドは待つ。
    const int status = MsgSend(
        coid,
        &request, sizeof(request),
        &reply, sizeof(reply));

    int exitCode = 0;

    if (status == -1) {
        std::perror("MsgSend");
        exitCode = 1;
    } else {
        std::cout << "[client] MsgSend returned: status="
                  << status << " result=" << reply.result
                  << std::endl;
    }

    if (name_close(coid) == -1) {
        std::perror("name_close");
        exitCode = 1;
    }

    return exitCode;
}