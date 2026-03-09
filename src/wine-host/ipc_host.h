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
 * @file ipc_host.h
 * @brief IPC host for Wine side communication with native side
 */

#pragma once

#include "socket.h"
#include <memory>
#include <string>

namespace vst3bridge {

/**
 * @brief IPC host for communication with native side
 * 
 * Wraps MessageSocket with higher-level message handling
 */
class IpcHost {
public:
    /**
     * @brief Construct IPC host
     * @param socket Connected message socket
     */
    explicit IpcHost(std::unique_ptr<MessageSocket> socket);

    ~IpcHost() = default;

    // Non-copyable
    IpcHost(const IpcHost&) = delete;
    IpcHost& operator=(const IpcHost&) = delete;

    // Movable
    IpcHost(IpcHost&&) = default;
    IpcHost& operator=(IpcHost&&) = default;

    /**
     * @brief Send a message with payload
     * @param type Message type
     * @param payload Payload data
     * @param payload_size Payload size
     * @return true on success
     */
    bool sendMessage(MsgType type, const void* payload, size_t payload_size);

    /**
     * @brief Receive a message
     * @param msg Output message
     * @param timeout_ms Timeout in milliseconds (-1 for infinite)
     * @return true on success
     */
    bool receiveMessage(GenericMessage& msg, int timeout_ms = -1);

    /**
     * @brief Check if connection is valid
     */
    bool isConnected() const;

    /**
     * @brief Close the connection
     */
    void close();

    /**
     * @brief Get underlying socket
     */
    MessageSocket* getSocket() { return socket_.get(); }

private:
    std::unique_ptr<MessageSocket> socket_;
};

} // namespace vst3bridge
