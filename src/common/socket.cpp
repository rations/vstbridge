/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * @file socket.cpp
 * @brief Unix domain socket implementation
 */

#include "socket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>

namespace vst3bridge {

// =============================================================================
// MessageSocket Implementation
// =============================================================================

MessageSocket::MessageSocket(int fd) : fd_(fd) {
    // Set socket to blocking mode by default
    if (fd_ >= 0) {
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
        }
    }
}

MessageSocket::~MessageSocket() {
    close();
}

MessageSocket::MessageSocket(MessageSocket&& other) noexcept 
    : fd_(other.fd_) {
    other.fd_ = -1;
}

MessageSocket& MessageSocket::operator=(MessageSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool MessageSocket::sendAll(const void* data, size_t size) {
    if (fd_ < 0) return false;
    
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = size;
    
    while (remaining > 0) {
        ssize_t sent = send(fd_, ptr, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

bool MessageSocket::receiveAll(void* data, size_t size) {
    if (fd_ < 0) return false;
    
    char* ptr = static_cast<char*>(data);
    size_t remaining = size;
    
    while (remaining > 0) {
        ssize_t received = recv(fd_, ptr, remaining, 0);
        if (received <= 0) {
            if (received < 0 && errno == EINTR) continue;
            return false;
        }
        ptr += received;
        remaining -= received;
    }
    return true;
}

bool MessageSocket::sendMessage(MsgType type, const void* payload, size_t payload_size) {
    if (fd_ < 0) return false;
    
    MessageHeader header;
    header.magic = MessageHeader::kMagic;
    header.version = PROTOCOL_VERSION;
    header.type = type;
    header.payload_size = static_cast<uint32_t>(payload_size);
    header.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // Send header
    if (!sendAll(&header, sizeof(header))) {
        return false;
    }
    
    // Send payload if any
    if (payload_size > 0 && payload) {
        return sendAll(payload, payload_size);
    }
    
    return true;
}

bool MessageSocket::receiveHeader(MessageHeader& header) {
    if (!receiveAll(&header, sizeof(header))) {
        return false;
    }
    
    // Validate magic number
    if (header.magic != MessageHeader::kMagic) {
        return false;
    }
    
    // Validate protocol version
    if (header.version != PROTOCOL_VERSION) {
        return false;
    }
    
    return true;
}

bool MessageSocket::receivePayload(void* buffer, size_t size) {
    return receiveAll(buffer, size);
}

bool MessageSocket::receiveMessage(void* buffer, size_t max_size, MsgType& type) {
    MessageHeader header;
    if (!receiveHeader(header)) {
        return false;
    }
    
    type = header.type;
    
    if (header.payload_size > max_size) {
        // Payload too large - drain it
        char drain[4096];
        size_t remaining = header.payload_size;
        while (remaining > 0) {
            size_t to_read = std::min(remaining, sizeof(drain));
            if (!receiveAll(drain, to_read)) {
                return false;
            }
            remaining -= to_read;
        }
        return false;
    }
    
    if (header.payload_size > 0) {
        return receiveAll(buffer, header.payload_size);
    }
    
    return true;
}

bool MessageSocket::receiveMessageWithTimeout(void* buffer, size_t max_size, 
                                               MsgType& type, 
                                               std::chrono::milliseconds timeout) {
    if (fd_ < 0) return false;
    
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;
    
    int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (ret <= 0) {
        return false;  // Timeout or error
    }
    
    return receiveMessage(buffer, max_size, type);
}

bool MessageSocket::sendRaw(const void* data, size_t size) {
    return sendAll(data, size);
}

bool MessageSocket::receiveRaw(void* buf, size_t size) {
    return receiveAll(buf, size);
}

void MessageSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void MessageSocket::setReceiveTimeout(int timeout_ms) {
    if (fd_ < 0) return;
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

// =============================================================================
// SocketServer Implementation
// =============================================================================

SocketServer::SocketServer() = default;

SocketServer::~SocketServer() {
    close();
}

bool SocketServer::bind(const std::string& path) {
    // Remove existing socket file
    unlink(path.c_str());
    
    fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
        return false;
    }
    
    // Allow reuse of address
    int reuse = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    
    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    path_ = path;
    return true;
}

bool SocketServer::listen(int backlog) {
    if (fd_ < 0) return false;
    return ::listen(fd_, backlog) == 0;
}

std::unique_ptr<MessageSocket> SocketServer::accept() {
    if (fd_ < 0) return nullptr;
    
    struct sockaddr_un addr;
    socklen_t len = sizeof(addr);
    
    int client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&addr), &len);
    if (client_fd < 0) {
        return nullptr;
    }
    
    return std::make_unique<MessageSocket>(client_fd);
}

std::unique_ptr<MessageSocket> SocketServer::acceptWithTimeout(int timeout_ms) {
    if (fd_ < 0) return nullptr;
    
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;
    
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return nullptr;  // Timeout or error
    }
    
    return accept();
}

void SocketServer::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (!path_.empty()) {
        unlink(path_.c_str());
        path_.clear();
    }
}

// =============================================================================
// Client Connection Functions
// =============================================================================

std::unique_ptr<MessageSocket> connectToSocket(const std::string& path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return nullptr;
    }
    
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return nullptr;
    }
    
    return std::make_unique<MessageSocket>(fd);
}

std::unique_ptr<MessageSocket> connectToSocketWithTimeout(const std::string& path, 
                                                          int timeout_ms) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return nullptr;
    }
    
    // Set non-blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    
    int result = connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (result < 0 && errno == EINPROGRESS) {
        // Connection in progress - wait for it
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        
        result = poll(&pfd, 1, timeout_ms);
        if (result > 0) {
            int so_error;
            socklen_t len = sizeof(so_error);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
            if (so_error == 0) {
                result = 0;  // Success
            } else {
                result = -1;
            }
        } else {
            result = -1;
        }
    }
    
    if (result < 0) {
        ::close(fd);
        return nullptr;
    }
    
    // Set back to blocking
    fcntl(fd, F_SETFL, flags);
    
    return std::make_unique<MessageSocket>(fd);
}

} // namespace vst3bridge
