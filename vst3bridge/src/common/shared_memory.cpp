// vst3bridge: A modern Wine VST3 bridge for Linux
// Copyright (C) 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.
#include "shared_memory.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace vst3bridge {

// ============================================================================
// SharedMemory Implementation
// ============================================================================

SharedMemory::SharedMemory()
    : data_(nullptr)
    , size_(0)
    , fd_(-1)
    , owner_(false)
{
}

SharedMemory::~SharedMemory() {
    Close();
}

SharedMemory::SharedMemory(SharedMemory&& other) noexcept
    : name_(std::move(other.name_))
    , data_(other.data_)
    , size_(other.size_)
    , fd_(other.fd_)
    , last_error_(other.last_error_)
    , owner_(other.owner_)
{
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_ = -1;
    other.owner_ = false;
}

SharedMemory& SharedMemory::operator=(SharedMemory&& other) noexcept {
    if (this != &other) {
        Close();
        
        name_ = std::move(other.name_);
        data_ = other.data_;
        size_ = other.size_;
        fd_ = other.fd_;
        last_error_ = other.last_error_;
        owner_ = other.owner_;
        
        other.data_ = nullptr;
        other.size_ = 0;
        other.fd_ = -1;
        other.owner_ = false;
    }
    return *this;
}

bool SharedMemory::Open(const std::string& name, size_t size, Mode mode) {
    Close();
    
    name_ = name;
    size_ = size;
    
    // Ensure name starts with /
    std::string shm_name = name;
    if (shm_name.empty() || shm_name[0] != '/') {
        shm_name = "/" + shm_name;
    }
    
    int flags = 0;
    mode_t permissions = 0666;
    
    if (mode == Mode::Create) {
        flags = O_CREAT | O_RDWR | O_EXCL;
        owner_ = true;
    } else {
        flags = O_RDWR;
        owner_ = false;
    }
    
    // Open shared memory object
    fd_ = shm_open(shm_name.c_str(), flags, permissions);
    if (fd_ < 0) {
        if (mode == Mode::Create && errno == EEXIST) {
            // Already exists, try opening
            owner_ = false;
            fd_ = shm_open(shm_name.c_str(), O_RDWR, permissions);
        }
        
        if (fd_ < 0) {
            SetError(errno);
            return false;
        }
    }
    
    // Set size if creating
    if (mode == Mode::Create && owner_) {
        if (ftruncate(fd_, static_cast<off_t>(size)) < 0) {
            SetError(errno);
            ::close(fd_);
            fd_ = -1;
            if (owner_) {
                shm_unlink(shm_name.c_str());
            }
            return false;
        }
    }
    
    // Map the shared memory
    data_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (data_ == MAP_FAILED) {
        SetError(errno);
        data_ = nullptr;
        ::close(fd_);
        fd_ = -1;
        if (owner_) {
            shm_unlink(shm_name.c_str());
        }
        return false;
    }
    
    return true;
}

void SharedMemory::Close() {
    if (data_ != nullptr && data_ != MAP_FAILED) {
        munmap(data_, size_);
        data_ = nullptr;
    }
    
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    
    if (owner_ && !name_.empty()) {
        std::string shm_name = name_;
        if (shm_name[0] != '/') {
            shm_name = "/" + shm_name;
        }
        shm_unlink(shm_name.c_str());
        owner_ = false;
    }
    
    size_ = 0;
}

void SharedMemory::SetError(int errno_code) {
    last_error_ = std::error_code(errno_code, std::system_category());
}

// ============================================================================
// AudioSharedMemory Implementation
// ============================================================================

AudioSharedMemory::AudioSharedMemory()
    : header_(nullptr)
    , input_channels_(nullptr)
    , output_channels_(nullptr)
    , num_channels_(0)
    , max_block_size_(0)
{
}

AudioSharedMemory::~AudioSharedMemory() {
    Close();
}

AudioSharedMemory::AudioSharedMemory(AudioSharedMemory&& other) noexcept
    : shm_(std::move(other.shm_))
    , header_(other.header_)
    , input_channels_(other.input_channels_)
    , output_channels_(other.output_channels_)
    , num_channels_(other.num_channels_)
    , max_block_size_(other.max_block_size_)
{
    other.header_ = nullptr;
    other.input_channels_ = nullptr;
    other.output_channels_ = nullptr;
    other.num_channels_ = 0;
    other.max_block_size_ = 0;
}

