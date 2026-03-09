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
 * @file iaudioprocessor.h
 * @brief VST3 IAudioProcessor interface and associated structures
 *
 * Defines all structures passed to IAudioProcessor::process():
 *   AudioBusBuffers, ProcessData, ProcessSetup, ProcessContext.
 * Layout must match the Steinberg ABI exactly so that real Windows
 * plugins compiled against the Steinberg SDK see the correct data.
 */

#pragma once

#include "icomponent.h"
#include "iparameterchanges.h"
#include "ieventlist.h"

namespace Steinberg {

// ============================================================================
// Sample-size and process-mode constants
// ============================================================================

/** Symbolic sample size identifiers passed to canProcessSampleSize(). */
namespace SymbolicSampleSizes {
    constexpr int32 kSample32 = 0;  ///< 32-bit IEEE float
    constexpr int32 kSample64 = 1;  ///< 64-bit IEEE float (double)
}

using namespace SymbolicSampleSizes;   // allow kSample32/kSample64 unqualified

/** Processing mode identifiers passed to ProcessSetup::processMode. */
enum ProcessModes : int32 {
    kRealtime = 0,   ///< Real-time production mode
    kPrefetch = 1,   ///< Prefetch/pre-render (slightly relaxed timing)
    kOffline  = 2    ///< Offline export (no real-time constraint)
};

// ============================================================================
// Speaker arrangement
//
// Each bit represents one speaker channel.  Bit positions match the
// Steinberg SpeakerArr constants.  Values here cover the common cases
// needed to count channels; a full implementation would expose all the
// constants from the real SDK.
// ============================================================================

/** Bitmask describing the speaker positions of one bus. */
typedef uint64 SpeakerArrangement;

/** Sample rate type (consistent with Steinberg SDK). */
typedef double SampleRate;

namespace SpeakerArr {
    constexpr SpeakerArrangement kEmpty  = 0x0000000000000000ULL;
    constexpr SpeakerArrangement kMono   = 0x0000000000000001ULL;  // bit 0 = left (mono)
    constexpr SpeakerArrangement kStereo = 0x0000000000000003ULL;  // bits 0+1 = L+R

    // Common multi-channel configurations
    constexpr SpeakerArrangement k30Cine  = 0x0000000000000007ULL; // L+R+C
    constexpr SpeakerArrangement k40Cine  = 0x000000000000000FULL; // L+R+C+Ls
    constexpr SpeakerArrangement k50      = 0x000000000000001FULL; // L+R+C+Ls+Rs
    constexpr SpeakerArrangement k51      = 0x000000000000003FULL; // L+R+C+LFE+Ls+Rs
    constexpr SpeakerArrangement k71Music = 0x00000000000000FFULL; // L+R+C+LFE+Lss+Rss+Ls+Rs

    /**
     * @brief Count the number of channels in a speaker arrangement.
     *
     * Each set bit corresponds to one channel speaker.
     * Uses the compiler built-in for a fast popcount when available.
     */
    inline int32 getChannelCount(SpeakerArrangement arr) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<int32>(__builtin_popcountll(static_cast<unsigned long long>(arr)));
#else
        // Portable fallback
        int32 count = 0;
        while (arr) {
            count += static_cast<int32>(arr & 1u);
            arr >>= 1;
        }
        return count;
#endif
    }
} // namespace SpeakerArr

// ============================================================================
// ProcessSetup
// ============================================================================

/**
 * @brief Processing configuration passed to IAudioProcessor::setupProcessing().
 *
 * The host fills this structure before activating the processor.  The
 * values remain constant until the next call to setupProcessing().
 */
struct ProcessSetup {
    int32      processMode;          ///< ProcessModes enum value
    int32      symbolicSampleSize;   ///< SymbolicSampleSizes enum value
    int32      maxSamplesPerBlock;   ///< Maximum block size (samples)
    SampleRate sampleRate;           ///< Sample rate in Hz
};

// ============================================================================
// AudioBusBuffers
// ============================================================================

/**
 * @brief Channel buffer pointers for one bus in a single process() call.
 *
 * Layout matches the Steinberg VST3 SDK ABI:
 *   - numChannels: number of active channels in this bus
 *   - silenceFlags: one bit per channel; bit N set = channel N is silent
 *   - channelBuffers32 / channelBuffers64: pointer array of length numChannels
 *
 * The host sets the pointers before calling process() and the plugin reads
 * input / writes output through them.
 *
 * @note  The union reinterprets the same pointer storage as 32-bit or 64-bit
 *        depending on ProcessSetup::symbolicSampleSize.  This matches the
 *        Steinberg SDK union layout exactly.
 */
struct AudioBusBuffers {
    int32  numChannels;   ///< Number of channels for this bus
    uint64 silenceFlags;  ///< Bitmask: bit N = 1 means channel N is silent

    union {
        float**  channelBuffers32;  ///< Used when symbolicSampleSize == kSample32
        double** channelBuffers64;  ///< Used when symbolicSampleSize == kSample64
    };
};

// ============================================================================
// ProcessContext
// ============================================================================

/**
 * @brief Transport and tempo information passed with each process() call.
 *
 * Fields are only valid when the corresponding bit in @c state is set.
 */
struct ProcessContext {
    uint32 state;                   ///< Combination of kPlaying, kTempoValid, etc.
    double sampleRate;              ///< Sample rate (always valid)
    double projectTimeSamples;      ///< Current project time in samples
    double projectTimeMusic;        ///< Current project time in quarter notes
    double tempo;                   ///< Tempo in BPM (valid when kTempoValid)
    int32  timeSigNumerator;        ///< Time signature numerator
    int32  timeSigDenominator;      ///< Time signature denominator
    double barPositionMusic;        ///< Bar start in quarter notes
    double cycleStartMusic;         ///< Loop start in quarter notes
    double cycleEndMusic;           ///< Loop end in quarter notes

