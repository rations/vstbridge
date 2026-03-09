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
 * @file gui_handler.h
 * @brief GUI event handler for native side (X11 events)
 */

#pragma once

#include "ipc_client.h"
#include "gui_events.h"
#include <xcb/xcb.h>
#include <thread>
#include <atomic>
#include <memory>

namespace vst3bridge {

/**
 * @brief Handles GUI events on the native side
 * 
 * Captures X11 events from the plugin window and forwards them
 * to the Wine host via IPC.
 */
class GUIEventHandler {
public:
    /**
     * @brief Construct GUI event handler
     * @param ipc IPC client for communication with Wine host
     * @param window X11 window to capture events from
     */
    GUIEventHandler(std::shared_ptr<IpcClient> ipc, xcb_window_t window);

    ~GUIEventHandler();

    /**
     * @brief Start event capture thread
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop event capture thread
     */
    void stop();

    /**
     * @brief Set scale factor for coordinate conversion
     * @param scale Scale factor (e.g., 1.5 for 150%)
     */
    void setScaleFactor(double scale) { scale_factor_ = scale; }

    /**
     * @brief Check if handler is running
     */
    bool isRunning() const { return running_.load(); }

private:
    /**
     * @brief Event capture loop
     */
    void eventLoop();

    /**
     * @brief Process a single X11 event
     * @param event X11 event
     */
    void processEvent(const xcb_generic_event_t* event);

    /**
     * @brief Send GUI event to Wine host
     * @param event Event to send
     */
    void sendEvent(const GUIEventMessage& event);

    /**
     * @brief Get modifier keys state from X11 state
     * @param state X11 state mask
     * @return ModifierKeys structure
     */
    ModifierKeys getModifiers(uint16_t state) const;

    std::shared_ptr<IpcClient> ipc_;
    xcb_window_t window_;
    xcb_connection_t* connection_;
    double scale_factor_ = 1.0;

    std::thread event_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Last mouse position for tracking
    int32_t last_mouse_x_ = 0;
    int32_t last_mouse_y_ = 0;

    // Track which buttons are currently pressed
    uint32_t button_state_ = 0;
};

} // namespace vst3bridge
