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
 * @file audio_processor.cpp
 * @brief Audio processing loop implementation for Wine host
 */

#include "audio_processor.h"
#include <cstring>
#include <iostream>

namespace vst3bridge {

AudioProcessor::AudioProcessor(std::shared_ptr<AudioSharedMemoryHost> audio_shm,
                               std::shared_ptr<IpcHost> ipc,
                               IAudioProcessor* processor,
                               IEditController* controller)
    : audio_shm_(audio_shm)
    , ipc_(ipc)
    , processor_(processor)
    , controller_(controller) {
}

AudioProcessor::~AudioProcessor() {
    stop();
    
    // Release parameter change interfaces
    if (input_param_changes_) {
        input_param_changes_->release();
        input_param_changes_ = nullptr;
    }
    if (output_param_changes_) {
        output_param_changes_->release();
        output_param_changes_ = nullptr;
    }
}

bool AudioProcessor::start() {
    if (running_.load()) {
        return true;
    }

    stop_requested_ = false;
    running_ = true;

    processing_thread_ = std::thread(&AudioProcessor::processingLoop, this);
    return true;
}

void AudioProcessor::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_ = true;
    
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }

    running_ = false;
}

bool AudioProcessor::setupBuses(int32 input_bus_count, int32 output_bus_count) {
    if (!processor_) {
        return false;
    }

    // Initialize process data
    std::memset(&process_data_, 0, sizeof(process_data_));
    process_data_.processMode = kRealtime;
    process_data_.symbolicSampleSize = kSample32;
    process_data_.numInputs = input_bus_count;
    process_data_.numOutputs = output_bus_count;

    // Set up input buses
    input_buses_.resize(input_bus_count);
    input_channel_ptrs_.resize(input_bus_count);
    input_scratch_.resize(input_bus_count);

    for (int32 bus = 0; bus < input_bus_count; bus++) {
        SpeakerArrangement arrangement;
        if (processor_->getBusArrangement(kInput, bus, arrangement) != kResultTrue) {
            arrangement = kStereo;  // Default to stereo
        }

        int32 channel_count = SpeakerArr::getChannelCount(arrangement);
        if (channel_count == 0) {
            channel_count = 2;  // Default to stereo
        }

        input_buses_[bus].numChannels = channel_count;
        input_buses_[bus].silenceFlags = 0;
        input_buses_[bus].channelBuffers32 = nullptr;

        input_channel_ptrs_[bus].resize(channel_count);
        input_scratch_[bus].resize(channel_count * 8192);  // Max buffer size
    }

    // Set up output buses
    output_buses_.resize(output_bus_count);
    output_channel_ptrs_.resize(output_bus_count);
    output_scratch_.resize(output_bus_count);

    for (int32 bus = 0; bus < output_bus_count; bus++) {
        SpeakerArrangement arrangement;
        if (processor_->getBusArrangement(kOutput, bus, arrangement) != kResultTrue) {
            arrangement = kStereo;  // Default to stereo
        }

        int32 channel_count = SpeakerArr::getChannelCount(arrangement);
        if (channel_count == 0) {
            channel_count = 2;  // Default to stereo
        }

        output_buses_[bus].numChannels = channel_count;
        output_buses_[bus].silenceFlags = 0;
        output_buses_[bus].channelBuffers32 = nullptr;

        output_channel_ptrs_[bus].resize(channel_count);
        output_scratch_[bus].resize(channel_count * 8192);  // Max buffer size
    }

    process_data_.inputs = input_buses_.data();
    process_data_.outputs = output_buses_.data();

    // Create parameter change queues using proper VST3 interfaces
    // These implement IParameterChanges and IParamValueQueue
    if (!input_param_changes_) {
        input_param_changes_ = new ParameterChanges();
        // Initial addRef since we'll be holding onto this
        input_param_changes_->addRef();
    }
    if (!output_param_changes_) {
        output_param_changes_ = new ParameterChanges();
        output_param_changes_->addRef();
    }
    
    // ProcessData takes IParameterChanges* (proper VST3 interface)
    process_data_.inputParameterChanges = input_param_changes_;
    process_data_.outputParameterChanges = output_param_changes_;

    return true;
}