    // State flag bits
    static constexpr uint32 kPlaying             = 1u << 1;
    static constexpr uint32 kCycleActive         = 1u << 2;
    static constexpr uint32 kRecording           = 1u << 3;
    static constexpr uint32 kSystemTimeValid     = 1u << 8;
    static constexpr uint32 kContTimeValid       = 1u << 17;
    static constexpr uint32 kProjectTimeMusicValid = 1u << 9;
    static constexpr uint32 kBarPositionValid    = 1u << 11;
    static constexpr uint32 kCycleValid          = 1u << 12;
    static constexpr uint32 kTempoValid          = 1u << 10;
    static constexpr uint32 kTimeSigValid        = 1u << 13;
    static constexpr uint32 kClockValid          = 1u << 15;
};

// ============================================================================
// ProcessData
// ============================================================================

/**
 * @brief All data passed to IAudioProcessor::process().
 *
 * The host fills this structure for each audio block.  Audio data lives
 * in {@p inputs} / {@p outputs} arrays of AudioBusBuffers.  Parameter
 * automation and MIDI events arrive through the -Changes and -Events
 * pointers (may be nullptr if nothing is pending).
 */
struct ProcessData {
    int32            processMode;           ///< ProcessModes enum value
    int32            symbolicSampleSize;    ///< SymbolicSampleSizes enum value
    int32            numSamples;            ///< Samples in this block
    int32            numInputs;             ///< Number of input buses
    int32            numOutputs;            ///< Number of output buses
    AudioBusBuffers* inputs;                ///< Input bus buffers (numInputs entries)
    AudioBusBuffers* outputs;               ///< Output bus buffers (numOutputs entries)
    IParameterChanges* inputParameterChanges;   ///< Automation in (may be nullptr)
    IParameterChanges* outputParameterChanges;  ///< Controller feedback out (may be nullptr)
    IEventList*        inputEvents;         ///< MIDI / note events in (may be nullptr)
    IEventList*        outputEvents;        ///< Events out (may be nullptr)
    ProcessContext*    processContext;       ///< Transport context (may be nullptr)
};

// ============================================================================
// IAudioProcessor
// ============================================================================

/**
 * @brief Audio processing interface.
 *
 * The DAW queries this interface from an IComponent-implementing plugin
 * instance, then calls it in the following order for each session:
 *
 *   setupProcessing() → setActive(true) → setProcessing(true)
 *     → process() × N
 *   → setProcessing(false) → setActive(false)
 *
 * Bus arrangements must be negotiated via setBusArrangements() before
 * setupProcessing().
 */
class IAudioProcessor : public FUnknown {
public:
    /**
     * @brief Negotiate speaker arrangements for all buses.
     *
     * The host proposes arrangements; the plugin may accept or suggest
     * alternatives.  kResultOk = accepted; kResultFalse = not accepted
     * (plugin will report its preferred arrangements via getBusArrangement).
     *
     * @param inputs   Proposed input arrangements (numIns elements).
     * @param numIns   Number of input buses.
     * @param outputs  Proposed output arrangements (numOuts elements).
     * @param numOuts  Number of output buses.
     * @return kResultOk if accepted, kResultFalse otherwise.
     */
    virtual tresult PLUGIN_API setBusArrangements(
            SpeakerArrangement* inputs,  int32 numIns,
            SpeakerArrangement* outputs, int32 numOuts) = 0;

    /**
     * @brief Query the current speaker arrangement for one bus.
     * @param dir      kInput or kOutput.
     * @param busIndex Zero-based bus index.
     * @param arr      Output speaker arrangement.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getBusArrangement(
            BusDirection dir, int32 busIndex,
            SpeakerArrangement& arr) = 0;

    /**
     * @brief Check if the plugin can process the given sample size.
     * @param symbolicSampleSize kSample32 or kSample64.
     * @return kResultOk if supported.
     */
    virtual tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) = 0;

    /**
     * @brief Get algorithmic latency introduced by the plugin.
     * @return Latency in samples.
     */
    virtual uint32 PLUGIN_API getLatencySamples() = 0;

    /**
     * @brief Prepare the plugin for processing with the given setup.
     *
     * Must be called while the plugin is inactive (setActive(false)).
     * @param setup Processing configuration.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setupProcessing(ProcessSetup& setup) = 0;

    /**
     * @brief Enable or disable real-time processing mode.
     *
     * Called between setActive(true) and the first process() call, and
     * again after the last process() call before setActive(false).
     *
     * @param state true = start processing, false = stop processing.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setProcessing(bool state) = 0;

    /**
     * @brief Process one block of audio.
     *
     * Called from the host audio thread.  Must be real-time safe:
     * no heap allocation, no blocking, no heavy synchronisation.
     *
     * @param data  Input/output buffers and control data.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API process(ProcessData& data) = 0;

    /**
     * @brief Get the tail length the plugin produces after input ends.
     * @return Tail in samples (0 = no tail, kMaxInt = infinite).
     */
    virtual uint32 PLUGIN_API getTailSamples() = 0;

    static const FUID iid;
};

inline const FUID IAudioProcessor::iid(
        0x42043F99u, 0xB7DA453Cu, 0x8104B7A9u, 0x84F66366u);

} // namespace Steinberg
