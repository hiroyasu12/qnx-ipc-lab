#include "protocol.hpp"
#include "proxy.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

int main(int argc, char* argv[])
{
    const bool unknown = argc == 2 && std::strcmp(argv[1], "unknown") == 0;
    if (argc > 2 || (argc == 2 && !unknown && std::strcmp(argv[1], "add") != 0)) {
        std::cerr << "usage: client [add|unknown]\n"; return 1;
    }
    const int coid = name_open(SERVICE_NAME, 0);
    if (coid == -1) { std::perror("name_open"); return 1; }
    bool passed = false;
    if (unknown) {
        const AddRequest request{UNKNOWN_METHOD, 0, 7, 5};
        AddReply reply{};
        const auto status = MsgSend(coid, &request, sizeof(request), &reply, sizeof(reply));
        const int error = errno;
        passed = status == -1 && error == ENOSYS;
        std::cout << "[client] unknown rejected=" << passed << " errno=" << error << std::endl;
    } else {
        std::int64_t result{};
        std::cout << "[client] call proxy.add(7, 5)" << std::endl;
        passed = CalculatorProxy(coid).add(7, 5, result) && result == 12;
        if (!passed) std::perror("proxy.add");
        std::cout << "[client] result=" << result << std::endl;
    }
    if (name_close(coid) == -1) { std::perror("name_close"); passed = false; }
    std::cout << (passed ? "PASS" : "FAIL") << std::endl;
    return passed ? 0 : 1;
}
