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
 * @file iparameterchanges.h
 * @brief VST3 parameter-change queue interfaces
 *
 * IParamValueQueue and IParameterChanges live in the Steinberg::Vst
 * namespace, matching the Steinberg SDK layout used by real plugins
 * and the ParameterChanges implementation in the Wine host.
 */

#pragma once

#include "funknown.h"

namespace Steinberg {
namespace Vst {

/** Unique parameter identifier (32-bit, matching Steinberg SDK). */
typedef uint32 ParamID;

/** Normalised parameter value in the range [0.0, 1.0]. */
typedef double ParamValue;

// ============================================================================
// IParamValueQueue
// ============================================================================

/**
 * @brief Ordered sequence of parameter value changes for one parameter
 *        within a single audio processing block.
 *
 * Each point associates a sample offset with a normalised value.
 * Points must be ordered by ascending sample offset.
 */
class IParamValueQueue : public FUnknown {
public:
    /** @return The parameter ID this queue belongs to. */
    virtual ParamID PLUGIN_API getParameterId() = 0;

    /** @return Number of value-change points in this queue. */
    virtual int32 PLUGIN_API getPointCount() = 0;

    /**
     * @brief Retrieve a value-change point.
     * @param index         Zero-based index.
     * @param sampleOffset  Output: sample offset within the buffer.
     * @param value         Output: normalised parameter value.
     * @return kResultOk on success, kInvalidArgument if index is out of range.
     */
    virtual tresult PLUGIN_API getPoint(
            int32 index,
            int32& sampleOffset,
            ParamValue& value) = 0;

    /**
     * @brief Add a new value-change point.
     *
     * The queue keeps points sorted by sampleOffset.
     *
     * @param sampleOffset  Sample position of the change.
     * @param value         Normalised parameter value.
     * @param index         Output: index at which the point was inserted.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API addPoint(
            int32 sampleOffset,
            ParamValue value,
            int32& index) = 0;

    static const FUID iid;
};

inline const FUID IParamValueQueue::iid(
        0x01263A18u, 0xED007838u, 0x96AB6020u, 0x5B6D9604u);

// ============================================================================
// IParameterChanges
// ============================================================================

/**
 * @brief Container of IParamValueQueue objects, one per changed parameter.
 *
 * Passed to IAudioProcessor::process() as inputParameterChanges and
 * outputParameterChanges.  The host fills inputParameterChanges before
 * each process() call; the plugin may fill outputParameterChanges to
 * report back GUI-driven changes.
 */
class IParameterChanges : public FUnknown {
public:
    /** @return Number of parameters with queued changes. */
    virtual int32 PLUGIN_API getParameterCount() = 0;

    /**
     * @brief Get the change queue at the given index.
     * @param index  Zero-based queue index.
     * @return Pointer to IParamValueQueue (still owned by this object).
     */
    virtual IParamValueQueue* PLUGIN_API getParameterData(int32 index) = 0;

    /**
     * @brief Add or retrieve the change queue for a specific parameter.
     *
     * If a queue already exists for @p id it is returned directly.
     * Otherwise a new empty queue is created.
     *
     * @param id     Parameter ID.
     * @param index  Output: index of the queue within this container.
     * @return Pointer to IParamValueQueue (owned by this object).
     */
    virtual IParamValueQueue* PLUGIN_API addParameterData(
            const ParamID& id,
            int32& index) = 0;

    static const FUID iid;
};

inline const FUID IParameterChanges::iid(
        0xA4779663u, 0x0BB64A56u, 0xB44384A8u, 0x466FEB6Du);

} // namespace Vst

// Convenience aliases so code in the Steinberg namespace can use these
// types without the Vst:: prefix where both are needed.
using Vst::ParamID;
using Vst::ParamValue;
using Vst::IParamValueQueue;
using Vst::IParameterChanges;

} // namespace Steinberg
