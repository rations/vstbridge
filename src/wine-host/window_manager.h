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
 * @file window_manager.h
 * @brief Wine-side off-screen window management
 */

#pragma once

#include <windows.h>
#include <vector>

namespace vst3bridge {

/**
 * @brief Manages off-screen windows for plugin GUIs
 */
class WindowManager {
public:
    WindowManager();
    ~WindowManager();
    
    /**
     * @brief Initialize the window manager
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Shutdown the window manager
     */
    void shutdown();

    // Non-copyable
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    /**
     * @brief Create off-screen window
     * @param width Window width
     * @param height Window height
     * @return Window handle or nullptr
     */
    HWND createWindow(int width, int height);

    /**
     * @brief Destroy window
     * @param hwnd Window handle
     */
    void destroyWindow(HWND hwnd);

    /**
     * @brief Process window messages
     */
    void processMessages();

    /**
     * @brief Resize a window
     * @param hwnd Window handle
     * @param width New width
     * @param height New height
     */
    void resizeWindow(HWND hwnd, int width, int height);

    /**
     * @brief Get window client size
     * @param hwnd Window handle
     * @param width Output width
     * @param height Output height
     */
    void getWindowSize(HWND hwnd, int& width, int& height);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg,
                                       WPARAM wparam, LPARAM lparam);
    
    std::vector<HWND> windows_;
    bool initialized_ = false;
};

} // namespace vst3bridge
