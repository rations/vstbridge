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
 * @file shared_memory.h
 * @brief POSIX shared memory wrapper for audio and GUI
 */

#pragma once

#include "protocol.h"
#include <memory>
#include <string>
#include <atomic>

namespace vst3bridge {

/**
 * @brief POSIX shared memory wrapper
 */
class SharedMemory {
public:
    /**
     * @brief Create or open shared memory
     * @param name Shared memory object name (should start with /)
     * @param size Size in bytes
     * @param create true to create, false to open existing
     */
    SharedMemory(const std::string& name, size_t size, bool create);
    
    ~SharedMemory();

    // Non-copyable and non-movable
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;

    /**
     * @brief Get pointer to mapped memory
     * @return Pointer to memory
     */
    void* getData() const { return data_; }

    /**
     * @brief Get size of mapped memory
     * @return Size in bytes
     */
    size_t getSize() const { return size_; }

    /**
     * @brief Check if mapping is valid
     * @return true if valid
     */
    bool isValid() const { return data_ != nullptr; }

    /**
     * @brief Unlink the shared memory object (removes name)
     */
    void unlink();

    /**
     * @brief Get the shared memory name
     * @return Name
     */
    const std::string& getName() const { return name_; }

    /**
     * @brief Generate a unique shared memory name
     * @param prefix Prefix for the name
     * @return Unique name
     */
    static std::string generateName(const std::string& prefix);

private:
    std::string name_;
    size_t size_ = 0;
    void* data_ = nullptr;
    int fd_ = -1;
    bool is_owner_ = false;
};

/**
 * @brief Audio shared memory buffer
 * 
 * Layout:
 * - AudioBufferLayout header
 * - Input channel buffers
 * - Output channel buffers
 */
class AudioSharedMemory {
public:
    /**
     * @brief Create audio shared memory
     * @param max_channels Maximum number of channels per bus
     * @param max_samples Maximum samples per buffer
     * @param is_host true if this is the host (Wine) side
     */
    AudioSharedMemory(uint32_t max_channels, uint32_t max_samples, bool is_host);
    
    ~AudioSharedMemory();

    // Non-copyable and non-movable
    AudioSharedMemory(const AudioSharedMemory&) = delete;
    AudioSharedMemory& operator=(const AudioSharedMemory&) = delete;
    AudioSharedMemory(AudioSharedMemory&&) = delete;
    AudioSharedMemory& operator=(AudioSharedMemory&&) = delete;

    /**
     * @brief Get layout header
     * @return Pointer to layout
     */
    AudioBufferLayout* getLayout() const {
        return static_cast<AudioBufferLayout*>(shm_->getData());
    }

    /**
     * @brief Get input buffer for a channel
     * @param bus Bus index
     * @param channel Channel index within bus
     * @return Pointer to float buffer, nullptr if invalid
     */
    float* getInputChannel(uint32_t bus, uint32_t channel) const;

    /**
     * @brief Get output buffer for a channel
     * @param bus Bus index
     * @param channel Channel index within bus
     * @return Pointer to float buffer, nullptr if invalid
     */
    float* getOutputChannel(uint32_t bus, uint32_t channel) const;

    /**
     * @brief Set the number of buses and channels
     * @param input_buses Array of input channel counts per bus
     * @param num_input_buses Number of input buses
     * @param output_buses Array of output channel counts per bus
     * @param num_output_buses Number of output buses
     */
    void setBusConfiguration(const uint32_t* input_buses, uint32_t num_input_buses,
                             const uint32_t* output_buses, uint32_t num_output_buses);

    /**
     * @brief Get shared memory name for passing to other process
     * @return Shared memory name
     */
    const std::string& getName() const { return shm_->getName(); }

    /**
     * @brief Open existing audio shared memory
     * @param name Shared memory name
     * @return AudioSharedMemory object, nullptr on error
     */
    static std::unique_ptr<AudioSharedMemory> open(const std::string& name);

private:
    std::unique_ptr<SharedMemory> shm_;
    uint32_t max_channels_;
    uint32_t max_samples_;
    bool is_host_;
};

/**
 * @brief GUI frame shared memory buffer
 * 
 * Layout:
 * - FrameBufferLayout header
 * - Pixel data (BGRA format)
 * 
 * Uses a ring buffer with atomic indices for lock-free access.
 */
class FrameSharedMemory {
public:
    /**
     * @brief Frame buffer slot
     */
    struct FrameSlot {
        std::atomic<uint32_t> state;      // 0 = empty, 1 = writing, 2 = ready
        uint32_t width;
        uint32_t height;
        uint32_t stride;
        // Pixel data follows
    };

    /**
     * @brief Ring buffer header
     */
    struct RingBuffer {
        static constexpr uint32_t kNumSlots = 3;  // Triple buffering
        
        std::atomic<uint32_t> write_index;   // Slot being written to
        std::atomic<uint32_t> read_index;    // Slot being read from
        std::atomic<uint32_t> frame_counter; // Total frames sent
        
        FrameSlot slots[kNumSlots];
    };

    /**
     * @brief Create frame shared memory
     * @param max_width Maximum frame width
     * @param max_height Maximum frame height
     * @param is_producer true if this side produces frames (Wine)
     */
    FrameSharedMemory(uint32_t max_width, uint32_t max_height, bool is_producer);
    
    ~FrameSharedMemory();

    // Non-copyable and non-movable
    FrameSharedMemory(const FrameSharedMemory&) = delete;
    FrameSharedMemory& operator=(const FrameSharedMemory&) = delete;
    FrameSharedMemory(FrameSharedMemory&&) = delete;
    FrameSharedMemory& operator=(FrameSharedMemory&&) = delete;

    /**
     * @brief Begin writing a frame
     * @param width Frame width
     * @param height Frame height
     * @return Pointer to pixel buffer, nullptr if no slot available
     */
    uint8_t* beginWrite(uint32_t width, uint32_t height);

    /**
     * @brief Finish writing frame
     */
    void endWrite();

    /**
     * @brief Begin reading a frame
     * @param width Output: frame width
     * @param height Output: frame height
     * @param stride Output: bytes per row
     * @return Pointer to pixel buffer, nullptr if no frame ready
     */
    const uint8_t* beginRead(uint32_t& width, uint32_t& height, uint32_t& stride);

    /**
     * @brief Finish reading frame
     */
    void endRead();

    /**
     * @brief Get shared memory name
     * @return Name
     */
    const std::string& getName() const { return shm_->getName(); }

    /**
     * @brief Check if a new frame is available
     * @return true if new frame ready
     */
    bool hasNewFrame() const;

    /**
     * @brief Wait for a frame to be available (with timeout)
     * @param timeout_ms Timeout in milliseconds
     * @return true if frame available
     */
    bool waitForFrame(int timeout_ms) const;

    /**
     * @brief Open existing frame shared memory
     * @param name Shared memory name
     * @return FrameSharedMemory object, nullptr on error
     */
    static std::unique_ptr<FrameSharedMemory> open(const std::string& name);

private:
    std::unique_ptr<SharedMemory> shm_;
    uint32_t max_width_;
    uint32_t max_height_;
    bool is_producer_;
    RingBuffer* ring_;
    uint8_t* pixel_data_;
    size_t slot_pixel_size_;
    int current_slot_ = -1;

    size_t getSlotOffset(uint32_t slot) const;
};

} // namespace vst3bridge
