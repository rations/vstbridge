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
 * @file ipluginbase.h
 * @brief VST3 IPluginBase and IHostApplication interfaces
 */

#pragma once

#include "funknown.h"

namespace Steinberg {

// ============================================================================
// IHostApplication
// ============================================================================

/**
 * @brief IHostApplication — host context passed to IPluginBase::initialize()
 *
 * Plugins may query this interface for host capabilities.  The bridge
 * implements a minimal version that only supplies the host name.
 */
class IHostApplication : public FUnknown {
public:
    /**
     * @brief Get application name (UTF-16).
     * @param name  Output buffer of at least 128 char16.
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API getName(String128 name) = 0;

    /**
     * @brief Create a host-provided object by class and interface ID.
     *
     * Most hosts return kNotImplemented.  Plugins must not depend on this
     * unless they know the specific host supports it.
     *
     * @param cid   Class identifier of the host object to create.
     * @param _iid  Interface identifier to query on the new object.
     * @param obj   Output pointer.
     * @return kResultOk on success, kNotImplemented if not supported.
     */
    virtual tresult PLUGIN_API createInstance(TUID cid, TUID _iid, void** obj) = 0;

    static const FUID iid;
};

inline const FUID IHostApplication::iid(0x58E595CCu, 0xDB2D4969u, 0x8B6AAF8Cu, 0x36A664D5u);

// ============================================================================
// IPluginBase
// ============================================================================

/**
 * @brief IPluginBase — common lifecycle interface for all VST3 components.
 *
 * Both IComponent (audio processing) and IEditController (GUI/parameters)
 * inherit from this interface.  The host calls initialize() once after
 * creating the instance and terminate() once before destroying it.
 */
class IPluginBase : public FUnknown {
public:
    /**
     * @brief Initialize the component.
     *
     * @param context  IHostApplication pointer (may be nullptr for minimal hosts).
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API initialize(FUnknown* context) = 0;

    /**
     * @brief Terminate the component.
     *
     * Release all host-provided resources obtained during initialize().
     * @return kResultOk on success.
     */
    virtual tresult PLUGIN_API terminate() = 0;

    static const FUID iid;
};

inline const FUID IPluginBase::iid(0x6C7D8F4Eu, 0x2B3C4D5Eu, 0xA6B7C8D9u, 0xE0F1A2B3u);

} // namespace Steinberg
