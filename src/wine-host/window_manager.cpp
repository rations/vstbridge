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
 * @file window_manager.cpp
 * @brief Wine-side off-screen window management
 * 
 * Creates and manages windows at off-screen coordinates for capture.
 * Windows are positioned at (-10000, -10000) so they're not visible
 * but can still be rendered and captured via GDI.
 */

#include "window_manager.h"
#include <cstring>

namespace vst3bridge {

// Off-screen position - far enough to be invisible
constexpr int OFFSCREEN_X = -10000;
constexpr int OFFSCREEN_Y = -10000;

// Window class name
constexpr wchar_t WINDOW_CLASS_NAME[] = L"VST3BridgePluginWindow";

WindowManager::WindowManager() : initialized_(false) {
}

WindowManager::~WindowManager() {
    if (initialized_) {
        // Unregister window class
        UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandle(nullptr));
        initialized_ = false;
    }
}

bool WindowManager::initialize() {
    if (initialized_) {
        return true; // Already initialized
    }
    
    // Register window class
    WNDCLASSEXW wcex;
    std::memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = windowProc;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    if (!RegisterClassExW(&wcex)) {
        return false; // Failed to register window class
    }
    
    initialized_ = true;
    return true;
}

HWND WindowManager::createWindow(int width, int height) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    
    // Create window off-screen
    HWND hwnd = CreateWindowExW(
        0,                          // Extended style
        WINDOW_CLASS_NAME,          // Window class
        L"VST3 Plugin",             // Window title
        WS_POPUP | WS_CLIPCHILDREN, // Style - popup window, no borders
        OFFSCREEN_X,                // X position (off-screen)
        OFFSCREEN_Y,                // Y position (off-screen)
        width,                      // Width
        height,                     // Height
        nullptr,                    // Parent window
        nullptr,                    // Menu
        hInstance,                  // Instance
        nullptr                     // User data
    );
    
    if (!hwnd) {
        return nullptr;
    }
    
    // Store window handle
    windows_.push_back(hwnd);
    
    // Show the window (even though it's off-screen, it needs to be "shown"
    // for the plugin to render to it)
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    return hwnd;
}

void WindowManager::destroyWindow(HWND hwnd) {
    if (!hwnd) return;
    
    // Remove from list
    auto it = std::find(windows_.begin(), windows_.end(), hwnd);
    if (it != windows_.end()) {
        windows_.erase(it);
    }
    
    DestroyWindow(hwnd);
}

void WindowManager::processMessages() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK WindowManager::windowProc(HWND hwnd, UINT msg, 
                                           WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            return 0;
            
        case WM_DESTROY:
            return 0;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // Clear to black (or transparent)
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_ERASEBKGND:
            // Prevent flicker
            return 1;
            
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

void WindowManager::resizeWindow(HWND hwnd, int width, int height) {
    if (!hwnd) return;
    
    SetWindowPos(hwnd, nullptr, OFFSCREEN_X, OFFSCREEN_Y, 
                 width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowManager::getWindowSize(HWND hwnd, int& width, int& height) {
    if (!hwnd) {
        width = 0;
        height = 0;
        return;
    }
    
    RECT rect;
    if (GetClientRect(hwnd, &rect)) {
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    } else {
        width = 0;
        height = 0;
    }
}

void WindowManager::shutdown() {
    // Destroy all windows
    for (HWND hwnd : windows_) {
        if (hwnd) {
            DestroyWindow(hwnd);
        }
    }
    windows_.clear();
    
    if (initialized_) {
        // Unregister window class
        UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandle(nullptr));
        initialized_ = false;
    }
}

} // namespace vst3bridge
