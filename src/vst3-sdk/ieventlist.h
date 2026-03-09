/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 */

#pragma once

#include "funknown.h"

namespace Steinberg {

// Forward declaration
struct Event;

/**
 * @brief Event list interface for MIDI/input events
 */
class IEventList : public FUnknown {
public:
    /**
     * @brief Get number of events in list
     * @return Number of events
     */
    virtual int32 PLUGIN_API getEventCount() = 0;

    /**
     * @brief Get event at index
     * @param index Event index
     * @param e Output event
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API getEvent(int32 index, Event& e) = 0;

    /**
     * @brief Add an event
     * @param e Event to add
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API addEvent(Event& e) = 0;

    static const FUID iid;
};

/**
 * @brief Event types
 */
enum EventTypes {
    kNoteOnEvent = 0,
    kNoteOffEvent = 1,
    kDataEvent = 2,
    kPolyPressureEvent = 3,
    kChokeEvent = 4,
    kScaleEvent = 5,
    kLegacyMIDICCOutEvent = 65535
};

/**
 * @brief Event structure
 */
struct Event {
    int32 busIndex;       ///< Bus index
    int32 sampleOffset;   ///< Sample offset
    double ppqPosition;   ///< Position in quarter notes
    uint16 flags;         ///< Event flags
    uint16 type;          ///< Event type (EventTypes)

    union {
        // Note on event
        struct {
            int16 channel;
            int16 pitch;
            float tuning;
            float velocity;
            int32 length;
            int32 noteId;
        } noteOn;

        // Note off event
        struct {
            int16 channel;
            int16 pitch;
            float velocity;
            int32 noteId;
            float tuning;
        } noteOff;

        // Data event (SysEx, etc.)
        struct {
            uint32 size;
            const uint8* bytes;
        } data;

        // Polyphonic pressure
        struct {
            int16 channel;
            int16 pitch;
            float pressure;
            int32 noteId;
        } polyPressure;

        // Choke event
        struct {
            int16 channel;
            int16 pitch;
            int32 noteId;
        } choke;

        // Scale event
        struct {
            int16 root;
            int16 mask;
            uint8 reserved[4];
        } scale;

        // Legacy MIDI CC event
        struct {
            uint8 controlNumber;
            int8 channel;
            int8 value;
            int8 value2;
        } midiCCOut;
    };

    // Event flags
    static constexpr uint16 kIsLive = 1 << 0;
    static constexpr uint16 kUserReserved1 = 1 << 1;
    static constexpr uint16 kUserReserved2 = 1 << 2;
};

// Interface ID
inline constexpr FUID IEventList_iid(0x3A4C8F4D, 0x5E7B4A6D, 0x9C3F8A2E, 0x1D5B7E4A);

} // namespace Steinberg
