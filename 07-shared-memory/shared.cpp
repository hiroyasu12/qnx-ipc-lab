#include "shared.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr mode_t SHARED_MEMORY_PERMISSIONS = S_IRUSR | S_IWUSR;

bool close_descriptor(const int descriptor)
{
    const bool success = (::close(descriptor) == 0);
    if (!success) {
        std::perror("close shared memory descriptor");
    }
    return success;
}
}

bool check_pthread(const int error, const char* const operation)
{
    const bool success = (error == 0);
    if (!success) {
        std::fprintf(stderr, "%s: %s\n", operation, std::strerror(error));
    }
    return success;
}

bool parse_count(const char* const text, const std::uint32_t maximum,
                 std::uint32_t& value)
{
    // Check the bound before arithmetic; reject signs, spaces and overflow.
    std::uint32_t parsed = 0U;
    bool valid = (text != nullptr);
    if (valid) {
        valid = (text[0] != '\0');
        const char* character = text;
        while (valid && (*character != '\0')) {
            if ((*character < '0') || (*character > '9')) {
                valid = false;
            } else {
                const auto digit = static_cast<std::uint32_t>(*character - '0');
                if ((digit > maximum) || (parsed > ((maximum - digit) / 10U))) {
                    valid = false;
                } else {
                    parsed = (parsed * 10U) + digit;
                    ++character;
                }
            }
        }
    }
    valid = valid && (parsed > 0U);
    if (valid) {
        value = parsed;
    }
    return valid;
}

bool initialize_shared_data(SharedData& data)
{
    pthread_mutexattr_t attributes{};
    bool success = check_pthread(pthread_mutexattr_init(&attributes),
                                 "pthread_mutexattr_init");
    if (success) {
        success = check_pthread(
            pthread_mutexattr_setpshared(&attributes, PTHREAD_PROCESS_SHARED),
            "pthread_mutexattr_setpshared");
        if (success) {
            success = check_pthread(pthread_mutex_init(&data.mutex, &attributes),
                                     "pthread_mutex_init");
        }
        if (!check_pthread(pthread_mutexattr_destroy(&attributes),
                           "pthread_mutexattr_destroy")) {
            success = false;
        }
    }
    if (success) {
        data.updates = 0U;
        data.total = 0U;
    }
    return success;
}

SharedMemory::~SharedMemory()
{
    // Explicit close() reports failures to the caller; this is the fallback.
    const bool closed = close();
    static_cast<void>(closed);
}

bool SharedMemory::create()
{
    return open(OpenMode::Create);
}

bool SharedMemory::attach()
{
    return open(OpenMode::Attach);
}

SharedData& SharedMemory::data()
{
    return *data_;
}

bool SharedMemory::open(const OpenMode mode)
{
    if ((data_ != nullptr) || owns_name_) {
        std::fprintf(stderr, "Shared memory is already open\n");
        return false;
    }
    int flags = O_RDWR;
    if (mode == OpenMode::Create) {
        flags |= O_CREAT | O_EXCL;
    }
    const int descriptor = shm_open(SHM_NAME, flags, SHARED_MEMORY_PERMISSIONS);
    if (descriptor == -1) {
        std::perror("shm_open");
        return false;
    }
    owns_name_ = (mode == OpenMode::Create);
    bool success = true;
    if (owns_name_) {
        success = (ftruncate(descriptor, static_cast<off_t>(sizeof(SharedData))) == 0);
        if (!success) {
            std::perror("ftruncate");
        }
    }
    if (success) {
        success = map_descriptor(descriptor);
    }
    // A successful mapping outlives its file descriptor.
    if (!close_descriptor(descriptor)) {
        success = false;
    }
    return success;
}

bool SharedMemory::map_descriptor(const int descriptor)
{
    struct stat information{};
    if (fstat(descriptor, &information) == -1) {
        std::perror("fstat");
        return false;
    }
    if (information.st_size != static_cast<off_t>(sizeof(SharedData))) {
        std::fprintf(stderr, "Shared memory size mismatch\n");
        return false;
    }
    void* const address = mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE,
                               MAP_SHARED, descriptor, 0);
    if (address == MAP_FAILED) {
        std::perror("mmap");
        return false;
    }
    // OS boundary: mmap returns storage aligned for SharedData and its mutex.
    data_ = static_cast<SharedData*>(address);
    return true;
}

bool SharedMemory::unmap()
{
    bool success = true;
    if (data_ != nullptr) {
        success = (munmap(data_, sizeof(SharedData)) == 0);
        if (success) {
            data_ = nullptr;
        } else {
            std::perror("munmap");
        }
    }
    return success;
}

bool SharedMemory::close()
{
    bool success = unmap();
    if (owns_name_) {
        if (shm_unlink(SHM_NAME) == 0) {
            owns_name_ = false;
        } else {
            std::perror("shm_unlink");
            success = false;
        }
    }
    return success;
}
