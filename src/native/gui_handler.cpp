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
 * @file gui_handler.cpp
 * @brief GUI event handler implementation for native side
 */

#include "gui_handler.h"
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <cstring>
#include <iostream>

namespace vst3bridge {

GUIEventHandler::GUIEventHandler(std::shared_ptr<IpcClient> ipc, xcb_window_t window)
    : ipc_(ipc)
    , window_(window)
    , connection_(nullptr) {
}

GUIEventHandler::~GUIEventHandler() {
    stop();
}

bool GUIEventHandler::start() {
    if (running_.load()) {
        return true;
    }

    // Get XCB connection from window
    int screen_num;
    connection_ = xcb_connect(nullptr, &screen_num);
    if (xcb_connection_has_error(connection_)) {
        std::cerr << "[GUIEventHandler] Failed to connect to X server\n";
        return false;
    }

    // Select input events on the window
    uint32_t event_mask = XCB_EVENT_MASK_BUTTON_PRESS |
                          XCB_EVENT_MASK_BUTTON_RELEASE |
                          XCB_EVENT_MASK_POINTER_MOTION |
                          XCB_EVENT_MASK_KEY_PRESS |
                          XCB_EVENT_MASK_KEY_RELEASE |
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                          XCB_EVENT_MASK_FOCUS_CHANGE;

    xcb_change_window_attributes(connection_, window_, XCB_CW_EVENT_MASK,
                                 &event_mask);
    xcb_flush(connection_);

    stop_requested_ = false;
    running_ = true;

    event_thread_ = std::thread(&GUIEventHandler::eventLoop, this);
    return true;
}

void GUIEventHandler::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_ = true;

    // Wake up the event loop by sending a dummy event
    xcb_client_message_event_t event;
    std::memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = window_;
    xcb_send_event(connection_, false, window_, XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char*>(&event));
    xcb_flush(connection_);

    if (event_thread_.joinable()) {
        event_thread_.join();
    }

    if (connection_) {
        xcb_disconnect(connection_);
        connection_ = nullptr;
    }

    running_ = false;
}

void GUIEventHandler::eventLoop() {
    while (!stop_requested_.load()) {
        xcb_generic_event_t* event = xcb_wait_for_event(connection_);
        if (!event) {
            continue;
        }

        processEvent(event);
        free(event);
    }
}

