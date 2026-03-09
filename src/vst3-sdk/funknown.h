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
 * @file funknown.h
 * @brief VST3 FUnknown base interface and supporting types
 *
 * Provides the complete set of base types, macros and the FUnknown
 * COM-style reference-counting interface required by all VST3 SDK
 * headers and implementations.
 *
 * ABI note: the interface IDs (FUID / iid values) used here are the
 * same as those in the Steinberg VST3 public SDK so that plugins
 * compiled against the real SDK are binary-compatible with this bridge.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

namespace Steinberg {

// ============================================================================
// Primitive type aliases (match Steinberg SDK exactly)
// ============================================================================

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef float    float32;
typedef double   float64;

typedef char     char8;
typedef char16_t char16;

/** Standard VST3 result type (32-bit unsigned, same as Windows HRESULT width) */
typedef uint32_t tresult;

/** Unique type-identity character array (16 bytes = 128-bit GUID), as used by
 *  Steinberg SDK createInstance / queryInterface calls.  Distinct from FUID. */
typedef char TUID[16];

/** Wide character string of 128 chars as used in getName(), etc. */
typedef char16_t String128[128];

// ============================================================================
// Platform calling convention
// ============================================================================

#ifdef _WIN32
#   define PLUGIN_API __stdcall
#else
#   define PLUGIN_API
#endif

// ============================================================================
// VST3 result codes  (matching Steinberg SDK)
// ============================================================================

static constexpr tresult kResultOk       = 0;
static constexpr tresult kResultTrue     = 0;           ///< Alias for kResultOk
static constexpr tresult kResultFalse    = 1;
static constexpr tresult kInvalidArgument = 0x80070057U;
static constexpr tresult kNotImplemented  = 0x80004001U;
static constexpr tresult kNoInterface     = 0x80004002U;
static constexpr tresult kInternalError   = 0x80004005U;
static constexpr tresult kOutOfMemory     = 0x8007000EU;
static constexpr tresult kInvalidState    = 0x8000000DU;

// ============================================================================
// FUID — 128-bit interface identifier
//
// Layout: identical to a Windows GUID (4-2-2-8 bytes).
// Plugins compiled against the real Steinberg SDK use TUID (char[16])
// internally; this FUID type mirrors that layout while providing
// C++ value-semantics and a comparison operator.
// ============================================================================

struct FUID {
    uint32 data1;
    uint16 data2;
    uint16 data3;
    uint8  data4[8];

    // Default: zero FUID
    constexpr FUID() noexcept
        : data1(0), data2(0), data3(0), data4{} {}

    // Standard GUID-style 4-parameter constructor
    constexpr FUID(uint32 d1, uint32 d2, uint32 d3, uint32 d4) noexcept
        : data1(d1)
        , data2(static_cast<uint16>(d2 >> 16))
        , data3(static_cast<uint16>(d2 & 0xFFFFu))
        , data4{ static_cast<uint8>(d3 >> 24), static_cast<uint8>(d3 >> 16),
                 static_cast<uint8>(d3 >>  8), static_cast<uint8>(d3),
                 static_cast<uint8>(d4 >> 24), static_cast<uint8>(d4 >> 16),
                 static_cast<uint8>(d4 >>  8), static_cast<uint8>(d4) }
    {}

    // Construct from a TUID (raw 16-byte array)
    explicit FUID(const TUID tuid) noexcept {
        std::memcpy(this, tuid, 16);
    }

    // Copy to TUID array
    void toTUID(TUID tuid) const noexcept {
        std::memcpy(tuid, this, 16);
    }

    bool operator==(const FUID& o) const noexcept {
        return std::memcmp(this, &o, sizeof(FUID)) == 0;
    }
    bool operator!=(const FUID& o) const noexcept { return !(*this == o); }
};

// ============================================================================
// FUnknown — COM-style base interface
// ============================================================================

class FUnknown {
public:
    virtual ~FUnknown() noexcept = default;

    /**
     * @brief Query for a specific interface by its FUID.
     * @param _iid  Requested interface identifier.
     * @param obj   Output pointer; set to the requested interface or nullptr.
     * @return kResultOk on success, kNoInterface if not supported.
     */
    virtual tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) = 0;

    /** Increment reference count. */
    virtual uint32 PLUGIN_API addRef() = 0;

    /** Decrement reference count; destroy when reaches zero. */
    virtual uint32 PLUGIN_API release() = 0;

    static const FUID iid;
};

// ============================================================================
// FUnknown interface ID
// ============================================================================

inline const FUID FUnknown::iid(0x00000000u, 0x00000000u, 0xC0000000u, 0x00000046u);

// ============================================================================
// Convenience macros for implementing FUnknown in concrete classes.
//
// Usage in a class body:
//   DECLARE_FUNKNOWN_METHODS
//
// Usage in the .cpp (or inline in the header after class definition):
//   IMPLEMENT_REFCOUNT(MyClass)
//   IMPLEMENT_QUERYINTERFACE(MyClass, IMyInterface)
// ============================================================================

/**
 * Declares the three FUnknown virtual overrides inside a class body.
 * The implementation is provided by IMPLEMENT_REFCOUNT and
 * IMPLEMENT_QUERYINTERFACE macros below.
 */
#define DECLARE_FUNKNOWN_METHODS                                              \
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) override;  \
    uint32  PLUGIN_API addRef() override;                                     \
    uint32  PLUGIN_API release() override;

/**
 * Implements addRef() and release() using an atomic reference count.
 * The object is deleted when the reference count reaches zero.
 *
 * @param ClassName  The name of the class being implemented.
 */
#define IMPLEMENT_REFCOUNT(ClassName)                                         \
    uint32 PLUGIN_API ClassName::addRef() {                                   \
        return static_cast<uint32>(++refCount_);                              \
    }                                                                         \
    uint32 PLUGIN_API ClassName::release() {                                  \
        uint32 r = static_cast<uint32>(--refCount_);                          \
        if (r == 0) { delete this; }                                          \
        return r;                                                             \
    }

/**
 * Implements queryInterface() for a class that directly implements one
 * primary interface (InterfaceType).  FUnknown queries are redirected to
 * that interface.  Extend with additional if-branches for multi-interface
 * classes.
 *
 * @param ClassName      The class name.
 * @param InterfaceType  The primary VST3 interface the class implements.
 */
#define IMPLEMENT_QUERYINTERFACE(ClassName, InterfaceType)                    \
    tresult PLUGIN_API ClassName::queryInterface(                             \
            const TUID _iid, void** obj) {                                    \
        if (!obj) return kInvalidArgument;                                    \
        FUID requested(_iid);                                                 \
        if (requested == FUnknown::iid ||                                     \
            requested == InterfaceType::iid) {                                \
            *obj = static_cast<InterfaceType*>(this);                         \
            addRef();                                                          \
            return kResultOk;                                                 \
        }                                                                     \
        *obj = nullptr;                                                       \
        return kNoInterface;                                                  \
    }

// ============================================================================
// Inline helper functions
// ============================================================================

/** Returns true when result indicates success. */
inline bool resultOk(tresult r) noexcept {
    return r == kResultOk;
}

/** Returns true when result indicates failure. */
inline bool resultFailed(tresult r) noexcept {
    return !resultOk(r);
}

} // namespace Steinberg
