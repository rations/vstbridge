// vstbridge: a Wine VST/CLAP plugin bridge
// Copyright (C) 2020-2024 Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// The native side of the PE plugin host. See `calls.h`. These functions run on
// the PE thread that made the `__wine_unix_call()`, so per-thread operations
// like `sched_setscheduler(0, ...)` apply to that thread.

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <xcb/xcb.h>

#include "calls.h"
#include "xcb-calls.h"

using namespace unixlib;

// `NTSTATUS` as seen from Wine's Unix side. We always return success and report
// errors through the argument structs instead.
using NTSTATUS = int32_t;
using unixlib_entry_t = NTSTATUS (*)(void*);

namespace {

bool fill_sockaddr(sockaddr_un& addr, const char* path) {
    addr = sockaddr_un{};
    addr.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(addr.sun_path)) {
        return false;
    }
    strcpy(addr.sun_path, path);

    return true;
}

NTSTATUS do_socket_connect(void* args) {
    auto& a = *static_cast<PathFdArgs*>(args);
    a.fd = -1;
    a.error = 0;

    sockaddr_un addr;
    if (!fill_sockaddr(addr, a.path)) {
        a.error = ENAMETOOLONG;
        return 0;
    }

    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        a.error = errno;
        return 0;
    }
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        a.error = errno;
        close(fd);
        return 0;
    }

    a.fd = fd;
    return 0;
}

NTSTATUS do_socket_listen(void* args) {
    auto& a = *static_cast<PathFdArgs*>(args);
    a.fd = -1;
    a.error = 0;

    sockaddr_un addr;
    if (!fill_sockaddr(addr, a.path)) {
        a.error = ENAMETOOLONG;
        return 0;
    }

    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        a.error = errno;
        return 0;
    }
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(fd, SOMAXCONN) != 0) {
        a.error = errno;
        close(fd);
        return 0;
    }

    a.fd = fd;
    return 0;
}

