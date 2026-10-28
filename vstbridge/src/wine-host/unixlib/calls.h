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
// fixed width integers and `ptr64`s. `long` would be 32-bit on the PE side and
// 64-bit on the Linux side, and so would plain pointers for the 32-bit PE
// bitbridge host (`vstbridge-host-32.exe`) on a WoW64 Wine. With these the
// structs have the same layout for both PE architectures, and the unixlib's
// `__wine_unix_call_wow64_funcs` can be the same table as its
// `__wine_unix_call_funcs`.
//
// Under WoW64 the Linux side can use the 32-bit host's pointers as is, since
// they point into the low 4 GiB of the same address space. The reverse isn't
// true: libxcb's and `mmap()`'s memory lives above that, so those are copied
// or mapped into memory allocated on the PE side instead (see `xcb-calls.h`
// and `ShmMapArgs`).
//
// A Wine that still has its 32-bit Unix side (`i386-unix`, as in WineHQ's
// packages) runs the 32-bit host in an i386 Linux process instead, which can't
// load the 64-bit unixlib. That process loads the i386 build of the unixlib
// (`vstbridge-host-32-unixlib-i386.so`) through `__wine_unix_call_funcs`, so
// 64-bit fields use `aligned_uint64_t` to keep the layout the same there too.

#include <stdint.h>

namespace unixlib {

/**
 * A `uint64_t` with the 8-byte alignment it has on Windows. i386 Linux only
 * aligns 64-bit integers in structs to 4 bytes.
 */
typedef uint64_t aligned_uint64_t __attribute__((aligned(8)));

/**
 * A pointer stored as 64 bits on both sides.
 */
template <typename T>
struct ptr64 {
    aligned_uint64_t value = 0;

    ptr64() = default;
    // NOLINTNEXTLINE(google-explicit-constructor)
    ptr64(T* pointer) : value(reinterpret_cast<uintptr_t>(pointer)) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator T*() const {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(value));
    }
};

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
    ptr64<const char> path;
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
    ptr64<void> buffer;
    aligned_uint64_t size;
    // The number of bytes read or written. 0 on end of file for reads.
    aligned_uint64_t result;
};

struct FdArgs {
    int32_t fd;
    int32_t error;
};

struct PathArgs {
    ptr64<const char> path;
    int32_t error;
};

struct SocketListeningArgs {
    ptr64<const char> path;
    // Whether a process is listening on the socket at `path`, according to
    // `/proc/net/unix`
    int32_t listening;
};

struct ShmMapArgs {
    int32_t fd;
    int32_t error;
    aligned_uint64_t size;
    // The existing mapping to grow or shrink, or null. Not used together with
    // `fixed_address`.
    ptr64<void> old_address;
    aligned_uint64_t old_size;
    // If set, map the object over this PE-side allocation of `size` bytes
    // instead of wherever `mmap()` puts it. The 32-bit host needs this since
    // it can only address the low 4 GiB.
    ptr64<void> fixed_address;
    ptr64<void> address;
    // Set when mapping with `MAP_LOCKED` failed and the region was mapped
    // without locking it instead
    int32_t unlocked;
};

struct ShmDestroyArgs {
    ptr64<const char> name;
    int32_t fd;
    // Unmapped if set. A `fixed_address` mapping is instead released by the PE
    // side, since Wine still considers that memory its own.
    ptr64<void> address;
    aligned_uint64_t size;
};

struct ProcessArgs {
    int32_t pid;
    int32_t running;
};

struct ThreadNameArgs {
    ptr64<const char> name;
};

struct RealtimePriorityArgs {
    int32_t sched_fifo;
    int32_t priority;
    // For `sched_set_realtime_priority`, whether it succeeded
    int32_t success;
};

// The layouts have to be the same in the 32-bit PE host, the 64-bit one, and
// both unixlibs
static_assert(sizeof(PathFdArgs) == 16);
static_assert(sizeof(AcceptArgs) == 12);
static_assert(sizeof(ReadWriteArgs) == 32);
static_assert(sizeof(FdArgs) == 8);
static_assert(sizeof(PathArgs) == 16);
static_assert(sizeof(SocketListeningArgs) == 16);
static_assert(sizeof(ShmMapArgs) == 56);
static_assert(sizeof(ShmDestroyArgs) == 32);
static_assert(sizeof(ProcessArgs) == 8);
static_assert(sizeof(ThreadNameArgs) == 8);
static_assert(sizeof(RealtimePriorityArgs) == 12);

}  // namespace unixlib
