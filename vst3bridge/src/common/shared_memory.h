// vst3bridge: A modern Wine VST3 bridge for Linux
// Copyright (C) 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <system_error>

namespace vst3bridge {

// ============================================================================
// Shared Memory Buffer
// ============================================================================
// 
// Provides zero-copy shared memory for audio buffers and GUI frame data.
// Uses POSIX shared memory (shm_open) on Linux side.
// Wine side accesses the same memory via the Windows shared memory API
// which Wine maps to POSIX shared memory internally.

class SharedMemory {
public:
    enum class Mode {
        Create,     // Create new shared memory segment
        Open        // Open existing shared memory segment
    };

    SharedMemory();
    ~SharedMemory();

    // Non-copyable
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Movable
    SharedMemory(SharedMemory&& other) noexcept;
    SharedMemory& operator=(SharedMemory&& other) noexcept;

    // Create or open a shared memory segment
    // Returns true on success, false on failure
    bool Open(const std::string& name, size_t size, Mode mode);
    
    // Close the shared memory segment
    void Close();

    // Check if the shared memory is valid
    bool IsValid() const { return data_ != nullptr && size_ > 0; }

    // Get pointer to shared memory data
    void* Data() { return data_; }
    const void* Data() const { return data_; }

    // Get size of shared memory
    size_t Size() const { return size_; }

    // Get name of shared memory
    const std::string& Name() const { return name_; }

    // Get last error
    std::error_code GetLastError() const { return last_error_; }

private:
    std::string name_;
    void* data_;
    size_t size_;
    int fd_;  // File descriptor (Linux side)
    std::error_code last_error_;
    bool owner_;  // True if we created (and should unlink) the segment
};

// ============================================================================
// Audio Buffer Layout
// ============================================================================
// 
// Layout of audio data in shared memory:
// [Header][Channel 0][Channel 1]...[Channel N]
//
// Each channel is a contiguous array of float samples.

#pragma pack(push, 1)

struct AudioBufferHeader {
    uint32_t magic;           // 'VST3' = 0x56535433
    uint32_t version;         // Structure version
    uint32_t sample_rate;
    uint32_t max_block_size;
    uint32_t num_input_channels;
    uint32_t num_output_channels;
    uint32_t current_block_size;
    uint64_t timestamp;
    uint32_t flags;           // Flags for buffer state
};

#pragma pack(pop)

constexpr uint32_t AUDIO_BUFFER_MAGIC = 0x56535433;  // 'VST3'
constexpr uint32_t AUDIO_BUFFER_VERSION = 1;
constexpr uint32_t AUDIO_BUFFER_FLAG_READY = 1;
constexpr uint32_t AUDIO_BUFFER_FLAG_PROCESSING = 2;

class AudioSharedMemory {
public:
    AudioSharedMemory();
    ~AudioSharedMemory();

    // Non-copyable, movable
    AudioSharedMemory(const AudioSharedMemory&) = delete;
    AudioSharedMemory& operator=(const AudioSharedMemory&) = delete;
    AudioSharedMemory(AudioSharedMemory&&) noexcept;
    AudioSharedMemory& operator=(AudioSharedMemory&&) noexcept;

    // Initialize audio shared memory
    // num_channels: Maximum number of audio channels
    // max_block_size: Maximum block size (samples per channel)
    bool Initialize(const std::string& name, uint32_t num_channels, uint32_t max_block_size);
    
    // Open existing audio shared memory
    bool Open(const std::string& name);

    // Close and cleanup
    void Close();

    // Get pointers to channel data
    float* GetInputChannel(uint32_t channel);
    float* GetOutputChannel(uint32_t channel);
    const float* GetInputChannel(uint32_t channel) const;
    const float* GetOutputChannel(uint32_t channel) const;

    // Get header
    AudioBufferHeader* GetHeader() { return header_; }
    const AudioBufferHeader* GetHeader() const { return header_; }

    // Check if valid
    bool IsValid() const { return shm_.IsValid() && header_ != nullptr; }

    // Calculate required size
    static size_t CalculateSize(uint32_t num_channels, uint32_t max_block_size);

private:
    SharedMemory shm_;
    AudioBufferHeader* header_;
    float** input_channels_;
    float** output_channels_;
    uint32_t num_channels_;
    uint32_t max_block_size_;
};

// ============================================================================
// GUI Frame Buffer Layout
// ============================================================================
// 
// Layout for GUI frame data in shared memory:
// [Header][Pixel Data]

#pragma pack(push, 1)

struct FrameBufferHeader {
    uint32_t magic;           // 'GUIB' = 0x47554942
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t format;          // 0 = BGRA, 1 = RGBA
    uint32_t stride;          // Bytes per row
    uint64_t frame_number;
    uint64_t timestamp;
    uint32_t flags;           // 0 = ready, 1 = updated, 2 = resize needed
};

#pragma pack(pop)

constexpr uint32_t FRAME_BUFFER_MAGIC = 0x47554942;  // 'GUIB'
constexpr uint32_t FRAME_BUFFER_VERSION = 1;
constexpr uint32_t FRAME_BUFFER_FLAG_READY = 1;
constexpr uint32_t FRAME_BUFFER_FLAG_UPDATED = 2;
constexpr uint32_t FRAME_BUFFER_FLAG_RESIZE = 4;

class FrameSharedMemory {
public:
    FrameSharedMemory();
    ~FrameSharedMemory();

    // Non-copyable, movable
    FrameSharedMemory(const FrameSharedMemory&) = delete;
    FrameSharedMemory& operator=(const FrameSharedMemory&) = delete;
    FrameSharedMemory(FrameSharedMemory&&) noexcept;
    FrameSharedMemory& operator=(FrameSharedMemory&&) noexcept;

    // Initialize frame buffer
    bool Initialize(const std::string& name, uint32_t width, uint32_t height, uint32_t format);
    
    // Open existing frame buffer
    bool Open(const std::string& name);

    // Resize buffer (must re-initialize)
    bool Resize(uint32_t width, uint32_t height);

    // Close
    void Close();

    // Get pixel data
    void* GetPixels() { return pixels_; }
    const void* GetPixels() const { return pixels_; }

    // Get header
    FrameBufferHeader* GetHeader() { return header_; }
    const FrameBufferHeader* GetHeader() const { return header_; }

    // Get dimensions
    uint32_t GetWidth() const { return header_ ? header_->width : 0; }
    uint32_t GetHeight() const { return header_ ? header_->height : 0; }
    uint32_t GetStride() const { return header_ ? header_->stride : 0; }

    // Check if valid
    bool IsValid() const { return shm_.IsValid() && header_ != nullptr; }

    // Mark frame as updated
    void MarkUpdated(uint64_t frame_number);

    // Check if new frame available
    bool IsNewFrame(uint64_t last_frame) const;

private:
    SharedMemory shm_;
    FrameBufferHeader* header_;
    void* pixels_;
};

} // namespace vst3bridge
