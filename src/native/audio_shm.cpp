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
 * @file audio_shm.cpp
 * @brief Native-side audio shared memory management implementation
 * 
 * Manages audio buffer exchange between the native DAW and Wine host.
 * Uses POSIX shared memory for zero-copy audio data transfer.
 */

#include "audio_shm.h"
#include "shared_memory.h"
#include "protocol.h"
#include <cstring>

namespace vst3bridge {

// Pimpl implementation
class AudioBufferManager::Impl {
public:
    std::unique_ptr<AudioSharedMemory> audio_shm_;
};

AudioBufferManager::AudioBufferManager() : impl(std::make_unique<Impl>()) {}

AudioBufferManager::~AudioBufferManager() = default;

bool AudioBufferManager::initialize(uint32_t max_channels, uint32_t max_samples) {
    impl->audio_shm_ = std::make_unique<AudioSharedMemory>(max_channels, max_samples, false);
    return impl->audio_shm_ != nullptr;
}

bool AudioBufferManager::isInitialized() const {
    return impl->audio_shm_ != nullptr;
}

AudioSharedMemory* AudioBufferManager::getSharedMemory() {
    return impl->audio_shm_.get();
}

const AudioSharedMemory* AudioBufferManager::getSharedMemory() const {
    return impl->audio_shm_.get();
}

void AudioBufferManager::shutdown() {
    impl->audio_shm_.reset();
}

float* AudioBufferManager::getInputChannel(uint32_t bus, uint32_t channel) {
    if (impl->audio_shm_) {
        return impl->audio_shm_->getInputChannel(bus, channel);
    }
    return nullptr;
}

float* AudioBufferManager::getOutputChannel(uint32_t bus, uint32_t channel) {
    if (impl->audio_shm_) {
        return impl->audio_shm_->getOutputChannel(bus, channel);
    }
    return nullptr;
}

// Global audio buffer manager instance
static AudioBufferManager g_audio_manager;

bool initializeAudioSharedMemory(uint32_t max_channels, uint32_t max_samples) {
    return g_audio_manager.initialize(max_channels, max_samples);
}

AudioBufferManager* getAudioBufferManager() {
    return &g_audio_manager;
}

const AudioBufferManager* getAudioBufferManagerConst() {
    return &g_audio_manager;
}

} // namespace vst3bridge
