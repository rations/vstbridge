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

#include "ipc-socket.h"

#include <algorithm>

namespace ipc {

namespace {

// The unixlib reports Linux `errno` values, which have the same meaning as
// the POSIX error codes in `std::generic_category()`
std::error_code errno_to_error_code(int error) {
    return std::error_code(error, std::generic_category());
}

// The parent directory of a Linux path
std::string parent_path(const std::string& path) {
    const size_t last_separator = path.find_last_of('/');
    if (last_separator == std::string::npos) {
        return "";
    } else if (last_separator == 0) {
        return "/";
    } else {
        return path.substr(0, last_separator);
    }
}

}  // namespace

endpoint::endpoint(std::string path) : path_(std::move(path)) {
    std::replace(path_.begin(), path_.end(), '\\', '/');
}

socket::socket(socket&& o) noexcept
    : io_context_(o.io_context_), fd_(o.fd_.exchange(-1)) {}

socket& socket::operator=(socket&& o) noexcept {
    if (this != &o) {
        close();
        io_context_ = o.io_context_;
        fd_ = o.fd_.exchange(-1);
    }

    return *this;
}

socket::~socket() noexcept {
    close();
}

void socket::connect(const endpoint& endpoint) {
    std::error_code ec;
    connect(endpoint, ec);
    if (ec) {
        throw std::system_error(ec, "connect to '" + endpoint.path() + "'");
    }
}

void socket::connect(const endpoint& endpoint, std::error_code& ec) {
    close();

    unixlib::PathFdArgs args{.path = endpoint.path().c_str()};
    unixlib::call(unixlib::socket_connect, &args);
    if (args.error) {
        ec = errno_to_error_code(args.error);
    } else {
        ec.clear();
        fd_ = args.fd;
    }
}

size_t socket::read_some_raw(void* data, size_t size, std::error_code& ec) {
    if (fd_ == -1) {
        ec = asio::error::bad_descriptor;
        return 0;
    }
    if (size == 0) {
        ec.clear();
        return 0;
    }

    unixlib::ReadWriteArgs args{.fd = fd_, .buffer = data, .size = size};
    unixlib::call(unixlib::socket_read, &args);
    if (args.error) {
        ec = errno_to_error_code(args.error);
    } else if (args.result == 0) {
        ec = asio::error::eof;
    } else {
        ec.clear();
    }

    return args.result;
}

size_t socket::write_some_raw(const void* data,
                              size_t size,
                              std::error_code& ec) {
    if (fd_ == -1) {
        ec = asio::error::bad_descriptor;
        return 0;
    }
    if (size == 0) {
        ec.clear();
        return 0;
    }

    unixlib::ReadWriteArgs args{
        .fd = fd_, .buffer = const_cast<void*>(data), .size = size};
    unixlib::call(unixlib::socket_write, &args);
    if (args.error) {
        ec = errno_to_error_code(args.error);
    } else {
        ec.clear();
    }

    return args.result;
}

void socket::shutdown(shutdown_type /*what*/, std::error_code& ec) noexcept {
    // vstbridge only ever shuts down both directions
    if (fd_ == -1) {
        ec = asio::error::bad_descriptor;
        return;
    }

    unixlib::FdArgs args{.fd = fd_};
    unixlib::call(unixlib::socket_shutdown, &args);
    if (args.error) {
        ec = errno_to_error_code(args.error);
    } else {
        ec.clear();
    }
}

void socket::shutdown(shutdown_type what) {
    std::error_code ec;
    shutdown(what, ec);
    if (ec) {
        throw std::system_error(ec, "shutdown");
    }
}

void socket::close() noexcept {
    std::error_code ec;
    close(ec);
}

void socket::close(std::error_code& ec) noexcept {
    ec.clear();

    const int fd = fd_.exchange(-1);
    if (fd != -1) {
        unixlib::FdArgs args{.fd = fd};
        unixlib::call(unixlib::fd_close, &args);
        if (args.error) {
            ec = errno_to_error_code(args.error);
        }
    }
}

acceptor::acceptor(asio::io_context& io_context, const endpoint& endpoint)
    : io_context_(&io_context) {
    unixlib::PathFdArgs args{.path = endpoint.path().c_str()};
    unixlib::call(unixlib::socket_listen, &args);
    if (args.error) {
        throw std::system_error(errno_to_error_code(args.error),
                                "bind to '" + endpoint.path() + "'");
    }

    fd_ = args.fd;
}

acceptor::~acceptor() noexcept {
    close();
}

void acceptor::accept(socket& socket) {
    std::error_code ec;
    const int fd = accept_fd(ec);
    if (ec) {
        throw std::system_error(ec, "accept");
    }

    socket = ipc::socket(*io_context_, fd);
}

void acceptor::close() noexcept {
    if (fd_ == -1) {
        return;
    }

    // On Linux, shutting down a listening socket makes a blocked `accept()`
    // return `EINVAL`. The accepting thread then posts the error to the IO
    // context and exits, and only after that can we close the file descriptor
    // without another thread still using it.
    unixlib::FdArgs shutdown_args{.fd = fd_};
    unixlib::call(unixlib::socket_shutdown, &shutdown_args);
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    unixlib::FdArgs close_args{.fd = fd_};
    unixlib::call(unixlib::fd_close, &close_args);
    fd_ = -1;
}

int acceptor::accept_fd(std::error_code& ec) {
    unixlib::AcceptArgs args{.listen_fd = fd_};
    unixlib::call(unixlib::socket_accept, &args);
    if (args.error) {
        ec = errno_to_error_code(args.error);
        return -1;
    } else {
        ec.clear();
        return args.fd;
    }
}

void create_endpoint_directory(const endpoint& endpoint) {
    const std::string directory = parent_path(endpoint.path());
    unixlib::PathArgs args{.path = directory.c_str()};
    unixlib::call(unixlib::path_create_directories, &args);
    if (args.error) {
        throw std::system_error(errno_to_error_code(args.error),
                                "create_directories '" + directory + "'");
    }
}

void remove_endpoint(const endpoint& endpoint) {
    unixlib::PathArgs args{.path = endpoint.path().c_str()};
    unixlib::call(unixlib::path_unlink, &args);
}

void remove_directory(const std::string& path) {
    // Uses the same normalization as the endpoints
    const ipc::endpoint normalized(path);
    unixlib::PathArgs args{.path = normalized.path().c_str()};
    unixlib::call(unixlib::path_remove_all, &args);
}

}  // namespace ipc
