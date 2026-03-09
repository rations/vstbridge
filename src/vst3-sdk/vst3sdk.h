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
 * @file vst3sdk.h
 * @brief Umbrella header — includes all VST3 SDK interface headers.
 *
 * Include this single header to get all VST3 types and interfaces needed
 * for hosting or implementing VST3 plugins.
 */

#pragma once

#include "funknown.h"
#include "ipluginfactory.h"
#include "ipluginbase.h"
#include "icomponent.h"
#include "iaudioprocessor.h"
#include "ieditcontroller.h"
#include "iplugview.h"
#include "ibstream.h"
#include "ieventlist.h"
#include "iparameterchanges.h"

// Version identifiers
#define VST3_BRIDGE_SDK_VERSION_MAJOR 3
#define VST3_BRIDGE_SDK_VERSION_MINOR 7
#define VST3_BRIDGE_SDK_VERSION_SUB   7

namespace Steinberg {

/**
 * @brief Safe interface query helper.
 *
 * Queries @p obj for interface @p IFaceType and returns the resulting
 * pointer, or nullptr if the query fails.
 * Increments the reference count on success; the caller owns the reference.
 *
 * @tparam IFaceType  Target interface type (must have a static iid).
 * @param  obj        Source FUnknown to query.
 * @return Typed interface pointer or nullptr.
 */
template<typename IFaceType>
IFaceType* queryInterface(FUnknown* obj) noexcept {
    if (!obj) return nullptr;
    TUID tuid;
    IFaceType::iid.toTUID(tuid);
    void* result = nullptr;
    if (obj->queryInterface(tuid, &result) == kResultOk) {
        return static_cast<IFaceType*>(result);
    }
    return nullptr;
}

} // namespace Steinberg
