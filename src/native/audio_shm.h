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
 * @file audio_shm.h
 * @brief Native-side audio shared memory management declarations
 * 
 * Manages audio buffer exchange between the native DAW and Wine host.
 * Uses POSIX shared memory for zero-copy audio data transfer.
 */

#pragma once

#include <cstdint>
#include <memory>

namespace vst3bridge {

// Forward declarations
class AudioSharedMemory;

/**
 * @brief Audio buffer manager for native side
 * 
 * Wraps AudioSharedMemory with a simpler API for initialization and management
 */
class AudioBufferManager {
public:
    AudioBufferManager();
    ~AudioBufferManager();

    // Non-copyable and non-movable
    AudioBufferManager(const AudioBufferManager&) = delete;
    AudioBufferManager& operator=(const AudioBufferManager&) = delete;
    AudioBufferManager(AudioBufferManager&&) = delete;
    AudioBufferManager& operator=(AudioBufferManager&&) = delete;

    /**
     * @brief Set up audio shared memory
     * @param max_channels Maximum number of channels
     * @param max_samples Maximum samples per buffer
     * @return true on success
     */
    bool initialize(uint32_t max_channels, uint32_t max_samples);

    /**
     * @brief Check if audio shared memory is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Get the AudioSharedMemory instance
     * @return Pointer to AudioSharedMemory or nullptr
     */
    AudioSharedMemory* getSharedMemory();

    /**
     * @brief Get the AudioSharedMemory instance (const)
     * @return Const pointer to AudioSharedMemory or nullptr
     */
    const AudioSharedMemory* getSharedMemory() const;

    /**
     * @brief Get input channel buffer from shared memory
     * @param bus Audio bus index
     * @param channel Channel index within the bus
     * @return Pointer to float buffer or nullptr
     */
    float* getInputChannel(uint32_t bus, uint32_t channel);

    /**
     * @brief Get output channel buffer from shared memory
     * @param bus Audio bus index
     * @param channel Channel index within the bus
     * @return Pointer to float buffer or nullptr
     */
    float* getOutputChannel(uint32_t bus, uint32_t channel);

    /**
     * @brief Shutdown and cleanup audio shared memory
     */
    void shutdown();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

/**
 * @brief Initialize audio shared memory globally
 * @param max_channels Maximum number of channels
 * @param max_samples Maximum samples per buffer
 * @return true on success
 */
bool initializeAudioSharedMemory(uint32_t max_channels, uint32_t max_samples);

/**
 * @brief Get the global audio buffer manager
 * @return Pointer to the global AudioBufferManager instance
 */
AudioBufferManager* getAudioBufferManager();

/**
 * @brief Get the global audio buffer manager (const)
 * @return Const pointer to the global AudioBufferManager instance
 */
const AudioBufferManager* getAudioBufferManagerConst();

} // namespace vst3bridge
