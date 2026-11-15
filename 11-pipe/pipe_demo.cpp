#include "../common/line_stream.hpp"
#include <csignal>
#include <cstring>
#include <sys/wait.h>

int main(int argc, char* argv[])
{
    const bool small = argc == 2 && std::strcmp(argv[1], "small") == 0;
    const bool invalid = argc == 2 && std::strcmp(argv[1], "invalid") == 0;
    const bool long_line = argc == 2 && std::strcmp(argv[1], "long") == 0;
    const bool partial = argc == 2 && std::strcmp(argv[1], "partial") == 0;
    if (argc > 2 || (argc == 2 && !small && !invalid && !long_line && !partial)) {
        std::cerr << "usage: pipe_demo [small|invalid|long|partial]\n"; return 1;
    }
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) { std::perror("signal"); return 1; }
    int fds[2]{};
    if (pipe(fds) == -1) { std::perror("pipe (check pipe manager)"); return 1; }
    const pid_t child = fork();
    if (child == -1) {
        std::perror("fork"); line_stream::close_fd(fds[0]); line_stream::close_fd(fds[1]); return 1;
    }
    if (child == 0) {
        bool passed = line_stream::close_fd(fds[1]);
        if (passed) passed = line_stream::read_lines(fds[0], small ? 2U : 64U);
        if (!line_stream::close_fd(fds[0])) passed = false;
        _exit(passed ? 0 : 1);
    }
    bool passed = line_stream::close_fd(fds[0]);
    const std::string text = invalid ? "7 x\n" : long_line ? std::string(64, '7') + "\n" : partial ? "7 5" : "7 5\n";
    if (!line_stream::write_all(fds[1], text)) passed = false;
    if (!line_stream::close_fd(fds[1])) passed = false;
    int status{};
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited == -1 && errno == EINTR);
    if (waited == -1) { std::perror("waitpid"); passed = false; }
    else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) passed = false;
    std::cout << "[parent] child reaped: " << (passed ? "PASS" : "FAIL") << std::endl;
    return passed ? 0 : 1;
}
