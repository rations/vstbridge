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

#pragma once

// The interface between the PE plugin host (`vstbridge-host.exe`, built for
// ARM64EC on aarch64) and its native unixlib (`vstbridge-host-unixlib.so`).
// Winelib `.exe.so` executables are not supported on aarch64
// (`wine/tools/winegcc/winegcc.c`), so everything the host used to do through
// Linux APIs goes through `__wine_unix_call()` instead. The PE side loads the
// unixlib with `MemoryWineLoadUnixLibByName`
// (`wine/dlls/ntdll/unix/virtual.c`), and every call runs on the calling
// thread.
//
// This header is compiled by both sides, so the argument structs only use
// fixed width integers and pointers. `long` would be 32-bit on the PE side and
// 64-bit on the Linux side.

#include <stdint.h>

namespace unixlib {

/**
 * Indices into `__wine_unix_call_funcs` in the unixlib. The order here must
 * match the table in `unixlib.cpp`.
 */
enum Call : uint32_t {
    // Sockets
    socket_connect,
    socket_listen,
    socket_accept,
    socket_read,
    socket_write,
    socket_shutdown,
    fd_close,
    path_unlink,
    path_create_directories,
    path_remove_all,
    path_socket_listening,

    // Shared memory
    shm_open_fd,
    shm_map,
    shm_destroy,

    // Processes, threads and scheduling
    process_id,
    process_running,
    thread_set_name,
    sched_get_realtime_priority,
    sched_set_realtime_priority,

    // X11, see `xcb-calls.h`
    xcb_call,

    call_count,
};

/**
 * `errno` values are returned as-is, positive, and 0 means success.
 */

struct PathFdArgs {
    const char* path;
    int32_t fd;
    int32_t error;
};

struct AcceptArgs {
    int32_t listen_fd;
    int32_t fd;
    int32_t error;
};

struct ReadWriteArgs {
    int32_t fd;
    int32_t error;
    void* buffer;
    uint64_t size;
    // The number of bytes read or written. 0 on end of file for reads.
    uint64_t result;
};

struct FdArgs {
    int32_t fd;
    int32_t error;
};

struct PathArgs {
    const char* path;
    int32_t error;
};

struct SocketListeningArgs {
    const char* path;
    // Whether a process is listening on the socket at `path`, according to
    // `/proc/net/unix`
    int32_t listening;
};

struct ShmMapArgs {
    int32_t fd;
    int32_t error;
    uint64_t size;
    // The existing mapping to grow or shrink, or null
    void* old_address;
    uint64_t old_size;
    void* address;
    // Set when mapping with `MAP_LOCKED` failed and the region was mapped
    // without locking it instead
    int32_t unlocked;
};

struct ShmDestroyArgs {
    const char* name;
    int32_t fd;
    void* address;
    uint64_t size;
};

struct ProcessArgs {
    int32_t pid;
    int32_t running;
};

struct ThreadNameArgs {
    const char* name;
};

struct RealtimePriorityArgs {
    int32_t sched_fifo;
    int32_t priority;
    // For `sched_set_realtime_priority`, whether it succeeded
    int32_t success;
};

}  // namespace unixlib
