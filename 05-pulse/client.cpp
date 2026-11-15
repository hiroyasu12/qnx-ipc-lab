#include "protocol.hpp"

#include <cstdio>
#include <iostream>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <unistd.h>

int main()
{
    std::cout << "[client] experiment=05-pulse PID=" << getpid()
              << " name=" << SERVICE_NAME << std::endl;
    const int coid = name_open(SERVICE_NAME, 0);
    if (coid == -1) {
        std::perror("name_open");
        return 1;
    }

    const AddRequest request{MSG_ADD, 0, 7, 5};
    AddReply reply{};
    std::cout << "[client] calling MsgSend: 7 + 5 coid=" << coid << std::endl;
    const int status = MsgSend(coid, &request, sizeof(request), &reply, sizeof(reply));
    int exit_code = 0;
    if (status == -1) {
        std::perror("MsgSend");
        exit_code = 1;
    } else {
        std::cout << "[client] MsgSend returned: status=" << status
                  << " result=" << reply.result << std::endl;
        if (status != 0 || reply.result != 12) {
            std::cerr << "[client] unexpected reply" << std::endl;
            exit_code = 1;
        }
    }
    if (name_close(coid) == -1) {
        std::perror("name_close");
        exit_code = 1;
    }
    return exit_code;
}
