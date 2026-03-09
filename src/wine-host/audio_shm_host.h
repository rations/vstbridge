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
 * @file audio_shm_host.h
 * @brief Wine-side audio shared memory host
 */

#pragma once

#include <cstdint>
#include "protocol.h"

namespace vst3bridge {

/**
 * @brief Audio shared memory host for Wine side
 */
class AudioSharedMemoryHost {
public:
    AudioSharedMemoryHost();
    ~AudioSharedMemoryHost();
    
    /**
     * @brief Initialize the audio shared memory host
     * @param name Shared memory name
     * @return true on success
     */
    bool initialize(const char* name);

    // Non-copyable
    AudioSharedMemoryHost(const AudioSharedMemoryHost&) = delete;
    AudioSharedMemoryHost& operator=(const AudioSharedMemoryHost&) = delete;

    /**
     * @brief Open shared memory
     * @param name Shared memory name
     * @return true on success
     */
    bool open(const char* name);

    /**
     * @brief Close shared memory
     */
    void close();
    
    /**
     * @brief Shutdown the audio shared memory host
     */
    void shutdown();

    /**
     * @brief Get input buffer
     * @param channel Channel index
     * @return Pointer to buffer or nullptr
     */
    float* getInputBuffer(int channel);

    /**
     * @brief Get output buffer
     * @param channel Channel index
     * @return Pointer to buffer or nullptr
     */
    float* getOutputBuffer(int channel);

    /**
     * @brief Get number of input channels
     * @return Number of inputs
     */
    uint32_t getNumInputs() const;

    /**
     * @brief Get number of output channels
     * @return Number of outputs
     */
    uint32_t getNumOutputs() const;

    /**
     * @brief Get number of samples
     * @return Number of samples
     */
    uint32_t getNumSamples() const;

    /**
     * @brief Set channel counts
     * @param num_inputs Number of inputs
     * @param num_outputs Number of outputs
     */
    void setChannelCounts(uint32_t num_inputs, uint32_t num_outputs);

    /**
     * @brief Set number of samples
     * @param num_samples Number of samples
     */
    void setNumSamples(uint32_t num_samples);

private:
    void* data_ = nullptr;
    void* mapping_handle_ = nullptr;
};

} // namespace vst3bridge