AudioSharedMemory& AudioSharedMemory::operator=(AudioSharedMemory&& other) noexcept {
    if (this != &other) {
        Close();
        
        shm_ = std::move(other.shm_);
        header_ = other.header_;
        input_channels_ = other.input_channels_;
        output_channels_ = other.output_channels_;
        num_channels_ = other.num_channels_;
        max_block_size_ = other.max_block_size_;
        
        other.header_ = nullptr;
        other.input_channels_ = nullptr;
        other.output_channels_ = nullptr;
        other.num_channels_ = 0;
        other.max_block_size_ = 0;
    }
    return *this;
}

bool AudioSharedMemory::Initialize(const std::string& name, uint32_t num_channels, uint32_t max_block_size) {
    Close();
    
    num_channels_ = num_channels;
    max_block_size_ = max_block_size;
    
    size_t total_size = CalculateSize(num_channels, max_block_size);
    
    if (!shm_.Open(name, total_size, SharedMemory::Mode::Create)) {
        return false;
    }
    
    header_ = static_cast<AudioBufferHeader*>(shm_.Data());
    
    // Initialize header
    header_->magic = AUDIO_BUFFER_MAGIC;
    header_->version = AUDIO_BUFFER_VERSION;
    header_->sample_rate = 0;
    header_->max_block_size = max_block_size;
    header_->num_input_channels = num_channels;
    header_->num_output_channels = num_channels;
    header_->current_block_size = 0;
    header_->timestamp = 0;
    header_->flags = AUDIO_BUFFER_FLAG_READY;
    
    // Calculate channel pointer arrays
    uint8_t* data = static_cast<uint8_t*>(shm_.Data());
    size_t header_size = sizeof(AudioBufferHeader);
    size_t pointer_array_size = num_channels * sizeof(float*);
    size_t channel_data_size = max_block_size * sizeof(float);
    
    // Input channel pointers start after header
    input_channels_ = reinterpret_cast<float**>(data + header_size);
    
    // Output channel pointers start after input pointers
    output_channels_ = reinterpret_cast<float**>(data + header_size + pointer_array_size);
    
    // Channel data starts after both pointer arrays
    size_t channel_data_offset = header_size + 2 * pointer_array_size;
    
    // Initialize channel pointers
    for (uint32_t i = 0; i < num_channels; ++i) {
        input_channels_[i] = reinterpret_cast<float*>(data + channel_data_offset + i * channel_data_size);
        output_channels_[i] = reinterpret_cast<float*>(data + channel_data_offset + (num_channels + i) * channel_data_size);
    }
    
    // Zero initialize all audio data
    for (uint32_t i = 0; i < num_channels; ++i) {
        std::memset(input_channels_[i], 0, channel_data_size);
        std::memset(output_channels_[i], 0, channel_data_size);
    }
    
    return true;
}

bool AudioSharedMemory::Open(const std::string& name) {
    Close();
    
    if (!shm_.Open(name, 0, SharedMemory::Mode::Open)) {
        return false;
    }
    
    header_ = static_cast<AudioBufferHeader*>(shm_.Data());
    
    // Validate header
    if (header_->magic != AUDIO_BUFFER_MAGIC) {
        Close();
        return false;
    }
    
    num_channels_ = header_->num_input_channels;
    max_block_size_ = header_->max_block_size;
    
    // Calculate channel pointers
    uint8_t* data = static_cast<uint8_t*>(shm_.Data());
    size_t header_size = sizeof(AudioBufferHeader);
    size_t pointer_array_size = num_channels_ * sizeof(float*);
    
    input_channels_ = reinterpret_cast<float**>(data + header_size);
    output_channels_ = reinterpret_cast<float**>(data + header_size + pointer_array_size);
    
    return true;
}

void AudioSharedMemory::Close() {
    header_ = nullptr;
    input_channels_ = nullptr;
    output_channels_ = nullptr;
    num_channels_ = 0;
    max_block_size_ = 0;
    shm_.Close();
}

float* AudioSharedMemory::GetInputChannel(uint32_t channel) {
    if (channel >= num_channels_ || !input_channels_) {
        return nullptr;
    }
    return input_channels_[channel];
}

float* AudioSharedMemory::GetOutputChannel(uint32_t channel) {
    if (channel >= num_channels_ || !output_channels_) {
        return nullptr;
    }
    return output_channels_[channel];
}

const float* AudioSharedMemory::GetInputChannel(uint32_t channel) const {
    if (channel >= num_channels_ || !input_channels_) {
        return nullptr;
    }
    return input_channels_[channel];
}

