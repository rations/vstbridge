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
 * @file gdi_capture.h
 * @brief GDI window content capture for Wine host
 */

#pragma once

#include <windows.h>
#include <cstdint>

namespace vst3bridge {

/**
 * @brief Captures window contents using GDI
 */
class GDICapture {
public:
    GDICapture();
    ~GDICapture();

    // Non-copyable
    GDICapture(const GDICapture&) = delete;
    GDICapture& operator=(const GDICapture&) = delete;

    /**
     * @brief Initialize capture for a window
     * @param hwnd Window to capture
     * @return true on success
     */
    bool initialize(HWND hwnd);

    /**
     * @brief Capture frame to buffer
     * @param buffer Output buffer
     * @param width Buffer width
     * @param height Buffer height
     * @return true on success
     */
    bool capture(uint8_t* buffer, int width, int height);

    /**
     * @brief Check if content has changed
     * @return true if window content changed
     */
    bool hasChanged() const;

    /**
     * @brief Get capture width
     * @return Width in pixels
     */
    int getWidth() const;

    /**
     * @brief Get capture height
     * @return Height in pixels
     */
    int getHeight() const;

private:
    HWND hwnd_ = nullptr;
    HDC hdc_window_ = nullptr;
    HDC hdc_mem_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace vst3bridge
