#ifndef QNX_IPC_LAB_07_SHARED_HPP
#define QNX_IPC_LAB_07_SHARED_HPP

#include "protocol.hpp"

// pthread functions return an error number directly; they do not set errno.
[[nodiscard]] bool check_pthread(int error, const char* operation);
[[nodiscard]] bool parse_count(const char* text, std::uint32_t maximum,
                               std::uint32_t& value);
[[nodiscard]] bool initialize_shared_data(SharedData& data);

// Owns this process's mapping. Only create() acquires ownership of the name.
// Mutex lifetime is managed by the server, separately from the mapping.
class SharedMemory final {
public:
    SharedMemory() = default;
    ~SharedMemory();

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;

    [[nodiscard]] bool create();
    [[nodiscard]] bool attach();
    [[nodiscard]] bool close();

    // Precondition: create() or attach() succeeded; close() has not been called.
    SharedData& data();

private:
    enum class OpenMode { Create, Attach };
    [[nodiscard]] bool open(OpenMode mode);
    [[nodiscard]] bool map_descriptor(int descriptor);
    [[nodiscard]] bool unmap();

    SharedData* data_ = nullptr;
    bool owns_name_ = false;
};

#endif // QNX_IPC_LAB_07_SHARED_HPP
