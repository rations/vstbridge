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
 * @file ieditcontroller.h
 * @brief VST3 IEditController, IComponentHandler and ParameterInfo
 *
 * The IEditController exposes the GUI-facing side of a VST3 plugin:
 * parameter enumeration, normalised value accessors, and GUI view creation.
 *
 * IComponentHandler is the host-side callback that plugins call when the
 * user changes parameter values in the plugin GUI.
 */

#pragma once

#include "ipluginbase.h"
#include "ibstream.h"

namespace Steinberg {

// Forward declarations
class IPlugView;
class IComponentHandler;

/** Pointer to a null-terminated ASCII/UTF-8 string used as a type tag. */
using FIDString = const char8*;

// ============================================================================
// ParameterInfo
// ============================================================================

/**
 * @brief Flags describing parameter properties.
 */
enum ParameterFlags : int32 {
    kCanAutomate       = 1 << 0,   ///< Automatable by the host
    kIsReadOnly        = 1 << 1,   ///< Host cannot change value
    kIsWrapAround      = 1 << 2,   ///< Value wraps from max → min
    kIsList            = 1 << 3,   ///< Discrete list of steps
    kIsHidden          = 1 << 4,   ///< Hidden from generic GUI
    kIsProgramChange   = 1 << 15,  ///< Program/preset change parameter
    kIsBypass          = 1 << 16   ///< Plugin bypass parameter
};

/**
 * @brief Static description of one plugin parameter.
 */
struct ParameterInfo {
    static constexpr int32 kTitleSize      = 128;
    static constexpr int32 kShortTitleSize = 128;
    static constexpr int32 kUnitsSize      = 128;

    uint32  id;                            ///< Unique parameter ID
    char16  title[kTitleSize];             ///< Full title (UTF-16)
    char16  shortTitle[kShortTitleSize];   ///< Short title (UTF-16)
    char16  units[kUnitsSize];             ///< Unit string, e.g. "dB" (UTF-16)
    int32   stepCount;                     ///< 0 = continuous; N = N+1 discrete steps
    double  defaultNormalizedValue;        ///< Default value in [0.0, 1.0]
    int32   unitId;                        ///< Unit ID (0 = kRootUnitId)
    int32   flags;                         ///< Combination of ParameterFlags
};

// ============================================================================
// IComponentHandler
// ============================================================================

/**
 * @brief Host-provided callback interface for parameter changes initiated
 *        by the plugin GUI.
 *
 * Plugins call these methods when the user moves a knob, slider, etc.
 * The host uses them to record automation and update the DAW transport.
 *
 * The bridge implements this interface on the native side and registers it
 * with the plugin controller via IEditController::setComponentHandler().
 */
class IComponentHandler : public FUnknown {
public:
    /**
     * @brief Notify the host that a parameter edit gesture has started
     *        (e.g., mouse button down on a knob).
     * @param id  Parameter ID being edited.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API beginEdit(uint32 id) = 0;

    /**
     * @brief Notify the host of a new normalised parameter value.
     *
     * May be called many times between beginEdit() and endEdit() for
     * continuous gestures.
     *
     * @param id               Parameter ID.
     * @param valueNormalized  New value in [0.0, 1.0].
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API performEdit(uint32 id, double valueNormalized) = 0;

    /**
     * @brief Notify the host that the parameter edit gesture has ended
     *        (e.g., mouse button released).
     * @param id  Parameter ID.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API endEdit(uint32 id) = 0;

    /**
     * @brief Request the host to restart the component with the given flags.
     *
     * Plugins call this when their configuration changes in a way that
     * requires re-initialisation (e.g., latency changed, I/O count changed).
     *
     * @param flags  Combination of RestartFlags.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API restartComponent(int32 flags) = 0;

    static const FUID iid;
};

inline const FUID IComponentHandler::iid(
        0x93A0BEA3u, 0x0BD045DBu, 0x8E890B0Cu, 0xC1E46AC6u);

// ============================================================================
// IEditController
// ============================================================================

/**
 * @brief Plugin controller interface — parameters and GUI creation.
 *
 * Either provided as a separate object from IComponent (whose class ID is
 * returned by IComponent::getControllerClassId()), or implemented by the
 * same object as IComponent if getControllerClassId() returns kResultFalse.
 */
class IEditController : public IPluginBase {
public:
    /**
     * @brief Synchronise the controller state from the component's state.
     *
     * Called by the host after restoring a project or loading a preset,
     * so the controller knows the current processor state.
     *
     * @param state  IBStream containing the component state.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setComponentState(IBStream* state) = 0;

    /**
     * @brief Restore controller-specific state from a stream.
     * @param state  IBStream providing the stored state.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setState(IBStream* state) = 0;

    /**
     * @brief Serialise controller-specific state to a stream.
     * @param state  IBStream to write into.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getState(IBStream* state) = 0;

    /**
     * @brief Get the total number of automatable parameters.
     * @return Parameter count.
     */
    virtual int32 PLUGIN_API getParameterCount() = 0;

