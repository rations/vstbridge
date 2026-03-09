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
 * @file ipluginfactory.h
 * @brief VST3 plugin factory interfaces and plugin class description structs
 *
 * The plugin factory is the entry point for a VST3 plugin DLL.
 * LoadLibrary("plugin.vst3") + GetProcAddress("GetPluginFactory") returns
 * an IPluginFactory pointer.
 */

#pragma once

#include "funknown.h"

namespace Steinberg {

// ============================================================================
// PClassInfo — basic plugin class descriptor (V1)
// ============================================================================

/**
 * @brief Basic information about one plugin class, returned by
 *        IPluginFactory::getClassInfo().
 */
struct PClassInfo {
    static constexpr int32 kCategorySize = 32;
    static constexpr int32 kNameSize     = 64;

    TUID  cid;                         ///< Class identifier (16 bytes)
    int32 cardinality;                 ///< kManyInstances or kSingleInstance
    char8 category[kCategorySize];     ///< Category string, e.g. "Audio Module Class"
    char8 name[kNameSize];             ///< Plugin display name

    PClassInfo() : cardinality(0x7FFFFFFF) {
        std::memset(cid,      0, sizeof(cid));
        std::memset(category, 0, sizeof(category));
        std::memset(name,     0, sizeof(name));
    }
};

// ============================================================================
// PClassInfo2 — extended V2 descriptor
// ============================================================================

/** @brief Extended plugin class descriptor with vendor, version and SDK info. */
struct PClassInfo2 {
    static constexpr int32 kVendorSize     = 64;
    static constexpr int32 kVersionSize    = 64;
    static constexpr int32 kSDKVersionSize = 64;

    TUID  cid;
    int32 cardinality;
    char8 category[PClassInfo::kCategorySize];
    char8 name[PClassInfo::kNameSize];
    uint32 classFlags;
    char8  subCategories[PClassInfo::kCategorySize];
    char8  vendor[kVendorSize];
    char8  version[kVersionSize];
    char8  sdkVersion[kSDKVersionSize];

    PClassInfo2() : cardinality(0x7FFFFFFF), classFlags(0) {
        std::memset(cid,           0, sizeof(cid));
        std::memset(category,      0, sizeof(category));
        std::memset(name,          0, sizeof(name));
        std::memset(subCategories, 0, sizeof(subCategories));
        std::memset(vendor,        0, sizeof(vendor));
        std::memset(version,       0, sizeof(version));
        std::memset(sdkVersion,    0, sizeof(sdkVersion));
    }
};

// ============================================================================
// PClassInfoW — Unicode V3 descriptor
// ============================================================================

/** @brief Unicode-name variant of the extended plugin class descriptor. */
struct PClassInfoW {
    static constexpr int32 kVendorSize     = 64;
    static constexpr int32 kVersionSize    = 64;
    static constexpr int32 kSDKVersionSize = 64;

    TUID   cid;
    int32  cardinality;
    char8  category[PClassInfo::kCategorySize];
    char16 name[PClassInfo::kNameSize];
    uint32 classFlags;
    char8  subCategories[PClassInfo::kCategorySize];
    char16 vendor[kVendorSize];
    char16 version[kVersionSize];
    char16 sdkVersion[kSDKVersionSize];

    PClassInfoW() : cardinality(0x7FFFFFFF), classFlags(0) {
        std::memset(cid,           0, sizeof(cid));
        std::memset(category,      0, sizeof(category));
        std::memset(name,          0, sizeof(name));
        std::memset(subCategories, 0, sizeof(subCategories));
        std::memset(vendor,        0, sizeof(vendor));
        std::memset(version,       0, sizeof(version));
        std::memset(sdkVersion,    0, sizeof(sdkVersion));
    }
};

// ============================================================================
// PFactoryInfo — factory-level descriptor
// ============================================================================

/** @brief Vendor and contact information for the plugin DLL as a whole. */
struct PFactoryInfo {
    static constexpr int32 kNameSize  = 64;
    static constexpr int32 kURLSize   = 256;
    static constexpr int32 kEmailSize = 128;

    /** Factory flags. */
    static constexpr int32 kClassesDiscardable        = 1 << 0;
    static constexpr int32 kLicenseCheck              = 1 << 1;
    static constexpr int32 kComponentNonDiscardable   = 1 << 2;
    static constexpr int32 kUnicode                   = 1 << 3;

