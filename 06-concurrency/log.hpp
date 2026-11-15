#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <sys/process.h>
#include <unistd.h>

inline double monotonic_seconds()
{
    struct timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        std::perror("clock_gettime");
        std::exit(EXIT_FAILURE);
    }
    return static_cast<double>(now.tv_sec) + now.tv_nsec / 1.0e9;
}

// Hold the stream lock for one line only; never across receive/work/reply.
inline void log_line(const char* format, ...) __attribute__((format(printf, 1, 2)));

inline void log_line(const char* format, ...)
{
    flockfile(stdout);
    std::printf("[mono=%.3f PID=%ld TID=%d] ", monotonic_seconds(),
                static_cast<long>(getpid()), gettid());
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::putchar('\n');
    std::fflush(stdout);
    funlockfile(stdout);
}