NTSTATUS do_socket_accept(void* args) {
    auto& a = *static_cast<AcceptArgs*>(args);
    a.error = 0;

    int fd;
    do {
        fd = accept4(a.listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
    } while (fd == -1 && (errno == EINTR || errno == ECONNABORTED));

    a.fd = fd;
    if (fd == -1) {
        a.error = errno;
    }

    return 0;
}

NTSTATUS do_socket_read(void* args) {
    auto& a = *static_cast<ReadWriteArgs*>(args);
    a.error = 0;

    ssize_t result;
    do {
        result = read(a.fd, a.buffer, a.size);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        a.error = errno;
        a.result = 0;
    } else {
        a.result = static_cast<uint64_t>(result);
    }

    return 0;
}

NTSTATUS do_socket_write(void* args) {
    auto& a = *static_cast<ReadWriteArgs*>(args);
    a.error = 0;

    // `MSG_NOSIGNAL` so a closed connection results in `EPIPE` instead of a
    // `SIGPIPE`, like Asio does on Linux
    ssize_t result;
    do {
        result = send(a.fd, a.buffer, a.size, MSG_NOSIGNAL);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        a.error = errno;
        a.result = 0;
    } else {
        a.result = static_cast<uint64_t>(result);
    }

    return 0;
}

NTSTATUS do_socket_shutdown(void* args) {
    auto& a = *static_cast<FdArgs*>(args);
    // This also wakes up threads blocked in `read()` or `accept()` on this
    // socket
    a.error = shutdown(a.fd, SHUT_RDWR) == 0 ? 0 : errno;

    return 0;
}

NTSTATUS do_fd_close(void* args) {
    auto& a = *static_cast<FdArgs*>(args);
    a.error = close(a.fd) == 0 ? 0 : errno;

    return 0;
}

NTSTATUS do_path_unlink(void* args) {
    auto& a = *static_cast<PathArgs*>(args);
    a.error = unlink(a.path) == 0 ? 0 : errno;

    return 0;
}

NTSTATUS do_path_create_directories(void* args) {
    auto& a = *static_cast<PathArgs*>(args);

    std::error_code err;
    std::filesystem::create_directories(a.path, err);
    a.error = err.value();

    return 0;
}

NTSTATUS do_path_remove_all(void* args) {
    auto& a = *static_cast<PathArgs*>(args);

    std::error_code err;
    std::filesystem::remove_all(a.path, err);
    a.error = err.value();

    return 0;
}

// The same check `create_acceptor_if_inactive()` in `group.cpp` does in the
// Winelib host
NTSTATUS do_path_socket_listening(void* args) {
    auto& a = *static_cast<SocketListeningArgs*>(args);
    a.listening = 0;

    const std::string endpoint_path(a.path);
    std::ifstream open_sockets("/proc/net/unix");
    for (std::string line; std::getline(open_sockets, line);) {
        if (line.size() >= endpoint_path.size() &&
            line.compare(line.size() - endpoint_path.size(),
                         endpoint_path.size(), endpoint_path) == 0) {
            a.listening = 1;
            break;
        }
    }

    return 0;
}

NTSTATUS do_shm_open_fd(void* args) {
    auto& a = *static_cast<PathFdArgs*>(args);

    a.fd = shm_open(a.path, O_RDWR | O_CREAT, 0600);
    a.error = a.fd == -1 ? errno : 0;

    return 0;
}

// The same as `AudioShmBuffer::setup_mapping()` in the Linux build
NTSTATUS do_shm_map(void* args) {
    auto& a = *static_cast<ShmMapArgs*>(args);
    a.error = 0;
    a.unlocked = 0;
    a.address = a.old_address;

    // `ftruncate()` with a size of 0 fails on shared memory objects
    if (a.size == 0) {
        return 0;
    }

    if (ftruncate(a.fd, static_cast<off_t>(a.size)) != 0) {
        a.error = errno;
        return 0;
    }

    void* address =
        a.old_address
            ? mremap(a.old_address, a.old_size, a.size, MREMAP_MAYMOVE)
            : mmap(nullptr, a.size, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_LOCKED, a.fd, 0);
    if (address == MAP_FAILED) {
        // The memory locking limit was probably reached, so we'll try again
        // without locking the memory. The PE side prints a warning.
        a.unlocked = 1;
        if (a.old_address) {
            munmap(a.old_address, a.old_size);
        }

        address = mmap(nullptr, a.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                       a.fd, 0);
        if (address == MAP_FAILED) {
            a.error = errno;
            a.address = nullptr;
            return 0;
        }
    }

    a.address = address;
    return 0;
}

NTSTATUS do_shm_destroy(void* args) {
    auto& a = *static_cast<ShmDestroyArgs*>(args);

    if (a.address) {
        munmap(a.address, a.size);
    }
    close(a.fd);
    shm_unlink(a.name);

    return 0;
}

NTSTATUS do_process_id(void* args) {
    auto& a = *static_cast<ProcessArgs*>(args);
    a.pid = getpid();

    return 0;
}

// The same check as `pid_running()` in `common/process.cpp`: zombies don't have
// a valid `/proc/<pid>/exe` link, and `EACCES` still means that the process is
// running
NTSTATUS do_process_running(void* args) {
    auto& a = *static_cast<ProcessArgs*>(args);

    std::error_code err;
    std::filesystem::canonical("/proc/" + std::to_string(a.pid) + "/exe", err);
    a.running = !err || err.value() == EACCES;

    return 0;
}

NTSTATUS do_thread_set_name(void* args) {
    auto& a = *static_cast<ThreadNameArgs*>(args);

    // Linux thread names are limited to 15 characters plus the terminator
    char name[16]{};
    strncpy(name, a.name, sizeof(name) - 1);
    pthread_setname_np(pthread_self(), name);

    return 0;
}

NTSTATUS do_sched_get_realtime_priority(void* args) {
    auto& a = *static_cast<RealtimePriorityArgs*>(args);

    sched_param params{};
    if (sched_getparam(0, &params) == 0 && params.sched_priority > 0) {
        a.sched_fifo = 1;
        a.priority = params.sched_priority;
    } else {
        a.sched_fifo = 0;
        a.priority = 0;
    }

    return 0;
}

NTSTATUS do_sched_set_realtime_priority(void* args) {
    auto& a = *static_cast<RealtimePriorityArgs*>(args);

    sched_param params{};
    params.sched_priority = a.sched_fifo ? a.priority : 0;
    a.success = sched_setscheduler(0, a.sched_fifo ? SCHED_FIFO : SCHED_OTHER,
                                   &params) == 0;

    return 0;
}

// Sizes of libxcb's allocations, see `read_packet()` in libxcb's `xcb_in.c`.
// Replies are 32 bytes plus `length` 4-byte units. Events and errors get an
// extra `full_sequence` field, and XGE events carry `length` units of data
// after that.
uint64_t reply_size(const void* reply) {
    return 32 + 4ull * static_cast<const xcb_generic_reply_t*>(reply)->length;
}

uint64_t event_size(const xcb_generic_event_t* event) {
    if ((event->response_type & 0x7f) == XCB_GE_GENERIC) {
        return sizeof(xcb_generic_event_t) +
               4ull * reinterpret_cast<const xcb_ge_generic_event_t*>(event)
                          ->length;
    } else {
        return sizeof(xcb_generic_event_t);
    }
}

void set_reply(XcbArgs& a, void* reply, xcb_generic_error_t* error) {
    a.packet = reply;
    a.packet_size = reply ? reply_size(reply) : 0;
    a.error = error;
    a.error_size = error ? sizeof(xcb_generic_error_t) : 0;
}

NTSTATUS do_xcb_call(void* args) {
    auto& a = *static_cast<XcbArgs*>(args);
    const uint64_t* v = a.args;
    auto* conn = reinterpret_cast<xcb_connection_t*>(v[0]);
    // Reply functions take an `xcb_generic_error_t**` as their last argument.
    // `v[2]` is non-zero if the caller passed one.
    xcb_generic_error_t* error = nullptr;
    xcb_generic_error_t** error_ptr = nullptr;

    a.result = 0;
    a.packet = nullptr;
    a.packet_size = 0;
    a.error = nullptr;
    a.error_size = 0;

    switch (a.op) {
        case xcb_op_connect:
            a.result = reinterpret_cast<uint64_t>(
                xcb_connect(reinterpret_cast<const char*>(v[0]),
                            reinterpret_cast<int*>(v[1])));
            break;
        case xcb_op_disconnect:
            xcb_disconnect(conn);
            break;
        case xcb_op_flush:
            a.result = static_cast<uint32_t>(xcb_flush(conn));
            break;
        case xcb_op_generate_id:
            a.result = xcb_generate_id(conn);
            break;
        case xcb_op_get_setup:
            a.result = reinterpret_cast<uint64_t>(xcb_get_setup(conn));
            break;
        case xcb_op_setup_roots_iterator:
            *reinterpret_cast<xcb_screen_iterator_t*>(v[1]) =
                xcb_setup_roots_iterator(
                    reinterpret_cast<const xcb_setup_t*>(v[0]));
            break;
        case xcb_op_screen_next:
            xcb_screen_next(reinterpret_cast<xcb_screen_iterator_t*>(v[0]));
            break;
        case xcb_op_poll_for_event: {
            xcb_generic_event_t* event = xcb_poll_for_event(conn);
            a.packet = event;
            a.packet_size = event ? event_size(event) : 0;
        } break;
        case xcb_op_request_check:
            error = xcb_request_check(
                conn, xcb_void_cookie_t{static_cast<unsigned int>(v[1])});
            set_reply(a, nullptr, error);
            break;

        case xcb_op_change_property:
            a.result =
                xcb_change_property(conn, static_cast<uint8_t>(v[1]),
                                    static_cast<xcb_window_t>(v[2]),
                                    static_cast<xcb_atom_t>(v[3]),
                                    static_cast<xcb_atom_t>(v[4]),
                                    static_cast<uint8_t>(v[5]),
                                    static_cast<uint32_t>(v[6]),
                                    reinterpret_cast<const void*>(v[7]))
                    .sequence;
            break;
        case xcb_op_change_window_attributes:
            a.result = xcb_change_window_attributes(
                           conn, static_cast<xcb_window_t>(v[1]),
                           static_cast<uint32_t>(v[2]),
                           reinterpret_cast<const void*>(v[3]))
                           .sequence;
            break;
        case xcb_op_configure_window:
            a.result = xcb_configure_window(
                           conn, static_cast<xcb_window_t>(v[1]),
                           static_cast<uint16_t>(v[2]),
                           reinterpret_cast<const void*>(v[3]))
                           .sequence;
            break;
        case xcb_op_create_window:
            a.result =
                xcb_create_window(
                    conn, static_cast<uint8_t>(v[1]),
                    static_cast<xcb_window_t>(v[2]),
                    static_cast<xcb_window_t>(v[3]),
                    static_cast<int16_t>(v[4]), static_cast<int16_t>(v[5]),
                    static_cast<uint16_t>(v[6]), static_cast<uint16_t>(v[7]),
                    static_cast<uint16_t>(v[8]), static_cast<uint16_t>(v[9]),
                    static_cast<xcb_visualid_t>(v[10]),
                    static_cast<uint32_t>(v[11]),
                    reinterpret_cast<const void*>(v[12]))
                    .sequence;
            break;
        case xcb_op_delete_property:
            a.result = xcb_delete_property(conn,
                                           static_cast<xcb_window_t>(v[1]),
                                           static_cast<xcb_atom_t>(v[2]))
                           .sequence;
            break;
        case xcb_op_destroy_window:
            a.result =
                xcb_destroy_window(conn, static_cast<xcb_window_t>(v[1]))
                    .sequence;
            break;
        case xcb_op_get_input_focus:
            a.result = xcb_get_input_focus(conn).sequence;
            break;
        case xcb_op_get_input_focus_reply:
            error_ptr = v[2] ? &error : nullptr;
            set_reply(a,
                      xcb_get_input_focus_reply(
                          conn,
                          xcb_get_input_focus_cookie_t{
                              static_cast<unsigned int>(v[1])},
                          error_ptr),
                      error);
            break;
        case xcb_op_get_keyboard_mapping:
            a.result =
                xcb_get_keyboard_mapping(conn, static_cast<xcb_keycode_t>(v[1]),
                                         static_cast<uint8_t>(v[2]))
                    .sequence;
            break;
        case xcb_op_get_keyboard_mapping_reply:
            error_ptr = v[2] ? &error : nullptr;
            set_reply(a,
                      xcb_get_keyboard_mapping_reply(
                          conn,
                          xcb_get_keyboard_mapping_cookie_t{
                              static_cast<unsigned int>(v[1])},
                          error_ptr),
                      error);
            break;
        case xcb_op_get_keyboard_mapping_keysyms:
            a.result = reinterpret_cast<uint64_t>(
                xcb_get_keyboard_mapping_keysyms(
                    reinterpret_cast<const xcb_get_keyboard_mapping_reply_t*>(
                        v[0])));
            break;
        case xcb_op_get_keyboard_mapping_keysyms_length:
            a.result = static_cast<uint32_t>(
                xcb_get_keyboard_mapping_keysyms_length(
                    reinterpret_cast<const xcb_get_keyboard_mapping_reply_t*>(
                        v[0])));
            break;
        case xcb_op_get_property:
            a.result =
                xcb_get_property(conn, static_cast<uint8_t>(v[1]),
                                 static_cast<xcb_window_t>(v[2]),
                                 static_cast<xcb_atom_t>(v[3]),
                                 static_cast<xcb_atom_t>(v[4]),
                                 static_cast<uint32_t>(v[5]),
                                 static_cast<uint32_t>(v[6]))
                    .sequence;
            break;
        case xcb_op_get_property_reply:
            error_ptr = v[2] ? &error : nullptr;
            set_reply(
                a,
                xcb_get_property_reply(
                    conn,
                    xcb_get_property_cookie_t{static_cast<unsigned int>(v[1])},
                    error_ptr),
                error);
            break;
        case xcb_op_get_property_value:
            a.result = reinterpret_cast<uint64_t>(xcb_get_property_value(
                reinterpret_cast<const xcb_get_property_reply_t*>(v[0])));
            break;
        case xcb_op_grab_key:
            a.result =
                xcb_grab_key(conn, static_cast<uint8_t>(v[1]),
                             static_cast<xcb_window_t>(v[2]),
                             static_cast<uint16_t>(v[3]),
                             static_cast<xcb_keycode_t>(v[4]),
                             static_cast<uint8_t>(v[5]),
                             static_cast<uint8_t>(v[6]))
                    .sequence;
            break;
        case xcb_op_intern_atom:
            a.result = xcb_intern_atom(conn, static_cast<uint8_t>(v[1]),
                                       static_cast<uint16_t>(v[2]),
                                       reinterpret_cast<const char*>(v[3]))
                           .sequence;
            break;
        case xcb_op_intern_atom_reply:
            error_ptr = v[2] ? &error : nullptr;
            set_reply(
                a,
                xcb_intern_atom_reply(
                    conn,
                    xcb_intern_atom_cookie_t{static_cast<unsigned int>(v[1])},
                    error_ptr),
                error);
            break;
        case xcb_op_map_window:
            a.result =
                xcb_map_window(conn, static_cast<xcb_window_t>(v[1])).sequence;
            break;
        case xcb_op_query_pointer:
            a.result =
                xcb_query_pointer(conn, static_cast<xcb_window_t>(v[1]))
                    .sequence;
            break;
        case xcb_op_query_pointer_reply:
            error_ptr = v[2] ? &error : nullptr;
            set_reply(
                a,
                xcb_query_pointer_reply(
                    conn,
                    xcb_query_pointer_cookie_t{static_cast<unsigned int>(v[1])},
                    error_ptr),
                error);
            break;
        case xcb_op_query_tree:
            a.result =
                xcb_query_tree(conn, static_cast<xcb_window_t>(v[1])).sequence;
            break;
        case xcb_op_query_tree_reply:
            error_ptr = v[2] ? &error : nullptr;
            set_reply(
                a,
                xcb_query_tree_reply(
                    conn,
                    xcb_query_tree_cookie_t{static_cast<unsigned int>(v[1])},
                    error_ptr),
                error);
            break;
        case xcb_op_reparent_window:
            a.result = xcb_reparent_window(conn,
                                           static_cast<xcb_window_t>(v[1]),
                                           static_cast<xcb_window_t>(v[2]),
                                           static_cast<int16_t>(v[3]),
                                           static_cast<int16_t>(v[4]))
                           .sequence;
            break;
        case xcb_op_reparent_window_checked:
            a.result = xcb_reparent_window_checked(
                           conn, static_cast<xcb_window_t>(v[1]),
                           static_cast<xcb_window_t>(v[2]),
                           static_cast<int16_t>(v[3]),
                           static_cast<int16_t>(v[4]))
                           .sequence;
            break;
        case xcb_op_send_event:
            a.result = xcb_send_event(conn, static_cast<uint8_t>(v[1]),
                                      static_cast<xcb_window_t>(v[2]),
                                      static_cast<uint32_t>(v[3]),
                                      reinterpret_cast<const char*>(v[4]))
                           .sequence;
            break;
        case xcb_op_set_input_focus:
            a.result = xcb_set_input_focus(conn, static_cast<uint8_t>(v[1]),
                                           static_cast<xcb_window_t>(v[2]),
                                           static_cast<xcb_timestamp_t>(v[3]))
                           .sequence;
            break;
        case xcb_op_set_selection_owner:
            a.result =
                xcb_set_selection_owner(conn, static_cast<xcb_window_t>(v[1]),
                                        static_cast<xcb_atom_t>(v[2]),
                                        static_cast<xcb_timestamp_t>(v[3]))
                    .sequence;
            break;
        case xcb_op_translate_coordinates:
            a.result = xcb_translate_coordinates(
                           conn, static_cast<xcb_window_t>(v[1]),
                           static_cast<xcb_window_t>(v[2]),
                           static_cast<int16_t>(v[3]),
                           static_cast<int16_t>(v[4]))
                           .sequence;
            break;
        case xcb_op_translate_coordinates_reply:
            error_ptr = v[2] ? &error : nullptr;
            set_reply(a,
                      xcb_translate_coordinates_reply(
                          conn,
                          xcb_translate_coordinates_cookie_t{
                              static_cast<unsigned int>(v[1])},
                          error_ptr),
                      error);
            break;
        case xcb_op_ungrab_key:
            a.result = xcb_ungrab_key(conn, static_cast<xcb_keycode_t>(v[1]),
                                      static_cast<xcb_window_t>(v[2]),
                                      static_cast<uint16_t>(v[3]))
                           .sequence;
            break;

        case xcb_op_free:
            free(reinterpret_cast<void*>(v[0]));
            free(reinterpret_cast<void*>(v[1]));
            break;
    }

    return 0;
}

}  // namespace

// Looked up by `get_unixlib_funcs()` in `wine/dlls/ntdll/unix/virtual.c`. The
// order must match `unixlib::Call`.
extern "C" __attribute__((visibility("default")))
const unixlib_entry_t __wine_unix_call_funcs[] = {
    do_socket_connect,
    do_socket_listen,
    do_socket_accept,
    do_socket_read,
    do_socket_write,
    do_socket_shutdown,
    do_fd_close,
    do_path_unlink,
    do_path_create_directories,
    do_path_remove_all,
    do_path_socket_listening,

    do_shm_open_fd,
    do_shm_map,
    do_shm_destroy,

    do_process_id,
    do_process_running,
    do_thread_set_name,
    do_sched_get_realtime_priority,
    do_sched_set_realtime_priority,

    do_xcb_call,
};

static_assert(sizeof(__wine_unix_call_funcs) / sizeof(unixlib_entry_t) ==
              call_count);
