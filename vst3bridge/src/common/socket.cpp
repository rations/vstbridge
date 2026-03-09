// vst3bridge: A modern Wine VST3 bridge for Linux
// Copyright (C) 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.

#include "socket.h"
#include "protocol.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>

namespace vst3bridge {

// ============================================================================
// Socket Base Class
// ============================================================================

Socket::Socket()
    : fd_(-1)
{
}

Socket::~Socket() {
    Close();
}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.fd_)
    , last_error_(other.last_error_)
    , socket_path_(std::move(other.socket_path_))
{
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        
        fd_ = other.fd_;
        last_error_ = other.last_error_;
        socket_path_ = std::move(other.socket_path_);
        
        other.fd_ = -1;
    }
    return *this;
}

void Socket::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void Socket::SetError(int errno_code) {
    last_error_ = std::error_code(errno_code, std::system_category());
}

// ============================================================================
// ClientSocket Implementation
// ============================================================================

ClientSocket::ClientSocket()
    : Socket()
{
}

ClientSocket::~ClientSocket() {
    // Close() is called by base destructor
}

bool ClientSocket::Connect(const std::string& socket_path) {
    Close();
    
    socket_path_ = socket_path;
    
    // Create Unix domain socket
    fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
        SetError(errno);
        return false;
    }
    
    // Set up server address
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    // Handle abstract socket (Linux-specific) or filesystem socket
    if (socket_path[0] == '\0') {
        // Abstract socket
        size_t path_len = socket_path.length();
        if (path_len >= sizeof(addr.sun_path)) {
            path_len = sizeof(addr.sun_path) - 1;
        }
        std::memcpy(addr.sun_path, socket_path.c_str(), path_len);
    } else {
        // Filesystem socket
        if (socket_path.length() >= sizeof(addr.sun_path)) {
            SetError(ENAMETOOLONG);
            Close();
            return false;
        }
        std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    }
    
    // Connect to server
    if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        SetError(errno);
        Close();
        return false;
    }
    
    return true;
}

ssize_t ClientSocket::Send(const void* data, size_t size) {
    if (fd_ < 0) {
        SetError(EBADF);
        return -1;
    }
    
    ssize_t result = ::send(fd_, data, size, MSG_NOSIGNAL);
    if (result < 0) {
        SetError(errno);
    }
    return result;
}

ssize_t ClientSocket::Send(const std::vector<uint8_t>& data) {
    return Send(data.data(), data.size());
}

ssize_t ClientSocket::Receive(void* buffer, size_t max_size) {
    if (fd_ < 0) {
        SetError(EBADF);
        return -1;
    }
    
    ssize_t result = ::recv(fd_, buffer, max_size, 0);
    if (result < 0) {
        SetError(errno);
    }
    return result;
}

ssize_t ClientSocket::ReceiveTimeout(void* buffer, size_t max_size, int timeout_ms) {
    if (fd_ < 0) {
        SetError(EBADF);
        return -1;
    }
    
    // Set up timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_, &read_fds);
    
    struct timeval tv;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int result = select(fd_ + 1, &read_fds, nullptr, nullptr, timeout_ms >= 0 ? &tv : nullptr);
    if (result < 0) {
        SetError(errno);
        return -1;
    }
    if (result == 0) {
        // Timeout
        return 0;
    }
    
    // Data available
    return Receive(buffer, max_size);
}

bool ClientSocket::SendMessage(uint32_t type, const void* payload, size_t payload_size) {
    std::vector<uint8_t> message = SerializeMessage(static_cast<MsgType>(type), payload, payload_size);
    ssize_t sent = Send(message.data(), message.size());
    return sent == static_cast<ssize_t>(message.size());
}

bool ClientSocket::SendMessage(uint32_t type, const std::vector<uint8_t>& payload) {
    return SendMessage(type, payload.data(), payload.size());
}

// ============================================================================
// ServerSocket Implementation
// ============================================================================

ServerSocket::ServerSocket()
    : Socket()
{
}

ServerSocket::~ServerSocket() {
    // Remove socket file if we created one
    if (!socket_path_.empty() && socket_path_[0] != '\0') {
        ::unlink(socket_path_.c_str());
    }
}

bool ServerSocket::Listen(const std::string& socket_path) {
    Close();
    
    socket_path_ = socket_path;
    
    // Create Unix domain socket
    fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
        SetError(errno);
        return false;
    }
    
    // Allow reuse of address
    int reuse = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        // Non-fatal, continue
    }
    
    // Set up server address
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    if (socket_path[0] == '\0') {
        // Abstract socket
        size_t path_len = socket_path.length();
        if (path_len >= sizeof(addr.sun_path)) {
            path_len = sizeof(addr.sun_path) - 1;
        }
        std::memcpy(addr.sun_path, socket_path.c_str(), path_len);
    } else {
        // Filesystem socket - remove existing file
        ::unlink(socket_path.c_str());
        
        if (socket_path.length() >= sizeof(addr.sun_path)) {
            SetError(ENAMETOOLONG);
            Close();
            return false;
        }
        std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    }
    
    // Bind to address
    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        SetError(errno);
        Close();
        return false;
    }
    
    // Start listening
    if (::listen(fd_, 5) < 0) {
        SetError(errno);
        Close();
        return false;
    }
    
    return true;
}