    char8 vendor[kNameSize];
    char8 url[kURLSize];
    char8 email[kEmailSize];
    int32 flags;

    PFactoryInfo() : flags(0) {
        std::memset(vendor, 0, sizeof(vendor));
        std::memset(url,    0, sizeof(url));
        std::memset(email,  0, sizeof(email));
    }
};

// ============================================================================
// Cardinality constant
// ============================================================================

/** Conventional cardinality value meaning "supports many instances". */
static constexpr int32 kManyInstances = 0x7FFFFFFF;

// ============================================================================
// IPluginFactory — V1
// ============================================================================

/**
 * @brief VST3 plugin factory — entry point for all plugins.
 *
 * Exported by every VST3 DLL via the GetPluginFactory() function:
 * @code
 *   typedef IPluginFactory* (PLUGIN_API *GetPluginFactoryProc)();
 * @endcode
 */
class IPluginFactory : public FUnknown {
public:
    /**
     * @brief Get vendor / URL / contact information.
     * @param info  Output factory info structure.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getFactoryInfo(PFactoryInfo* info) = 0;

    /**
     * @brief Get the number of plugin classes exported by this DLL.
     * @return Class count (>= 0).
     */
    virtual int32 PLUGIN_API countClasses() = 0;

    /**
     * @brief Get the base class descriptor for one class.
     * @param index  Zero-based index (0 .. countClasses()-1).
     * @param info   Output class descriptor.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getClassInfo(int32 index, PClassInfo* info) = 0;

    /**
     * @brief Create one instance of a plugin class.
     *
     * @param cid   Class identifier copied from PClassInfo.cid.
     * @param _iid  Interface identifier to query on the new object
     *              (e.g. IComponent::iid or IEditController::iid).
     * @param obj   Output pointer — caller must call release() when done.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API createInstance(TUID cid, TUID _iid, void** obj) = 0;

    static const FUID iid;
};

inline const FUID IPluginFactory::iid(
        0x7A4D8F64u, 0x6D7E4E08u, 0xB4E2F84Bu, 0xD9C8F9F4u);

// ============================================================================
// IPluginFactory2 — V2 extended class info
// ============================================================================

/**
 * @brief Extended factory that returns richer class descriptors.
 *
 * Plugins that support ClassInfo2 should implement this interface.
 */
class IPluginFactory2 : public IPluginFactory {
public:
    /**
     * @brief Get the extended class descriptor for one class.
     * @param index  Zero-based class index.
     * @param info   Output extended class descriptor.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getClassInfo2(int32 index, PClassInfo2* info) = 0;

    static const FUID iid;
};

inline const FUID IPluginFactory2::iid(
        0x0007B650u, 0xF2444A4Eu, 0x8F2D7B4Au, 0x3F2D4E88u);

// ============================================================================
// IPluginFactory3 — V3 Unicode class info + host context
// ============================================================================

/**
 * @brief Factory supporting Unicode class names and host-context handshake.
 */
class IPluginFactory3 : public IPluginFactory2 {
public:
    /**
     * @brief Get the Unicode-name class descriptor.
     * @param index  Zero-based class index.
     * @param info   Output Unicode class descriptor.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getClassInfoUnicode(int32 index, PClassInfoW* info) = 0;

    /**
     * @brief Provide the factory with a host context (IHostApplication).
     *
     * Called by the host once per factory load so the plugin can query
     * host capabilities.
     *
     * @param context  IHostApplication or nullptr.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API setHostContext(FUnknown* context) = 0;

    static const FUID iid;
};

inline const FUID IPluginFactory3::iid(
        0x4555A2AAu, 0xB7E94C48u, 0xB8C3F9E5u, 0x2F3D8A9Bu);

// ============================================================================
// GetPluginFactory function pointer type
// ============================================================================

/** Type of the exported GetPluginFactory() entry-point function. */
typedef IPluginFactory* (PLUGIN_API *GetPluginFactoryProc)();

/** Name of the exported entry-point function. */
static constexpr const char* kGetPluginFactorySymbol = "GetPluginFactory";

} // namespace Steinberg
