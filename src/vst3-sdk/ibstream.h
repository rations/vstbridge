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
 * @file ibstream.h
 * @brief VST3 Stream interface
 * 
 * Based on Steinberg VST3 SDK specification
 */

#pragma once

#include "funknown.h"

namespace Steinberg {

/**
 * @brief IBStream - Binary stream interface
 * 
 * Used for plugin state serialization/deserialization.
 */
class IBStream : public FUnknown {
public:
    /**
     * @brief Read data from stream
     * @param buffer Buffer to read into
     * @param numBytes Number of bytes to read
     * @param numBytesRead Output: actual bytes read (can be null)
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API read(void* buffer, int32 numBytes, int32* numBytesRead = nullptr) = 0;

    /**
     * @brief Write data to stream
     * @param buffer Buffer to write from
     * @param numBytes Number of bytes to write
     * @param numBytesWritten Output: actual bytes written (can be null)
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API write(void* buffer, int32 numBytes, int32* numBytesWritten = nullptr) = 0;

    /**
     * @brief Seek to position
     * @param pos Position in bytes
     * @param mode Seek mode (see SeekMode)
     * @param result Output: new position (can be null)
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API seek(int64 pos, int32 mode, int64* result = nullptr) = 0;

    /**
     * @brief Tell current position
     * @param pos Output: current position
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API tell(int64* pos) = 0;

    static const FUID iid;
};

/**
 * @brief Seek modes
 */
enum SeekMode {
    kIBSeekSet = 0,  ///< Seek from beginning
    kIBSeekCur = 1,  ///< Seek from current position
    kIBSeekEnd = 2   ///< Seek from end
};

/**
 * @brief ISizeableStream - Stream with known size
 */
class ISizeableStream : public FUnknown {
public:
    /**
     * @brief Get stream size
     * @param size Output: size in bytes
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API getStreamSize(int64& size) = 0;

    /**
     * @brief Set stream size
     * @param size New size in bytes
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API setStreamSize(int64 size) = 0;

    static const FUID iid;
};

// Interface IDs
inline constexpr FUID IBStream_iid(0x4A4F8E3B, 0x6C7D5F29, 0xB8E4F9A6, 0x3D9C5B2F);
inline constexpr FUID ISizeableStream_iid(0x5B5F9C4C, 0x7D8E6F3A, 0xC9F50B7A, 0x4E0D6C3A);

} // namespace Steinberg
