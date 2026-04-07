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
 * @file component_handler.h
 * @brief Wine-side IComponentHandler proxy.
 *
 * The plugin controller calls IComponentHandler methods when the user
 * interacts with the plugin GUI (moves a knob, presses a button, etc.).
 * This implementation serialises those calls and forwards them over the
 * IPC socket to the native side, where the DAW's IComponentHandler is
 * called.
 *
 * The native side's PluginProxy::setComponentHandler() stores the DAW
 * handler and calls it when it receives ComponentHandlerBeginEdit /
 * PerformEdit / EndEdit / Restart messages.
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "vst3sdk.h"
#include "protocol.h"

namespace vst3bridge {

class WineSocketClient;

/**
 * @brief IComponentHandler that forwards all callbacks over the IPC socket.
 *
 * One instance is created per plugin instance and registered with the
 * IEditController via setComponentHandler().  It remains alive for the
 * lifetime of the plugin instance.
 */
class ComponentHandler final : public Steinberg::IComponentHandler {
public:
    /**
     * @param socket  Socket used to send messages to the native side.
     *                The caller must ensure socket outlives this object.
     */
    ComponentHandler(WineSocketClient* socket);
    ~ComponentHandler() override = default;

    // Non-copyable, non-movable
    ComponentHandler(const ComponentHandler&) = delete;
    ComponentHandler& operator=(const ComponentHandler&) = delete;

    // ---- FUnknown -----------------------------------------------------------

    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    // ---- IComponentHandler --------------------------------------------------

    /**
     * @brief Called by plugin when user starts dragging a control.
     *
     * Forwards MsgComponentHandlerBeginEdit to the native side.
     */
    Steinberg::tresult PLUGIN_API beginEdit(Steinberg::uint32 id) override;

    /**
     * @brief Called by plugin on every value change during a drag gesture.
     *
     * Forwards MsgComponentHandlerPerformEdit.
     */
    Steinberg::tresult PLUGIN_API performEdit(
            Steinberg::uint32 id, double valueNormalized) override;

    /**
     * @brief Called by plugin when the user releases the control.
     *
     * Forwards MsgComponentHandlerEndEdit.
     */
    Steinberg::tresult PLUGIN_API endEdit(Steinberg::uint32 id) override;

    /**
     * @brief Called by plugin to request a host restart.
     *
     * Forwards MsgComponentHandlerRestart.
     */
    Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override;

    // ---- FUnknown iid -------------------------------------------------------

    static const Steinberg::FUID iid;

private:
    WineSocketClient* socket_;
    Steinberg::int32  refCount_;
};

} // namespace vst3bridge
