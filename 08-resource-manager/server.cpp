#include "protocol.hpp"

// iofunc.h defines the default resource-manager attribute and OCB types.
#include <sys/iofunc.h>
#include <sys/dispatch.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {
constexpr std::size_t RECEIVE_BUFFER_SIZE = 2048U;
// Business logic has no knowledge of messages, file descriptors, or dispatch.
std::int64_t add(const std::int32_t left, const std::int32_t right)
{
    return static_cast<std::int64_t>(left) + static_cast<std::int64_t>(right);
}

// Stub role: validate/decode the request, call business logic, encode the reply.
int handle_devctl(resmgr_context_t* const context, io_devctl_t* const message,
                  iofunc_ocb_t* const open_context)
{
    const int default_status = iofunc_devctl_default(context, message, open_context);
    if (default_status != _RESMGR_DEFAULT) {
        return default_status;
    }
    if (message->i.dcmd != DCMD_LAB_ADD) {
        return ENOTTY;
    }
    if (message->i.nbytes != sizeof(AddData)) {
        return EINVAL;
    }

    AddData data{};
    // Read the actual payload, including bytes not in the initial receive buffer.
    // resmgr_msgread accounts for the resource-manager context's message offset.
    const auto bytes_read = resmgr_msgread(context, &data, sizeof(data), sizeof(message->i));
    if (bytes_read == -1) {
        return errno;
    }
    if (bytes_read != static_cast<ssize_t>(sizeof(data))) {
        return EINVAL;
    }
    data.result = add(data.left, data.right);
    std::printf("handler=devctl client_pid=%ld left=%d right=%d result=%lld\n",
                static_cast<long>(context->info.pid), data.left, data.right,
                static_cast<long long>(data.result));

    // The dispatch context owns this buffer until the framework sends the reply.
    // Do not return an IOV pointing at the stack-local 'data'.
    std::memcpy(_IO_OUTPUT_PAYLOAD(message), &data, sizeof(data));
    std::memset(&message->o, 0, sizeof(message->o));
    message->o.nbytes = static_cast<decltype(message->o.nbytes)>(sizeof(data));
    return _RESMGR_PTR(context, &message->o, sizeof(message->o) + sizeof(data));
}

bool serve(dispatch_context_t* context)
{
    while (true) {
        dispatch_context_t* const received_context = dispatch_block(context);
        if (received_context == nullptr) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("dispatch_block");
            return false;
        }
        context = received_context;
        if (dispatch_handler(context) == -1) {
            std::perror("dispatch_handler");
            return false;
        }
    }
}
}

int main()
{
    if (std::setvbuf(stdout, nullptr, _IONBF, 0U) != 0) {
        std::fprintf(stderr, "setvbuf failed\n");
        return EXIT_FAILURE;
    }
    dispatch_t* const dispatch = dispatch_create();
    if (dispatch == nullptr) {
        std::perror("dispatch_create");
        return EXIT_FAILURE;
    }

    resmgr_connect_funcs_t connect_functions{};
    resmgr_io_funcs_t io_functions{};
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_functions,
                     _RESMGR_IO_NFUNCS, &io_functions);
    io_functions.devctl = handle_devctl;

    iofunc_attr_t device_attributes{};
    iofunc_attr_init(&device_attributes, S_IFCHR | 0600, nullptr, nullptr);
    resmgr_attr_t manager_attributes{};
    manager_attributes.nparts_max = 1U;
    manager_attributes.msg_max_size = RECEIVE_BUFFER_SIZE;

    const int attachment_id = resmgr_attach(dispatch, &manager_attributes, DEVICE_PATH,
                                             _FTYPE_ANY, 0U, &connect_functions,
                                             &io_functions, &device_attributes);
    bool success = (attachment_id != -1);
    if (!success) {
        std::perror("resmgr_attach");
    } else {
        dispatch_context_t* const context = dispatch_context_alloc(dispatch);
        if (context == nullptr) {
            std::perror("dispatch_context_alloc");
            success = false;
        } else {
            std::printf("experiment=08-resource-manager build=%s %s PID=%ld path=%s threads=1\n",
                        __DATE__, __TIME__, static_cast<long>(getpid()), DEVICE_PATH);
            success = serve(context);
            dispatch_context_free(context);
        }
        if (resmgr_detach(dispatch, attachment_id, _RESMGR_DETACH_ALL) == -1) {
            std::perror("resmgr_detach");
            success = false;
        }
    }
    if (dispatch_destroy(dispatch) == -1) {
        std::perror("dispatch_destroy");
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
