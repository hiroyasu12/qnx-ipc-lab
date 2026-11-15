#pragma once
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

inline constexpr char FIFO_PATH[] = "/tmp/qnx-ipc-lab-add-12.fifo";
inline bool is_fifo()
{
    struct stat info{};
    if (lstat(FIFO_PATH, &info) == -1) { std::perror("lstat (run reader --prepare first)"); return false; }
    if (!S_ISFIFO(info.st_mode)) {
        std::cerr << "[fifo] existing path is not a FIFO; refusing\n"; return false;
    }
    return true;
}
inline int open_fifo(int flags)
{
    if (!is_fifo()) return -1;
    std::cout << "[fifo] before open " << FIFO_PATH << std::endl;
    int fd;
    do { fd = open(FIFO_PATH, flags | O_NOFOLLOW); } while (fd == -1 && errno == EINTR);
    if (fd == -1) { std::perror("open"); return -1; }
    struct stat info{};
    if (fstat(fd, &info) == -1 || !S_ISFIFO(info.st_mode)) {
        std::cerr << "[fifo] opened descriptor is not a FIFO\n"; close(fd); return -1;
    }
    std::cout << "[fifo] after open" << std::endl;
    return fd;
}
