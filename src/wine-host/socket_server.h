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
 * @file socket_server.h
 * @brief Wine-side socket server
 */

#pragma once

#include <string>

namespace vst3bridge {

/**
 * @brief Socket server for Wine side
 */
class WineSocketServer {
public:
    WineSocketServer();
    ~WineSocketServer();

    // Non-copyable
    WineSocketServer(const WineSocketServer&) = delete;
    WineSocketServer& operator=(const WineSocketServer&) = delete;

    /**
     * @brief Connect to native plugin
     * @param socket_path Unix socket path
     * @return true on success
     */
    bool connect(const std::string& socket_path);

    /**
     * @brief Disconnect
     */
    void disconnect();

    /**
     * @brief Check if connected
     * @return true if connected
     */
    bool isConnected() const;

private:
    int fd_ = -1;
};

} // namespace vst3bridge
