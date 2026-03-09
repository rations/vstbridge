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
 * @file gui_events.h
 * @brief GUI event definitions for forwarding mouse/keyboard events
 */

#pragma once

#include <cstdint>

namespace vst3bridge {

/**
 * @brief GUI event types
 */
enum class GUIEventType : uint32_t {
    MouseMove = 0,      ///< Mouse moved
    MouseDown,          ///< Mouse button pressed
    MouseUp,            ///< Mouse button released
    MouseDoubleClick,   ///< Mouse double-clicked
    MouseWheel,         ///< Mouse wheel scrolled
    KeyDown,            ///< Key pressed
    KeyUp,              ///< Key released
    Resize,             ///< Window resized
    FocusIn,            ///< Window gained focus
    FocusOut,           ///< Window lost focus
};

/**
 * @brief Mouse buttons
 */
enum class MouseButton : uint32_t {
    Left = 0,
    Right,
    Middle,
    XButton1,
    XButton2,
};

/**
 * @brief Modifier keys state
 */
struct ModifierKeys {
    uint32_t shift : 1;
    uint32_t ctrl : 1;
    uint32_t alt : 1;
    uint32_t super : 1;  ///< Windows/Command key
    uint32_t reserved : 28;

    ModifierKeys() : shift(0), ctrl(0), alt(0), super(0), reserved(0) {}
};

/**
 * @brief Mouse event data
 */
struct MouseEvent {
    int32_t x = 0;              ///< X coordinate in window
    int32_t y = 0;              ///< Y coordinate in window
    MouseButton button = MouseButton::Left;  ///< Mouse button (for down/up events)
    ModifierKeys modifiers;     ///< Modifier keys state
    float wheel_delta = 0.0f;   ///< Wheel delta (for wheel events)
    int32_t wheel_axis = 0;     ///< 0 = vertical, 1 = horizontal
};

/**
 * @brief Key event data
 */
struct KeyEvent {
    uint32_t key_code = 0;      ///< Platform-independent key code (Windows VK codes)
    uint32_t scan_code = 0;     ///< Hardware scan code
    ModifierKeys modifiers;     ///< Modifier keys state
    char16_t character = 0;     ///< Unicode character (for text input)
    bool is_repeat = false;     ///< Whether this is a repeat event
};

/**
 * @brief GUI event message (sent from native to Wine)
 */
struct GUIEventMessage {
    GUIEventType type;
    union {
        MouseEvent mouse;
        KeyEvent key;
        struct {
            uint32_t width;
            uint32_t height;
        } resize;
    } data;

    GUIEventMessage() : type(GUIEventType::MouseMove), data{} {}
};

/**
 * @brief Convert X11 keycode to Windows VK code
 * @param x11_keycode X11 keycode
 * @param x11_keysym X11 keysym
 * @return Windows VK code
 */
uint32_t X11KeyToWindowsVK(uint32_t x11_keycode, uint32_t x11_keysym);

/**
 * @brief Convert X11 button to MouseButton
 * @param x11_button X11 button number (1-5)
 * @return MouseButton
 */
MouseButton X11ButtonToMouseButton(uint32_t x11_button);

} // namespace vst3bridge
