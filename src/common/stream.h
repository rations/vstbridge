/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

/**
 * @file stream.h
 * @brief IBStream implementation for plugin state serialization.
 *
 * This header re-exports vst3bridge::MemoryStream from istream_impl.h
 * and provides the legacy StreamData helper for IPC transfer.
 */

#pragma once

#include "istream_impl.h"
#include <vector>

namespace vst3bridge {

// MemoryStream is defined in istream_impl.h and re-exported here.

/**
 * @brief Serialised stream data for IPC transfer.
 *
 * Used when a plugin state blob must be sent over the socket.
 * Caller populates from a MemoryStream and then transmits the bytes.
 */
struct StreamData {
    uint32_t size = 0;
    std::vector<uint8_t> data;

    /**
     * @brief Populate from an existing MemoryStream.
     * Rewinds the stream, reads all bytes into data.
     */
    void fromStream(MemoryStream& stream) {
        stream.rewind();
        size = static_cast<uint32_t>(stream.size());
        data.resize(size);
        if (size > 0) {
            Steinberg::int32 read = 0;
            stream.read(data.data(), static_cast<Steinberg::int32>(size), &read);
        }
    }

    /**
     * @brief Create a new MemoryStream from the stored data.
     * The caller owns the returned IBStream reference (call release() when done).
     */
    Steinberg::IBStream* toStream() const {
        auto* s = new MemoryStream(data.data(),
                                   static_cast<Steinberg::int32>(data.size()));
        return s;
    }
};

} // namespace vst3bridge