    /**
     * @brief Retrieve static description for a parameter by index.
     * @param paramIndex  Zero-based index (0 .. getParameterCount()-1).
     * @param info        Output: parameter description.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getParameterInfo(
            int32 paramIndex, ParameterInfo& info) = 0;

    /**
     * @brief Convert a normalised value to a display string.
     * @param id               Parameter ID.
     * @param valueNormalized  Value in [0.0, 1.0].
     * @param string           Output buffer (at least 128 char16).
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getParamStringByValue(
            uint32 id, double valueNormalized, char16* string) = 0;

    /**
     * @brief Parse a display string to a normalised value.
     * @param id               Parameter ID.
     * @param string           Input string (UTF-16, null-terminated).
     * @param valueNormalized  Output normalised value.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getParamValueByString(
            uint32 id, char16* string, double& valueNormalized) = 0;

    /**
     * @brief Get the current normalised value of a parameter.
     * @param id  Parameter ID.
     * @return Normalised value in [0.0, 1.0].
     */
    virtual double PLUGIN_API getParamNormalized(uint32 id) = 0;

    /**
     * @brief Set a parameter's normalised value (host → plugin).
     *
     * Used for automation playback, preset loading, etc.
     *
     * @param id    Parameter ID.
     * @param value Normalised value in [0.0, 1.0].
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setParamNormalized(uint32 id, double value) = 0;

    /**
     * @brief Register the host's component handler callback.
     *
     * Must be called before the plugin GUI is opened so the plugin can
     * notify the host of user-initiated parameter changes.
     *
     * @param handler  Host-provided IComponentHandler.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setComponentHandler(IComponentHandler* handler) = 0;

    /**
     * @brief Create the plugin GUI view.
     *
     * @param name  View type identifier; use ViewType::kEditor ("editor")
     *              for the main plugin GUI.
     * @return Newly created IPlugView (caller holds reference), or nullptr
     *         if the plugin has no GUI or does not support this view type.
     */
    virtual IPlugView* PLUGIN_API createView(FIDString name) = 0;

    static const FUID iid;
};

inline const FUID IEditController::iid(
        0xDCD7BBE3u, 0x7742448Du, 0xA874AAAEu, 0x0468603Du);

// ============================================================================
// Restart flags (used by IComponentHandler::restartComponent)
// ============================================================================

enum RestartFlags : int32 {
    kReloadComponent          = 1 << 0,   ///< Plugin DLL must be reloaded
    kIoChanged                = 1 << 1,   ///< Bus count / arrangement changed
    kParamValuesChanged       = 1 << 2,   ///< All parameter values changed
    kLatencyChanged           = 1 << 3,   ///< Reported latency changed
    kParamTitlesChanged       = 1 << 4,   ///< Parameter names/units changed
    kMidiCCAssignmentChanged  = 1 << 5,   ///< MIDI learn assignments changed
    kNoteExpressionChanged    = 1 << 6,   ///< Note expression map changed
    kIoTitlesChanged          = 1 << 7    ///< Bus name/titles changed
};

} // namespace Steinberg