void AudioProcessor::processingLoop() {
    // Send AudioReady message to indicate we're ready
    MsgAudioReady audio_ready;
    audio_ready.input_bus_count = process_data_.numInputs;
    audio_ready.output_bus_count = process_data_.numOutputs;
    
    // Count total channels per bus
    for (int32 bus = 0; bus < process_data_.numInputs && bus < kMaxBuses; bus++) {
        audio_ready.input_bus_channels[bus] = input_buses_[bus].numChannels;
    }
    for (int32 bus = 0; bus < process_data_.numOutputs && bus < kMaxBuses; bus++) {
        audio_ready.output_bus_channels[bus] = output_buses_[bus].numChannels;
    }

    ipc_->sendMessage(MsgType::AudioReady, &audio_ready, sizeof(audio_ready));

    // Main processing loop
    while (!stop_requested_.load()) {
        // Wait for AudioProcess message from native side
        GenericMessage msg;
        if (!ipc_->receiveMessage(msg, 100)) {  // 100ms timeout
            continue;
        }

        if (msg.header.type != MsgType::AudioProcess) {
            continue;
        }

        const auto* process_msg = reinterpret_cast<const MsgAudioProcess*>(msg.payload.data());
        uint32_t num_samples = process_msg->num_samples;

        if (num_samples == 0 || num_samples > 8192) {
            // Invalid sample count
            MsgProcessComplete complete;
            complete.success = false;
            complete.output_samples = 0;
            ipc_->sendMessage(MsgType::ProcessComplete, &complete, sizeof(complete));
            continue;
        }

        // Process the audio buffer
        bool success = processBuffer(num_samples);

        // Send completion message
        MsgProcessComplete complete;
        complete.success = success;
        complete.output_samples = success ? num_samples : 0;
        ipc_->sendMessage(MsgType::ProcessComplete, &complete, sizeof(complete));
    }
}

bool AudioProcessor::processBuffer(uint32_t num_samples) {
    if (!processor_) {
        return false;
    }

    // Update process data
    process_data_.numSamples = num_samples;

    // Copy input audio from shared memory
    copyFromSharedMemory(num_samples);

    // Set up channel buffer pointers
    for (size_t bus = 0; bus < input_buses_.size(); bus++) {
        for (int32 ch = 0; ch < input_buses_[bus].numChannels; ch++) {
            input_channel_ptrs_[bus][ch] = input_scratch_[bus].data() + ch * num_samples;
        }
        input_buses_[bus].channelBuffers32 = input_channel_ptrs_[bus].data();
    }

    for (size_t bus = 0; bus < output_buses_.size(); bus++) {
        for (int32 ch = 0; ch < output_buses_[bus].numChannels; ch++) {
            output_channel_ptrs_[bus][ch] = output_scratch_[bus].data() + ch * num_samples;
        }
        output_buses_[bus].channelBuffers32 = output_channel_ptrs_[bus].data();
    }

    // Update parameters
    updateParameters();

    // Process audio
    tresult result = processor_->process(process_data_);

    // Copy output audio to shared memory
    if (result == kResultTrue) {
        copyToSharedMemory(num_samples);
    }

    return result == kResultTrue;
}

void AudioProcessor::copyFromSharedMemory(uint32_t num_samples) {
    if (!audio_shm_) return;

    // Copy input audio from shared memory to scratch buffers
    for (size_t bus = 0; bus < input_buses_.size(); bus++) {
        for (int32 ch = 0; ch < input_buses_[bus].numChannels; ch++) {
            float* dst = input_scratch_[bus].data() + ch * num_samples;
            float* src = audio_shm_->getInputChannel(static_cast<uint32_t>(bus), static_cast<uint32_t>(ch));
            
            if (src) {
                std::memcpy(dst, src, num_samples * sizeof(float));
            } else {
                std::memset(dst, 0, num_samples * sizeof(float));
            }
        }
    }
}

void AudioProcessor::copyToSharedMemory(uint32_t num_samples) {
    if (!audio_shm_) return;

    // Copy output audio from scratch buffers to shared memory
    for (size_t bus = 0; bus < output_buses_.size(); bus++) {
        for (int32 ch = 0; ch < output_buses_[bus].numChannels; ch++) {
            float* src = output_scratch_[bus].data() + ch * num_samples;
            float* dst = audio_shm_->getOutputChannel(static_cast<uint32_t>(bus), static_cast<uint32_t>(ch));
            
            if (dst) {
                std::memcpy(dst, src, num_samples * sizeof(float));
            }
        }
    }
}

void AudioProcessor::updateParameters() {
    if (!controller_ || !input_param_changes_) return;

    // Clear previous parameter changes from the last processing block
    input_param_changes_->clear();

    // Build new parameter changes from IPC data
    // TODO: Receive parameter changes from native side via IPC
    // The param_builder_ accumulates changes which are then transferred
    // to the input_param_changes_ interface
    
    // Example of how to add parameter changes:
    // param_builder_.addChange(paramId, sampleOffset, normalizedValue);
    
    // When parameter changes are received via IPC, they should be added
    // using param_builder_.addChange(), then committed with build()
    // ParameterChanges* changes = param_builder_.build();
    // if (changes) {
    //     // Transfer changes to input_param_changes_
    //     // ... copy data ...
    //     delete changes;
    // }
    // param_builder_.clear();
}

} // namespace vst3bridge
