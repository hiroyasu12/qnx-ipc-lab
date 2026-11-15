#include "fifo.hpp"
#include "../common/line_stream.hpp"
#include <csignal>

int main(int argc, char*[])
{
    if (argc != 1) { std::cerr << "usage: writer\n"; return 1; }
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) { std::perror("signal"); return 1; }
    const int fd = open_fifo(O_WRONLY);
    if (fd == -1) return 1;
    bool passed = line_stream::write_all(fd, "7 5\n");
    if (!line_stream::close_fd(fd)) passed = false;
    std::cout << "[writer] " << (passed ? "PASS" : "FAIL") << std::endl;
    return passed ? 0 : 1;
}
