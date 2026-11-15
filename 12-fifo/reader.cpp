#include "fifo.hpp"
#include "../common/line_stream.hpp"
#include <cstring>

int main(int argc, char* argv[])
{
    const bool prepare = argc == 2 && std::strcmp(argv[1], "--prepare") == 0;
    if (argc > 2 || (argc == 2 && !prepare)) {
        std::cerr << "usage: reader [--prepare]\n"; return 1;
    }
    bool created = false;
    struct stat owned{};
    if (mkfifo(FIFO_PATH, 0600) == 0) created = true;
    else if (errno != EEXIST) { std::perror("mkfifo"); return 1; }
    if (!is_fifo() || lstat(FIFO_PATH, &owned) == -1) return 1;
    if (prepare) {
        std::cout << "[reader] FIFO prepared; retained for writer-first experiment\n";
        return 0;
    }
    const int fd = open_fifo(O_RDONLY);
    bool passed = fd != -1;
    if (passed) {
        std::cout << "[reader] waiting in read" << std::endl;
        passed = line_stream::read_lines(fd, 64);
        if (!line_stream::close_fd(fd)) passed = false;
    }
    // Only remove the same FIFO created by this invocation. Pre-existing paths remain.
    if (created) {
        struct stat current{};
        if (lstat(FIFO_PATH, &current) == -1 || current.st_dev != owned.st_dev ||
            current.st_ino != owned.st_ino || !S_ISFIFO(current.st_mode)) {
            std::cerr << "[reader] path changed; refusing unlink\n"; passed = false;
        } else if (unlink(FIFO_PATH) == -1) { std::perror("unlink"); passed = false; }
    }
    std::cout << "[reader] " << (passed ? "PASS" : "FAIL")
              << (created ? "" : " (existing FIFO retained)") << std::endl;
    return passed ? 0 : 1;
}
