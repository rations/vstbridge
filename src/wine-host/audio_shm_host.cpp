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
 * @file audio_shm_host.cpp
 * @brief Wine-side audio shared memory host
 * 
 * Manages audio buffer exchange on the Wine/Windows side.
 * Uses memory-mapped files for compatibility with POSIX shared memory.
 */

#include "audio_shm_host.h"
#include <windows.h>
#include <cstring>
#include <string>

namespace vst3bridge {

AudioSharedMemoryHost::AudioSharedMemoryHost() = default;

AudioSharedMemoryHost::~AudioSharedMemoryHost() {
    close();
}

bool AudioSharedMemoryHost::initialize(const char* name) {
    return open(name);
}
bool AudioSharedMemoryHost::open(const char* name) {
    if (!name) return false;
    
    close();
    
    // On Windows/Wine, we use a file mapping to simulate POSIX shared memory
    // The name should be formatted as: "Global\\vst3bridge_audio_XXX"
    std::wstring wide_name;
    int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (len > 0) {
        wide_name.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, name, -1, &wide_name[0], len);
    }
    
    // Open file mapping
    HANDLE handle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wide_name.c_str());
    if (!handle) {
        return false;
    }
    
    // Map view of file
    void* data = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!data) {
        CloseHandle(handle);
        return false;
    }
    
    data_ = data;
    mapping_handle_ = handle;
    
    return true;
}

void AudioSharedMemoryHost::close() {
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    
    if (mapping_handle_) {
        CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
}

float* AudioSharedMemoryHost::getInputBuffer(int channel) {
    if (!data_) return nullptr;
    
    // Access the layout structure
    auto* layout = static_cast<AudioBufferLayout*>(data_);
    
    if (channel < 0 || channel >= static_cast<int>(layout->num_input_buses)) {
        return nullptr;
    }
    
    uint8_t* base = static_cast<uint8_t*>(data_);
    return reinterpret_cast<float*>(base + layout->input_offsets[channel]);
}

float* AudioSharedMemoryHost::getOutputBuffer(int channel) {
    if (!data_) return nullptr;
    
    // Access the layout structure
    auto* layout = static_cast<AudioBufferLayout*>(data_);
    
    if (channel < 0 || channel >= static_cast<int>(layout->num_output_buses)) {
        return nullptr;
    }
    
    uint8_t* base = static_cast<uint8_t*>(data_);
    return reinterpret_cast<float*>(base + layout->output_offsets[channel]);
}

uint32_t AudioSharedMemoryHost::getNumInputs() const {
    if (!data_) return 0;
    auto* layout = static_cast<AudioBufferLayout*>(data_);
    return layout->num_input_buses;
}

uint32_t AudioSharedMemoryHost::getNumOutputs() const {
    if (!data_) return 0;
    auto* layout = static_cast<AudioBufferLayout*>(data_);
    return layout->num_output_buses;
}

uint32_t AudioSharedMemoryHost::getNumSamples() const {
    if (!data_) return 0;
    auto* layout = static_cast<AudioBufferLayout*>(data_);
    return layout->num_samples;
}

void AudioSharedMemoryHost::setChannelCounts(uint32_t num_inputs, uint32_t num_outputs) {
    if (!data_) return;
    auto* layout = static_cast<AudioBufferLayout*>(data_);
    layout->num_input_buses = num_inputs;
    layout->num_output_buses = num_outputs;
}

void AudioSharedMemoryHost::setNumSamples(uint32_t num_samples) {
    if (!data_) return;
    auto* layout = static_cast<AudioBufferLayout*>(data_);
    layout->num_samples = num_samples;
}

float* AudioSharedMemoryHost::getInputChannel(uint32_t bus, uint32_t ch) {
    // TODO: Implement proper bus/channel mapping
    (void)bus;
    return getInputBuffer(static_cast<int>(ch));
}

float* AudioSharedMemoryHost::getOutputChannel(uint32_t bus, uint32_t ch) {
    // TODO: Implement proper bus/channel mapping
    (void)bus;
    return getOutputBuffer(static_cast<int>(ch));
}

void AudioSharedMemoryHost::shutdown() {
    close();
}

} // namespace vst3bridge
