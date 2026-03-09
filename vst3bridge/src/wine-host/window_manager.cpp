// vst3bridge: A modern Wine VST3 bridge for Linux
// Copyright (C) 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.

#include <windows.h>

namespace vst3bridge {

// WindowManager - Creates and manages off-screen Win32 windows
// The plugin renders to these windows, then we capture the contents

class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    // Create off-screen window for plugin
    HWND CreateOffscreenWindow(int width, int height);
    
    // Destroy window
    void DestroyWindow(HWND window);
    
    // Resize window
    bool ResizeWindow(HWND window, int width, int height);
    
    // Get X11 window handle from Win32 HWND
    // This allows us to use X11 Composite on the Wine side
    unsigned long GetX11Window(HWND window);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, 
                                       WPARAM wParam, LPARAM lParam);
    static bool RegisterWindowClass();
    
    static const wchar_t* WINDOW_CLASS_NAME;
    static bool class_registered_;
};

const wchar_t* WindowManager::WINDOW_CLASS_NAME = L"VST3BridgeOffscreenWindow";
bool WindowManager::class_registered_ = false;

WindowManager::WindowManager() {
    RegisterWindowClass();
}

WindowManager::~WindowManager() {
    // Cleanup
}

bool WindowManager::RegisterWindowClass() {
    if (class_registered_) {
        return true;
    }
    
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassExW(&wcex)) {
        return false;
    }
    
    class_registered_ = true;
    return true;
}

HWND WindowManager::CreateOffscreenWindow(int width, int height) {
    // Create window at off-screen coordinates
    HWND window = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        WINDOW_CLASS_NAME,
        L"VST3Bridge",
        WS_POPUP,
        -10000, -10000,  // Off-screen position
        width, height,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    return window;
}

void WindowManager::DestroyWindow(HWND window) {
    if (window) {
        DestroyWindow(window);
    }
}

bool WindowManager::ResizeWindow(HWND window, int width, int height) {
    return SetWindowPos(window, nullptr, 0, 0, width, height,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

unsigned long WindowManager::GetX11Window(HWND window) {
    // Wine-specific: Get the underlying X11 window handle
    // This is Wine-specific and uses Wine internals
    // TODO: Implement using Wine's HWND to X11 window mapping
    return 0;
}

LRESULT CALLBACK WindowManager::WindowProc(HWND hwnd, UINT uMsg,
                                           WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

} // namespace vst3bridge