std::unique_ptr<Socket> ServerSocket::Accept() {
    if (fd_ < 0) {
        SetError(EBADF);
        return nullptr;
    }
    
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0) {
        SetError(errno);
        return nullptr;
    }
    
    auto client = std::make_unique<Socket>();
    client->fd_ = client_fd;
    return client;
}

std::unique_ptr<Socket> ServerSocket::AcceptTimeout(int timeout_ms) {
    if (fd_ < 0) {
        SetError(EBADF);
        return nullptr;
    }
    
    // Set up timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_, &read_fds);
    
    struct timeval tv;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int result = select(fd_ + 1, &read_fds, nullptr, nullptr, timeout_ms >= 0 ? &tv : nullptr);
    if (result < 0) {
        SetError(errno);
        return nullptr;
    }
    if (result == 0) {
        // Timeout
        return nullptr;
    }
    
    // Connection available
    return Accept();
}

// ============================================================================
// SocketUtils Implementation
// ============================================================================

std::string SocketUtils::GenerateSocketPath(const std::string& prefix) {
    // Use abstract socket namespace (Linux-specific) for automatic cleanup
    // Abstract sockets start with a null byte and don't create filesystem entries
    std::string path;
    path.push_back('\0');  // Abstract socket indicator
    path += prefix;
    path += "_";
    
    // Add random suffix
    const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < 16; ++i) {
        path += chars[rand() % (sizeof(chars) - 1)];
    }
    
    return path;
}

bool SocketUtils::RemoveSocket(const std::string& socket_path) {
    if (socket_path.empty() || socket_path[0] == '\0') {
        // Abstract socket - nothing to remove
        return true;
    }
    return ::unlink(socket_path.c_str()) == 0;
}

bool SocketUtils::SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

bool SocketUtils::SetTimeout(int fd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    return true;
}

bool SocketUtils::HasData(int fd, int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    struct timeval tv;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int result = select(fd + 1, &read_fds, nullptr, nullptr, timeout_ms >= 0 ? &tv : nullptr);
    return result > 0;
}

std::string SocketUtils::GetErrorString(const std::error_code& error) {
    return error.message();
}

// ============================================================================
// MessageSocket Implementation
// ============================================================================

MessageSocket::MessageSocket()
{
}

MessageSocket::~MessageSocket()
{
    Close();
}

void MessageSocket::Attach(std::unique_ptr<Socket> socket) {
    socket_ = std::move(socket);
}

std::unique_ptr<Socket> MessageSocket::Detach() {
    return std::move(socket_);
}

bool MessageSocket::Send(uint32_t type, const void* payload, size_t payload_size) {
    if (!socket_ || !socket_->IsConnected()) {
        return false;
    }
    
    // Create header
    MessageHeader header;
    header.type = type;
    header.size = static_cast<uint32_t>(payload_size);
    header.timestamp = 0;  // TODO
    header.sequence = 0;   // TODO
    
    // Send header
    ssize_t sent = static_cast<ClientSocket*>(socket_.get())->Send(&header, sizeof(header));
    if (sent != sizeof(header)) {
        return false;
    }
    
    // Send payload if any
    if (payload_size > 0 && payload != nullptr) {
        sent = static_cast<ClientSocket*>(socket_.get())->Send(payload, payload_size);
        if (sent != static_cast<ssize_t>(payload_size)) {
            return false;
        }
    }
    
    return true;
}

bool MessageSocket::Send(uint32_t type, const std::vector<uint8_t>& payload) {
    return Send(type, payload.data(), payload.size());
}

bool MessageSocket::Receive(uint32_t& type, std::vector<uint8_t>& payload) {
    if (!socket_ || !socket_->IsConnected()) {
        return false;
    }
    
    // Receive header
    MessageHeader header;
    ssize_t received = static_cast<ClientSocket*>(socket_.get())->Receive(&header, sizeof(header));
    if (received != sizeof(header)) {
        return false;
    }
    
    type = header.type;
    
    // Receive payload
    if (header.size > 0) {
        payload.resize(header.size);
        size_t total_received = 0;
        
        while (total_received < header.size) {
            received = static_cast<ClientSocket*>(socket_.get())->Receive(
                payload.data() + total_received, 
                header.size - total_received
            );
            if (received <= 0) {
                return false;
            }
            total_received += received;
        }
    } else {
        payload.clear();
    }
    
    return true;
}

bool MessageSocket::IsConnected() const {
    return socket_ && socket_->IsConnected();
}

void MessageSocket::Close() {
    if (socket_) {
        socket_->Close();
    }
}

} // namespace vst3bridge
