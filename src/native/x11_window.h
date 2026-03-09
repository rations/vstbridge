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
 * @file x11_window.h
 * @brief X11 window management for plugin GUI
 */

#pragma once

#include <X11/Xlib.h>
#include <functional>

namespace vst3bridge {

/**
 * @brief X11 window wrapper for plugin GUI
 */
class X11Window {
public:
    X11Window();
    ~X11Window();

    // Non-copyable
    X11Window(const X11Window&) = delete;
    X11Window& operator=(const X11Window&) = delete;

    /**
     * @brief Create window
     * @param parent Parent window (0 for root)
     * @param x X position
     * @param y Y position
     * @param width Width
     * @param height Height
     * @return true on success
     */
    bool create(Window parent, int x, int y, int width, int height);

    /**
     * @brief Destroy window
     */
    void destroy();

    /**
     * @brief Show window
     */
    void show();

    /**
     * @brief Hide window
     */
    void hide();

    /**
     * @brief Resize window
     * @param width New width
     * @param height New height
     */
    void resize(int width, int height);

    /**
     * @brief Process pending X11 events
     */
    void processEvents();

    /**
     * @brief Get native X11 window handle
     * @return Window handle
     */
    Window getNativeWindow() const;

    /**
     * @brief Get X display connection
     * @return Display pointer
     */
    Display* getDisplay() const;

    /**
     * @brief Get window width
     * @return Width
     */
    int getWidth() const;

    /**
     * @brief Get window height
     * @return Height
     */
    int getHeight() const;

private:
    void handleEvent(const XEvent& event);

    Display* display_ = nullptr;
    Window window_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace vst3bridge
