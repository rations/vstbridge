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
 * @file x11_capture.h
 * @brief X11 window content capture for off-screen Wine windows
 */

#pragma once

#include <X11/Xlib.h>
#include <cstdint>

namespace vst3bridge {

/**
 * @brief Captures content from X11 windows using Composite extension
 * 
 * This class captures window contents without requiring the window to be
 * visible or embedded, using the X Composite extension.
 */
class X11Capture {
public:
    /**
     * @brief Construct capture object
     * @param display X display connection
     * @param target_window Window to capture
     */
    X11Capture(Display* display, Window target_window);
    
    ~X11Capture();

    // Non-copyable
    X11Capture(const X11Capture&) = delete;
    X11Capture& operator=(const X11Capture&) = delete;

    /**
     * @brief Initialize capture
     * @return true on success
     */
    bool initialize();

    /**
     * @brief Cleanup resources
     */
    void cleanup();

    /**
     * @brief Capture frame into buffer
     * @param buffer Output buffer (must be large enough for width*height*4 bytes)
     * @param width Target width (for scaling)
     * @param height Target height (for scaling)
     * @return true on success
     */
    bool captureFrame(uint8_t* buffer, int width, int height);

    /**
     * @brief Check if window has new content
     * @return true if new content available
     */
    bool hasNewContent();

    /**
     * @brief Update pixmap if window resized
     */
    void updatePixmap();

    /**
     * @brief Get captured width
     * @return Width
     */
    int getWidth() const;

    /**
     * @brief Get captured height
     * @return Height
     */
    int getHeight() const;

private:
    bool setupXShm();

    Display* display_ = nullptr;
    Window target_window_ = 0;
    Pixmap pixmap_ = 0;
    XImage* image_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace vst3bridge
