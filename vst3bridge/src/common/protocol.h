// vst3bridge: A modern Wine VST3 bridge for Linux
// Copyright (C) 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <array>

namespace vst3bridge {

// ============================================================================
// Protocol Constants
// ============================================================================

constexpr uint32_t PROTOCOL_VERSION = 1;
constexpr uint32_t MAX_AUDIO_CHANNELS = 64;
constexpr uint32_t MAX_PARAMETERS = 65536;
constexpr uint32_t SOCKET_BUFFER_SIZE = 65536;

// ============================================================================
// Message Types
// ============================================================================

enum class MsgType : uint32_t {
    // Connection & Lifecycle (0-99)
    Handshake = 1,
    HandshakeResponse,
    Ping,
    Pong,
    Error,
    
    // Plugin Lifecycle (100-199)
    PluginInit = 100,
    PluginInitResponse,
    PluginTerminate,
    PluginTerminateResponse,
    
    // Audio Processing (200-299)
    AudioSetup = 200,
    AudioSetupResponse,
    AudioProcess,
    AudioProcessResponse,
    
    // Parameters (300-399)
    ParameterGet = 300,
    ParameterGetResponse,
    ParameterSet,
    ParameterSetResponse,
    ParameterChangeEvent,
    
    // GUI (400-499)
    GuiCreate = 400,
    GuiCreateResponse,
    GuiDestroy,
    GuiSetSize,
    GuiFrameUpdate,
    GuiMouseEvent,
    GuiKeyboardEvent,
    
    // Shared Memory (500-599)
    ShmCreate = 500,
    ShmCreateResponse,
    ShmDestroy,
};

// ============================================================================
// Base Message Structure
// ============================================================================

#pragma pack(push, 1)

struct MessageHeader {
    uint32_t type;       // Message type (MsgType)
    uint32_t size;       // Size of payload following header
    uint64_t timestamp;  // Microseconds since epoch
    uint64_t sequence;   // Message sequence number
    
    static constexpr size_t SIZE = 24;
};

// ============================================================================
// Connection Messages
// ============================================================================

struct HandshakePayload {
    uint32_t protocol_version;
    uint32_t flags;
    char plugin_path[512];
};

struct HandshakeResponsePayload {
    uint32_t status;  // 0 = success, non-zero = error code
    uint32_t host_version;
    char error_message[256];
};

// ============================================================================
// Plugin Messages
// ============================================================================

struct PluginInitPayload {
    uint32_t sample_rate;
    uint32_t max_block_size;
    uint32_t num_input_channels;
    uint32_t num_output_channels;
    char plugin_name[128];
    char vendor_name[128];
};

struct PluginInitResponsePayload {
    uint32_t status;
    uint32_t num_parameters;
    uint32_t flags;  // SupportsDoublePrecision, etc.
    char error_message[256];
};

// ============================================================================
// Audio Messages
// ============================================================================

struct AudioSetupPayload {
    uint32_t sample_rate;
    uint32_t block_size;
    uint32_t num_input_channels;
    uint32_t num_output_channels;
    uint32_t shm_size;  // Size of shared memory buffer
    char shm_name[64];  // Name of shared memory segment
};

struct AudioSetupResponsePayload {
    uint32_t status;
    uint32_t latency_samples;
    char error_message[256];
};

struct AudioProcessPayload {
    uint32_t num_samples;
    uint32_t flags;  // IsRealtime, etc.
    uint64_t timestamp;
};

// ============================================================================
// Parameter Messages
// ============================================================================

struct ParameterGetPayload {
    uint32_t param_id;
};

struct ParameterGetResponsePayload {
    uint32_t param_id;
    double value;
    char value_string[64];
    uint32_t status;
};

struct ParameterSetPayload {
    uint32_t param_id;
    double value;
    uint32_t flags;  // Immediate, etc.
};

// ============================================================================
// GUI Messages
// ============================================================================

struct GuiCreatePayload {
    uint64_t parent_window;  // X11 window ID on Linux side
    uint32_t width;
    uint32_t height;
    uint32_t capture_method;  // 0 = auto, 1 = GDI, 2 = OpenGL, 3 = Vulkan
};

struct GuiCreateResponsePayload {
    uint32_t status;
    uint32_t actual_width;
    uint32_t actual_height;
    uint32_t capture_method_used;
    char shm_name[64];  // Shared memory for frame buffer
    uint32_t shm_size;
};

struct GuiFrameUpdatePayload {
    uint32_t width;
    uint32_t height;
    uint32_t format;  // 0 = BGRA, 1 = RGBA
    uint32_t damage_x;
    uint32_t damage_y;
    uint32_t damage_width;
    uint32_t damage_height;
    uint64_t frame_number;
};

struct GuiMouseEventPayload {
    uint32_t type;  // 0 = move, 1 = down, 2 = up, 3 = double click
    int32_t x;
    int32_t y;
    uint32_t button;  // 0 = left, 1 = right, 2 = middle
    uint32_t modifiers;  // Shift, Ctrl, Alt, etc.
};

struct GuiKeyboardEventPayload {
    uint32_t type;  // 0 = down, 1 = up
    uint32_t key_code;
    uint32_t scan_code;
    uint32_t modifiers;
    char utf8_char[8];
};

// ============================================================================
// Shared Memory Messages
// ============================================================================

struct ShmCreatePayload {
    uint32_t size;
    char name[64];
    uint32_t purpose;  // 0 = audio, 1 = video/gui
};

struct ShmCreateResponsePayload {
    uint32_t status;
    char name[64];
    uint32_t actual_size;
};

#pragma pack(pop)

// ============================================================================
// Helper Functions
// ============================================================================

inline std::vector<uint8_t> SerializeMessage(MsgType type, const void* payload, size_t payload_size) {
    std::vector<uint8_t> buffer;
    buffer.resize(MessageHeader::SIZE + payload_size);
    
    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer.data());
    header->type = static_cast<uint32_t>(type);
    header->size = static_cast<uint32_t>(payload_size);
    header->timestamp = 0;  // TODO: Fill with actual timestamp
    header->sequence = 0;   // TODO: Fill with sequence number
    
    if (payload_size > 0 && payload != nullptr) {
        std::memcpy(buffer.data() + MessageHeader::SIZE, payload, payload_size);
    }
    
    return buffer;
}

template<typename T>
inline std::vector<uint8_t> SerializeMessage(MsgType type, const T& payload) {
    return SerializeMessage(type, &payload, sizeof(T));
}

} // namespace vst3bridge
