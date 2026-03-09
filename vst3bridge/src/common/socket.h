// vst3bridge: A modern Wine VST3 bridge for Linux
// Copyright (C) 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <system_error>

namespace vst3bridge {

// ============================================================================
// Unix Domain Socket Communication
// ============================================================================
// 
// Provides reliable, message-based communication between the native Linux
// plugin and the Wine host process.

class Socket {
public:
    using MessageHandler = std::function<void(const uint8_t* data, size_t size)>;
    using ErrorHandler = std::function<void(const std::error_code& error)>;

    Socket();
    virtual ~Socket();

    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Movable
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Check if socket is connected
    bool IsConnected() const { return fd_ >= 0; }

    // Get last error
    std::error_code GetLastError() const { return last_error_; }

    // Close the socket
    void Close();

protected:
    int fd_;
    std::error_code last_error_;
    std::string socket_path_;

    void SetError(int errno_code);
};

// ============================================================================
// Client Socket (Native Linux Side)
// ============================================================================

class ClientSocket : public Socket {
public:
    ClientSocket();
    ~ClientSocket() override;

    // Connect to a server socket
    // socket_path: Path to the Unix domain socket
    // Returns true on success
    bool Connect(const std::string& socket_path);

    // Send data
    // Returns number of bytes sent, or -1 on error
    ssize_t Send(const void* data, size_t size);
    ssize_t Send(const std::vector<uint8_t>& data);

    // Receive data (blocking)
    // Returns number of bytes received, 0 if disconnected, or -1 on error
    ssize_t Receive(void* buffer, size_t max_size);

    // Receive with timeout
    // timeout_ms: Timeout in milliseconds (-1 = infinite)
    ssize_t ReceiveTimeout(void* buffer, size_t max_size, int timeout_ms);

    // Send message with header (convenience method)
    bool SendMessage(uint32_t type, const void* payload, size_t payload_size);
    bool SendMessage(uint32_t type, const std::vector<uint8_t>& payload);
};

// ============================================================================
// Server Socket (Wine Host Side)
// ============================================================================

class ServerSocket : public Socket {
public:
    ServerSocket();
    ~ServerSocket() override;

    // Bind and listen on a socket
    // socket_path: Path to create the Unix domain socket
    // Returns true on success
    bool Listen(const std::string& socket_path);

    // Accept a connection (blocking)
    // Returns connected client socket, or nullptr on error
    std::unique_ptr<Socket> Accept();

    // Accept with timeout
    std::unique_ptr<Socket> AcceptTimeout(int timeout_ms);

    // Get the socket path
    const std::string& GetSocketPath() const { return socket_path_; }
};

// ============================================================================
// Socket Utilities
// ============================================================================

class SocketUtils {
public:
    // Generate a unique socket path in the runtime directory
    static std::string GenerateSocketPath(const std::string& prefix);

    // Remove a socket file
    static bool RemoveSocket(const std::string& socket_path);

    // Set socket to non-blocking mode
    static bool SetNonBlocking(int fd);

    // Set socket timeout
    static bool SetTimeout(int fd, int timeout_ms);

    // Check if data is available to read
    static bool HasData(int fd, int timeout_ms);

    // Get error string from error code
    static std::string GetErrorString(const std::error_code& error);
};

// ============================================================================
// Message Socket (Higher-level interface)
// ============================================================================
// 
// Provides message-based communication with automatic header handling

class MessageSocket {
public:
    MessageSocket();
    ~MessageSocket();

    // Wrap an existing socket
    void Attach(std::unique_ptr<Socket> socket);
    
    // Detach and return the underlying socket
    std::unique_ptr<Socket> Detach();

    // Send a complete message (header + payload)
    bool Send(uint32_t type, const void* payload, size_t payload_size);
    bool Send(uint32_t type, const std::vector<uint8_t>& payload);

    // Receive a complete message
    // Returns true if a message was received, false on error/disconnect
    // The payload is stored in the internal buffer
    bool Receive(uint32_t& type, std::vector<uint8_t>& payload);

    // Check if connected
    bool IsConnected() const;

    // Close connection
    void Close();

private:
    std::unique_ptr<Socket> socket_;
    std::vector<uint8_t> receive_buffer_;
    std::vector<uint8_t> payload_buffer_;
};

} // namespace vst3bridge
