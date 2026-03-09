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
 * @file gui_events.cpp
 * @brief GUI event conversion implementations
 */

#include "gui_events.h"

namespace vst3bridge {

uint32_t X11KeyToWindowsVK(uint32_t x11_keycode, uint32_t x11_keysym) {
    (void)x11_keycode; // Not used in current implementation - keysym is sufficient for basic mapping
    
    // Map X11 keysyms to Windows VK codes
    // This is a simplified mapping - a full implementation would be more comprehensive
    
    // Letters
    if (x11_keysym >= 'a' && x11_keysym <= 'z') {
        return 'A' + (x11_keysym - 'a');  // Windows VK codes use uppercase
    }
    if (x11_keysym >= 'A' && x11_keysym <= 'Z') {
        return x11_keysym;
    }
    
    // Numbers
    if (x11_keysym >= '0' && x11_keysym <= '9') {
        return x11_keysym;
    }
    
    // Function keys
    if (x11_keysym >= 0xFFBE && x11_keysym <= 0xFFCF) {  // F1-F12
        return 0x70 + (x11_keysym - 0xFFBE);  // VK_F1 = 0x70
    }
    
    // Special keys (X11 keysym constants)
    switch (x11_keysym) {
        case 0xFF08: return 0x08;  // Backspace
        case 0xFF09: return 0x09;  // Tab
        case 0xFF0D: return 0x0D;  // Return
        case 0xFF1B: return 0x1B;  // Escape
        case 0xFF63: return 0x2E;  // Insert
        case 0xFFFF: return 0x2E;  // Delete
        case 0xFF50: return 0x24;  // Home
        case 0xFF57: return 0x23;  // End
        case 0xFF55: return 0x21;  // Page Up
        case 0xFF56: return 0x22;  // Page Down
        case 0xFF51: return 0x25;  // Left
        case 0xFF52: return 0x26;  // Up
        case 0xFF53: return 0x27;  // Right
        case 0xFF54: return 0x28;  // Down
        case 0xFFE1: return 0x10;  // Shift (L)
        case 0xFFE2: return 0x10;  // Shift (R)
        case 0xFFE3: return 0x11;  // Control (L)
        case 0xFFE4: return 0x11;  // Control (R)
        case 0xFFE9: return 0x12;  // Alt (L)
        case 0xFFEA: return 0x12;  // Alt (R)
        case 0xFFEB: return 0x5B;  // Super/Win (L)
        case 0xFFEC: return 0x5C;  // Super/Win (R)
        case 0xFF20: return 0x20;  // Space
        case 0xFF6B: return 0x14;  // Caps Lock
        case 0xFF7F: return 0x90;  // Num Lock
        case 0xFF14: return 0x91;  // Scroll Lock
        case 0xFF13: return 0x13;  // Pause
        case 0xFF61: return 0x2C;  // Print Screen
        case 0xFF15: return 0x2D;  // Help (Insert on some keyboards)
        
        // Numpad
        case 0xFFB0: return 0x60;  // Numpad 0
        case 0xFFB1: return 0x61;  // Numpad 1
        case 0xFFB2: return 0x62;  // Numpad 2
        case 0xFFB3: return 0x63;  // Numpad 3
        case 0xFFB4: return 0x64;  // Numpad 4
        case 0xFFB5: return 0x65;  // Numpad 5
        case 0xFFB6: return 0x66;  // Numpad 6
        case 0xFFB7: return 0x67;  // Numpad 7
        case 0xFFB8: return 0x68;  // Numpad 8
        case 0xFFB9: return 0x69;  // Numpad 9
        case 0xFFAE: return 0x6E;  // Numpad .
        case 0xFFAB: return 0x6B;  // Numpad +
        case 0xFFAD: return 0x6D;  // Numpad -
        case 0xFFAA: return 0x6A;  // Numpad *
        case 0xFFAF: return 0x6F;  // Numpad /
        case 0xFF8D: return 0x0D;  // Numpad Enter
        
        default: return x11_keysym & 0xFF;  // Default fallback
    }
}

MouseButton X11ButtonToMouseButton(uint32_t x11_button) {
    switch (x11_button) {
        case 1: return MouseButton::Left;
        case 2: return MouseButton::Middle;
        case 3: return MouseButton::Right;
        case 8: return MouseButton::XButton1;
        case 9: return MouseButton::XButton2;
        default: return MouseButton::Left;
    }
}

} // namespace vst3bridge
