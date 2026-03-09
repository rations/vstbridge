/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

/**
 * @file icomponent.h
 * @brief VST3 IComponent interface and bus management types
 *
 * IComponent is the primary audio-processing interface implemented by
 * every VST3 plugin.  It extends IPluginBase with bus configuration
 * and state persistence methods.
 */

#pragma once

#include "ipluginbase.h"
#include "ibstream.h"

namespace Steinberg {

// ============================================================================
// Media types
// ============================================================================

/**
 * @brief Numeric media type identifier used in bus queries.
 *
 * Stored as int32 so it can be embedded in protocol message structs without
 * padding surprises.  Values match the Steinberg SDK kAudio / kEvent constants.
 */
enum MediaType : int32 {
    kAudio = 0,   ///< Audio bus (PCM samples)
    kEvent = 1    ///< Event bus (MIDI / VST3 events)
};

// ============================================================================
// Bus direction
// ============================================================================

/**
 * @brief Bus direction identifier.  Stored as int32 for protocol serialisation.
 */
enum BusDirection : int32 {
    kInput  = 0,
    kOutput = 1
};

// ============================================================================
// Bus types
// ============================================================================

/** Bus type: main signal path or auxiliary send/return. */
enum BusType : int32 {
    kMain = 0,
    kAux  = 1
};

// ============================================================================
// I/O modes
// ============================================================================

/**
 * @brief I/O mode set by the host to indicate the processing context.
 * Passed to IComponent::setIoMode().
 */
enum IoMode : int32 {
    kSimple             = 0,  ///< Default: simple I/O mapping
    kAdvanced           = 1,  ///< Host handles complex routing internally
    kOfflineProcessing  = 2   ///< Offline export — no real-time constraints
};

// ============================================================================
// BusInfo
// ============================================================================

/**
 * @brief Information about one bus returned by IComponent::getBusInfo().
 */
struct BusInfo {
    static constexpr int32 kNameSize = 128;

    MediaType    mediaType;              ///< kAudio or kEvent
    BusDirection direction;             ///< kInput or kOutput
    int32        channelCount;          ///< Number of channels (audio buses)
    char8        name[kNameSize];       ///< Human-readable bus name (UTF-8)
    BusType      busType;               ///< kMain or kAux
    uint32       flags;                 ///< Bus flags

    /** Set when the bus should be active by default. */
    static constexpr uint32 kDefaultActive = 1u << 0;
};

// ============================================================================
// RoutingInfo
// ============================================================================

/**
 * @brief Used by getRoutingInfo() to describe the internal routing between
 *        an input bus and an output bus.
 */
struct RoutingInfo {
    MediaType mediaType;  ///< Media type of the bus
    int32     busIndex;   ///< Bus index (-1 = not connected)
    int32     channel;    ///< Channel index (-1 = all channels / not connected)

    RoutingInfo() : mediaType(kAudio), busIndex(-1), channel(-1) {}
};

// ============================================================================
// IComponent
// ============================================================================

/**
 * @brief Core plugin component interface.
 *
 * Extends IPluginBase with:
 *  - Bus enumeration and activation (audio and event buses)
 *  - I/O mode
 *  - Plugin state serialisation (save / restore preset)
 *
 * IAudioProcessor must also be queryable from the same object for audio
 * plugins (or the plugin may separate the interfaces across two objects
 * connected via IConnectionPoint).
 */
class IComponent : public IPluginBase {
public:
    /**
     * @brief Get the FUID of the IEditController class associated with
     *        this component.
     *
     * If the component and controller are the same object, this should
     * return kResultFalse.
     *
     * @param classId  Output: controller class identifier.
     * @return kResultOk if a separate controller class exists.
     */
    virtual tresult PLUGIN_API getControllerClassId(FUID* classId) = 0;

    /**
     * @brief Set I/O mode.
     * @param mode  One of the IoMode enum values.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setIoMode(IoMode mode) = 0;

    /**
     * @brief Get the number of buses for a given media type and direction.
     * @param type  kAudio or kEvent.
     * @param dir   kInput or kOutput.
     * @return Bus count (>= 0).
     */
    virtual int32 PLUGIN_API getBusCount(MediaType type, BusDirection dir) = 0;

    /**
     * @brief Get information about a specific bus.
     * @param type   Media type.
     * @param dir    Direction.
     * @param index  Zero-based bus index.
     * @param bus    Output: bus information.
     * @return kResultOk on success, kInvalidArgument if index is out of range.
     */
    virtual tresult PLUGIN_API getBusInfo(
            MediaType type, BusDirection dir,
            int32 index, BusInfo& bus) = 0;

    /**
     * @brief Get routing information for a bus.
     * @param inInfo   Input routing.
     * @param outInfo  Output routing.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getRoutingInfo(
            RoutingInfo& inInfo, RoutingInfo& outInfo) = 0;

    /**
     * @brief Activate or deactivate a bus.
     *
     * Inactive buses will not receive / produce audio.
     * @param type    Media type.
     * @param dir     Direction.
     * @param index   Bus index.
     * @param state   true = activate, false = deactivate.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API activateBus(
            MediaType type, BusDirection dir,
            int32 index, bool state) = 0;

    /**
     * @brief Activate or deactivate the entire component.
     *
     * Activation prepares the plugin for processing.  All bus arrangements
     * must be set before calling setActive(true).
     *
     * @param state  true = activate (prepare), false = deactivate.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setActive(bool state) = 0;

    /**
     * @brief Restore plugin state from a stream (load preset / project).
     * @param state  IBStream providing the stored state bytes.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setState(IBStream* state) = 0;

    /**
     * @brief Serialise plugin state to a stream (save preset / project).
     * @param state  IBStream to write state bytes into.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getState(IBStream* state) = 0;

    static const FUID iid;
};

// IComponent real IID: {E831FF31-F2D5-4301-928E-BBEE25697802}
inline const FUID IComponent::iid(
        0xE831FF31u, 0xF2D54301u, 0x928EBBEEu, 0x25697802u);

} // namespace Steinberg
