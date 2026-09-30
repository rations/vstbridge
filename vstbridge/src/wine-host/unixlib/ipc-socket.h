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

// Unix domain sockets for the PE plugin host, with the subset of
// `asio::local::stream_protocol`'s interface that vstbridge uses. The socket
// operations are native Linux calls made through the unixlib. Winsock's AF_UNIX
// support (wine-staging's ws2_32-af_unix) was measured to be about nine times
// slower per round trip on the Pi 5, since every `recv()` and `send()` is a
// wineserver request (`wine/dlls/ntdll/unix/socket.c`, `sock_recv()` and
// `sock_send()`), and it can't `AcceptEx()` these sockets.
//
// Asio itself is still used on the PE side for its IO contexts and timers.

#include <atomic>
#include <string>
#include <system_error>
#include <thread>

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include "calls.h"
#include "pe.h"

namespace ipc {

/**
 * A Linux path to a Unix domain socket. `ghc::filesystem` uses backslashes as
 * the separator on Windows, so those are turned back into forward slashes.
 * vstbridge's socket paths never contain backslashes.
 */
class endpoint {
   public:
    endpoint() = default;
    endpoint(std::string path);
    endpoint(const char* path) : endpoint(std::string(path)) {}

    const std::string& path() const noexcept { return path_; }

   private:
    std::string path_;
};

/**
 * A connected Unix domain socket. Like Asio's sockets, the read and write
 * operations are blocking. `shutdown()` from another thread wakes up a thread
 * that's blocked reading from this socket.
 */
class socket {
   public:
    enum shutdown_type {
        shutdown_receive,
        shutdown_send,
        shutdown_both,
    };

    explicit socket(asio::io_context& io_context) noexcept
        : io_context_(&io_context) {}
    /**
     * Take ownership of an existing socket file descriptor.
     */
    socket(asio::io_context& io_context, int fd) noexcept
        : io_context_(&io_context), fd_(fd) {}

    socket(socket&& o) noexcept;
    socket& operator=(socket&& o) noexcept;
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    ~socket() noexcept;

    bool is_open() const noexcept { return fd_ != -1; }

    void connect(const endpoint& endpoint);
    void connect(const endpoint& endpoint, std::error_code& ec);

    template <typename MutableBufferSequence>
    size_t read_some(const MutableBufferSequence& buffers,
                     std::error_code& ec) {
        const auto buffer = first_buffer<asio::mutable_buffer>(buffers);
        return read_some_raw(buffer.data(), buffer.size(), ec);
    }

    template <typename MutableBufferSequence>
    size_t read_some(const MutableBufferSequence& buffers) {
        std::error_code ec;
        const size_t result = read_some(buffers, ec);
        if (ec) {
            throw std::system_error(ec, "read_some");
        }

        return result;
    }

    template <typename ConstBufferSequence>
    size_t write_some(const ConstBufferSequence& buffers,
                      std::error_code& ec) {
        const auto buffer = first_buffer<asio::const_buffer>(buffers);
        return write_some_raw(buffer.data(), buffer.size(), ec);
    }

    template <typename ConstBufferSequence>
    size_t write_some(const ConstBufferSequence& buffers) {
        std::error_code ec;
        const size_t result = write_some(buffers, ec);
        if (ec) {
            throw std::system_error(ec, "write_some");
        }

        return result;
    }

    void shutdown(shutdown_type what, std::error_code& ec) noexcept;
    void shutdown(shutdown_type what);

    void close() noexcept;
    void close(std::error_code& ec) noexcept;

   private:
    // Asio's `read()` and `write()` pass buffer sequences, but they only ever
    // need the first non-empty buffer to make progress
    template <typename Buffer, typename BufferSequence>
    static Buffer first_buffer(const BufferSequence& buffers) {
        const auto end = asio::buffer_sequence_end(buffers);
        for (auto it = asio::buffer_sequence_begin(buffers); it != end; ++it) {
            const Buffer buffer(*it);
            if (buffer.size() > 0) {
                return buffer;
            }
        }

        return Buffer();
    }

    size_t read_some_raw(void* data, size_t size, std::error_code& ec);
    size_t write_some_raw(const void* data, size_t size, std::error_code& ec);

    asio::io_context* io_context_;
    std::atomic_int fd_ = -1;
};

/**
 * Listens on a Unix domain socket. Accepting can be done synchronously, or
 * asynchronously on the IO context the acceptor was created with. Asynchronous
 * accepts block a helper thread in the unixlib and post the result back to the
 * IO context, keeping the IO context's `run()` busy until then like Asio does.
 */
class acceptor {
   public:
    /**
     * Bind and listen on `endpoint`.
     *
     * @throw std::system_error If the socket could not be bound, for instance
     *   because the socket file already exists.
     */
    acceptor(asio::io_context& io_context, const endpoint& endpoint);

    acceptor(const acceptor&) = delete;
    acceptor& operator=(const acceptor&) = delete;

    ~acceptor() noexcept;

    void accept(socket& socket);

    /**
     * Accept a connection on another thread and call `handler(const
     * std::error_code&, ipc::socket)` from the IO context. Only one
     * asynchronous accept can be active at a time, which matches how vstbridge
     * uses Asio's acceptors.
     */
    template <typename Handler>
    void async_accept(Handler&& handler) {
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }

        accept_thread_ = std::thread(
            [this, io_context = io_context_,
             work = asio::make_work_guard(*io_context_),
             handler = std::forward<Handler>(handler)]() mutable {
                std::error_code ec;
                const int fd = accept_fd(ec);

                asio::post(*io_context,
                           [handler = std::move(handler), ec,
                            accepted = socket(*io_context, fd)]() mutable {
                               handler(ec, std::move(accepted));
                           });
            });
    }

    void close() noexcept;

   private:
    int accept_fd(std::error_code& ec);

    asio::io_context* io_context_;
    int fd_ = -1;
    std::thread accept_thread_;
};

/**
 * `ghc::filesystem::create_directories()` for the parent directory of a Linux
 * endpoint path.
 */
void create_endpoint_directory(const endpoint& endpoint);

/**
 * Remove a Linux socket file. Errors are ignored.
 */
void remove_endpoint(const endpoint& endpoint);

/**
 * Recursively remove a directory using its Linux path. Errors are ignored.
 */
void remove_directory(const std::string& path);

}  // namespace ipc