const float* AudioSharedMemory::GetOutputChannel(uint32_t channel) const {
    if (channel >= num_channels_ || !output_channels_) {
        return nullptr;
    }
    return output_channels_[channel];
}

size_t AudioSharedMemory::CalculateSize(uint32_t num_channels, uint32_t max_block_size) {
    size_t header_size = sizeof(AudioBufferHeader);
    size_t pointer_array_size = num_channels * sizeof(float*);
    size_t channel_data_size = max_block_size * sizeof(float);
    
    // Header + input pointers + output pointers + input data + output data
    return header_size + (2 * pointer_array_size) + (2 * num_channels * channel_data_size);
}

// ============================================================================
// FrameSharedMemory Implementation
// ============================================================================

FrameSharedMemory::FrameSharedMemory()
    : header_(nullptr)
    , pixels_(nullptr)
{
}

FrameSharedMemory::~FrameSharedMemory() {
    Close();
}

FrameSharedMemory::FrameSharedMemory(FrameSharedMemory&& other) noexcept
    : shm_(std::move(other.shm_))
    , header_(other.header_)
    , pixels_(other.pixels_)
{
    other.header_ = nullptr;
    other.pixels_ = nullptr;
}

FrameSharedMemory& FrameSharedMemory::operator=(FrameSharedMemory&& other) noexcept {
    if (this != &other) {
        Close();
        
        shm_ = std::move(other.shm_);
        header_ = other.header_;
        pixels_ = other.pixels_;
        
        other.header_ = nullptr;
        other.pixels_ = nullptr;
    }
    return *this;
}

bool FrameSharedMemory::Initialize(const std::string& name, uint32_t width, uint32_t height, uint32_t format) {
    Close();
    
    // Calculate size: header + pixel data
    // Format 0 = BGRA (4 bytes per pixel), 1 = RGBA (4 bytes per pixel)
    uint32_t bytes_per_pixel = 4;
    uint32_t stride = width * bytes_per_pixel;
    // Align stride to 4 bytes
    stride = (stride + 3) & ~3;
    size_t pixel_data_size = stride * height;
    size_t total_size = sizeof(FrameBufferHeader) + pixel_data_size;
    
    if (!shm_.Open(name, total_size, SharedMemory::Mode::Create)) {
        return false;
    }
    
    header_ = static_cast<FrameBufferHeader*>(shm_.Data());
    pixels_ = static_cast<uint8_t*>(shm_.Data()) + sizeof(FrameBufferHeader);
    
    // Initialize header
    header_->magic = FRAME_BUFFER_MAGIC;
    header_->version = FRAME_BUFFER_VERSION;
    header_->width = width;
    header_->height = height;
    header_->format = format;
    header_->stride = stride;
    header_->frame_number = 0;
    header_->timestamp = 0;
    header_->flags = FRAME_BUFFER_FLAG_READY;
    
    return true;
}

bool FrameSharedMemory::Open(const std::string& name) {
    Close();
    
    // Open with size 0 - we'll determine size from header
    if (!shm_.Open(name, 0, SharedMemory::Mode::Open)) {
        return false;
    }
    
    header_ = static_cast<FrameBufferHeader*>(shm_.Data());
    
    // Validate header
    if (header_->magic != FRAME_BUFFER_MAGIC) {
        Close();
        return false;
    }
    
    pixels_ = static_cast<uint8_t*>(shm_.Data()) + sizeof(FrameBufferHeader);
    
    return true;
}

bool FrameSharedMemory::Resize(uint32_t width, uint32_t height) {
    if (!IsValid()) {
        return false;
    }
    
    std::string name = shm_.Name();
    uint32_t format = header_->format;
    
    Close();
    
    return Initialize(name, width, height, format);
}

void FrameSharedMemory::Close() {
    header_ = nullptr;
    pixels_ = nullptr;
    shm_.Close();
}

void FrameSharedMemory::MarkUpdated(uint64_t frame_number) {
    if (header_) {
        header_->frame_number = frame_number;
        header_->timestamp = 0;  // TODO: Get actual timestamp
        header_->flags |= FRAME_BUFFER_FLAG_UPDATED;
    }
}

bool FrameSharedMemory::IsNewFrame(uint64_t last_frame) const {
    if (!header_) {
        return false;
    }
    return (header_->flags & FRAME_BUFFER_FLAG_UPDATED) && header_->frame_number > last_frame;
}

} // namespace vst3bridge
