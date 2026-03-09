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
 * @file ipc_host.cpp
 * @brief IPC host implementation
 */

#include "ipc_host.h"
#include <cstring>

namespace vst3bridge {

IpcHost::IpcHost(std::unique_ptr<MessageSocket> socket)
    : socket_(std::move(socket)) {
}

bool IpcHost::sendMessage(MsgType type, const void* payload, size_t payload_size) {
    if (!socket_ || !socket_->isValid()) {
        return false;
    }
    return socket_->sendMessage(type, payload, payload_size);
}

bool IpcHost::receiveMessage(GenericMessage& msg, int timeout_ms) {
    if (!socket_ || !socket_->isValid()) {
        return false;
    }

    if (timeout_ms >= 0) {
        socket_->setReceiveTimeout(timeout_ms);
    }

    MessageHeader header;
    if (!socket_->receiveHeader(header)) {
        return false;
    }

    msg.header = header;
    
    if (header.payload_size > 0) {
        msg.payload.resize(header.payload_size);
        if (!socket_->receivePayload(msg.payload.data(), header.payload_size)) {
            return false;
        }
    } else {
        msg.payload.clear();
    }

    return true;
}

bool IpcHost::isConnected() const {
    return socket_ && socket_->isValid();
}

void IpcHost::close() {
    if (socket_) {
        socket_->close();
    }
}

} // namespace vst3bridge