void GUIEventHandler::processEvent(const xcb_generic_event_t* event) {
    uint8_t event_type = event->response_type & ~0x80;

    switch (event_type) {
        case XCB_BUTTON_PRESS: {
            auto* button = reinterpret_cast<const xcb_button_press_event_t*>(event);
            
            if (button->detail >= 4 && button->detail <= 7) {
                // Mouse wheel
                GUIEventMessage msg;
                msg.type = GUIEventType::MouseWheel;
                msg.data.mouse.x = static_cast<int32_t>(button->event_x / scale_factor_);
                msg.data.mouse.y = static_cast<int32_t>(button->event_y / scale_factor_);
                msg.data.mouse.modifiers = getModifiers(button->state);
                msg.data.mouse.wheel_delta = (button->detail == 4 || button->detail == 6) ? 1.0f : -1.0f;
                msg.data.mouse.wheel_axis = (button->detail <= 5) ? 0 : 1;  // 0=vertical, 1=horizontal
                sendEvent(msg);
            } else {
                // Regular button press
                button_state_ |= (1 << (button->detail - 1));
                
                GUIEventMessage msg;
                msg.type = GUIEventType::MouseDown;
                msg.data.mouse.x = static_cast<int32_t>(button->event_x / scale_factor_);
                msg.data.mouse.y = static_cast<int32_t>(button->event_y / scale_factor_);
                msg.data.mouse.button = X11ButtonToMouseButton(button->detail);
                msg.data.mouse.modifiers = getModifiers(button->state);
                sendEvent(msg);

                last_mouse_x_ = msg.data.mouse.x;
                last_mouse_y_ = msg.data.mouse.y;
            }
            break;
        }

        case XCB_BUTTON_RELEASE: {
            auto* button = reinterpret_cast<const xcb_button_release_event_t*>(event);
            
            // Ignore wheel release events
            if (button->detail >= 4 && button->detail <= 7) {
                break;
            }

            button_state_ &= ~(1 << (button->detail - 1));

            GUIEventMessage msg;
            msg.type = GUIEventType::MouseUp;
            msg.data.mouse.x = static_cast<int32_t>(button->event_x / scale_factor_);
            msg.data.mouse.y = static_cast<int32_t>(button->event_y / scale_factor_);
            msg.data.mouse.button = X11ButtonToMouseButton(button->detail);
            msg.data.mouse.modifiers = getModifiers(button->state);
            sendEvent(msg);
            break;
        }

        case XCB_MOTION_NOTIFY: {
            auto* motion = reinterpret_cast<const xcb_motion_notify_event_t*>(event);

            GUIEventMessage msg;
            msg.type = GUIEventType::MouseMove;
            msg.data.mouse.x = static_cast<int32_t>(motion->event_x / scale_factor_);
            msg.data.mouse.y = static_cast<int32_t>(motion->event_y / scale_factor_);
            msg.data.mouse.modifiers = getModifiers(motion->state);
            sendEvent(msg);

            last_mouse_x_ = msg.data.mouse.x;
            last_mouse_y_ = msg.data.mouse.y;
            break;
        }

        case XCB_KEY_PRESS: {
            auto* key = reinterpret_cast<const xcb_key_press_event_t*>(event);
            xcb_keycode_t keycode = key->detail;
            
            // Get keysym
            xcb_key_symbols_t* keysyms = xcb_key_symbols_alloc(connection_);
            xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
            xcb_key_symbols_free(keysyms);

            GUIEventMessage msg;
            msg.type = GUIEventType::KeyDown;
            msg.data.key.key_code = X11KeyToWindowsVK(keycode, keysym);
            msg.data.key.scan_code = keycode;
            msg.data.key.modifiers = getModifiers(key->state);
            msg.data.key.is_repeat = false;  // Would need tracking
            sendEvent(msg);
            break;
        }

        case XCB_KEY_RELEASE: {
            auto* key = reinterpret_cast<const xcb_key_release_event_t*>(event);
            xcb_keycode_t keycode = key->detail;
            
            // Get keysym
            xcb_key_symbols_t* keysyms = xcb_key_symbols_alloc(connection_);
            xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
            xcb_key_symbols_free(keysyms);

            GUIEventMessage msg;
            msg.type = GUIEventType::KeyUp;
            msg.data.key.key_code = X11KeyToWindowsVK(keycode, keysym);
            msg.data.key.scan_code = keycode;
            msg.data.key.modifiers = getModifiers(key->state);
            sendEvent(msg);
            break;
        }

        case XCB_CONFIGURE_NOTIFY: {
            auto* config = reinterpret_cast<const xcb_configure_notify_event_t*>(event);

            GUIEventMessage msg;
            msg.type = GUIEventType::Resize;
            msg.data.resize.width = static_cast<uint32_t>(config->width / scale_factor_);
            msg.data.resize.height = static_cast<uint32_t>(config->height / scale_factor_);
            sendEvent(msg);
            break;
        }

        case XCB_FOCUS_IN: {
            GUIEventMessage msg;
            msg.type = GUIEventType::FocusIn;
            sendEvent(msg);
            break;
        }

        case XCB_FOCUS_OUT: {
            GUIEventMessage msg;
            msg.type = GUIEventType::FocusOut;
            sendEvent(msg);
            break;
        }
    }
}

void GUIEventHandler::sendEvent(const GUIEventMessage& event) {
    if (!ipc_) return;

    // Send as GUI event message type
    // Note: This uses a custom message type in the protocol
    // For now, we'll serialize and send as a generic message
    ipc_->sendRawMessage(&event, sizeof(event));
}

ModifierKeys GUIEventHandler::getModifiers(uint16_t state) const {
    ModifierKeys modifiers;
    modifiers.shift = (state & XCB_MOD_MASK_SHIFT) ? 1 : 0;
    modifiers.ctrl = (state & XCB_MOD_MASK_CONTROL) ? 1 : 0;
    // Alt is Mod1 in X11
    modifiers.alt = (state & XCB_MOD_MASK_1) ? 1 : 0;
    // Super/Windows is Mod4
    modifiers.super = (state & XCB_MOD_MASK_4) ? 1 : 0;
    return modifiers;
}

} // namespace vst3bridge
