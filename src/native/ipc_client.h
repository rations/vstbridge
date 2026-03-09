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
 * @file ipc_client.h
 * @brief IPC client for native side communication with Wine host
 */

#pragma once

#include "socket.h"
#include <memory>
#include <string>

namespace vst3bridge {

/**
 * @brief IPC client for communication with Wine host
 * 
 * Wraps MessageSocket with higher-level message handling
 */
class IpcClient {
public:
    /**
     * @brief Construct IPC client
     * @param socket Connected message socket
     */
    explicit IpcClient(std::unique_ptr<MessageSocket> socket);

    ~IpcClient() = default;

    // Non-copyable
    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    // Movable
    IpcClient(IpcClient&&) = default;
    IpcClient& operator=(IpcClient&&) = default;

    /**
     * @brief Send a message with payload
     * @param type Message type
     * @param payload Payload data
     * @param payload_size Payload size
     * @return true on success
     */
    bool sendMessage(MsgType type, const void* payload, size_t payload_size);

    /**
     * @brief Send raw data (for custom protocols like GUI events)
     * @param data Data to send
     * @param size Data size
     * @return true on success
     */
    bool sendRawMessage(const void* data, size_t size);

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
