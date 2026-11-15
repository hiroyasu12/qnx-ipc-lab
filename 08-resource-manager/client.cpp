#include "protocol.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace {
// Proxy role: convert an application call into a device request and its reply.
// devctl returns an error number directly; it does not use the -1/errno contract.
int add_remote(const int descriptor, const std::int32_t left, const std::int32_t right,
                std::int64_t& result)
{
    AddData data{left, right, 0};
    const int status = devctl(descriptor, DCMD_LAB_ADD, &data, sizeof(data), nullptr);
    if (status == EOK) {
        result = data.result;
    }
    return status;
}

bool run_request(const int descriptor, const char* const mode)
{
    int status = EOK;
    int expected_status = EOK;
    std::int64_t result = 0;
    if (std::strcmp(mode, "add") == 0) {
        status = add_remote(descriptor, 7, 5, result);
    } else {
        // Deliberately bypass the proxy to check server-side input validation.
        AddData data{7, 5, 0};
        if (std::strcmp(mode, "unknown") == 0) {
            expected_status = ENOTTY;
            status = devctl(descriptor, DCMD_LAB_UNKNOWN, &data, sizeof(data), nullptr);
        } else {
            expected_status = EINVAL;
            status = devctl(descriptor, DCMD_LAB_ADD, &data, sizeof(data.left), nullptr);
        }
    }
    const bool success = (status == expected_status) &&
                          ((expected_status != EOK) || (result == 12));
    std::printf("mode=%s status=%d (%s) result=%lld expected_status=%d %s\n",
                mode, status, std::strerror(status), static_cast<long long>(result),
                expected_status, success ? "PASS" : "FAIL");
    return success;
}
}

int main(int argc, char* argv[])
{
    const char* const mode = (argc == 2) ? argv[1] : "add";
    if ((argc > 2) || ((std::strcmp(mode, "add") != 0) &&
                       (std::strcmp(mode, "unknown") != 0) &&
                       (std::strcmp(mode, "short") != 0))) {
        std::fprintf(stderr, "Usage: %s [add|unknown|short]\n", argv[0]);
        return EXIT_FAILURE;
    }
    std::printf("experiment=08-resource-manager build=%s %s PID=%ld path=%s\n",
                __DATE__, __TIME__, static_cast<long>(getpid()), DEVICE_PATH);
    const int descriptor = open(DEVICE_PATH, O_RDWR);
    if (descriptor == -1) {
        std::perror("open");
        return EXIT_FAILURE;
    }
    bool success = run_request(descriptor, mode);
    if (close(descriptor) == -1) {
        std::perror("close");
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
