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
 * @file gdi_capture.cpp
 * @brief GDI window content capture for Wine host
 * 
 * Captures window contents using GDI BitBlt and converts to BGRA format
 * for transmission to the native Linux side via shared memory.
 */

#include "gdi_capture.h"

// CAPTUREBLT flag for capturing layered windows
#ifndef CAPTUREBLT
#define CAPTUREBLT 0x40000000
#endif
#include <cstring>
#include <vector>
#include <algorithm>

namespace vst3bridge {

GDICapture::GDICapture() = default;

GDICapture::~GDICapture() {
    cleanup();
}

bool GDICapture::initialize(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }
    
    cleanup();
    
    hwnd_ = hwnd;
    
    // Get window dimensions
    RECT rect;
    if (!GetClientRect(hwnd_, &rect)) {
        return false;
    }
    
    width_ = rect.right - rect.left;
    height_ = rect.bottom - rect.top;
    
    if (width_ <= 0 || height_ <= 0) {
        return false;
    }
    
    // Get window DC
    hdc_window_ = GetDC(hwnd_);
    if (!hdc_window_) {
        return false;
    }
    
    // Create compatible memory DC
    hdc_mem_ = CreateCompatibleDC(hdc_window_);
    if (!hdc_mem_) {
        cleanup();
        return false;
    }
    
    // Create DIB section for efficient pixel access
    BITMAPINFO bmi;
    std::memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_;  // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;      // BGRA format
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = width_ * height_ * 4;
    
    void* bits = nullptr;
    bitmap_ = CreateDIBSection(hdc_mem_, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap_) {
        cleanup();
        return false;
    }
    
    // Select bitmap into memory DC
    SelectObject(hdc_mem_, bitmap_);
    
    return true;
}

void GDICapture::cleanup() {
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    
    if (hdc_mem_) {
        DeleteDC(hdc_mem_);
        hdc_mem_ = nullptr;
    }
    
    if (hdc_window_ && hwnd_) {
        ReleaseDC(hwnd_, hdc_window_);
        hdc_window_ = nullptr;
    }
    
    hwnd_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool GDICapture::capture(uint8_t* buffer, int width, int height) {
    if (!hdc_window_ || !hdc_mem_ || !bitmap_ || !buffer) {
        return false;
    }
    
    // Check if window size changed
    RECT rect;
    if (!GetClientRect(hwnd_, &rect)) {
        return false;
    }
    
    int new_width = rect.right - rect.left;
    int new_height = rect.bottom - rect.top;
    
    if (new_width != width_ || new_height != height_) {
        // Window resized - reinitialize
        if (!initialize(hwnd_)) {
            return false;
        }
    }
    
    // Capture window to memory DC
    // Use SRCCOPY for simple copy, or CAPTUREBLT to capture layered windows
    BOOL result = BitBlt(hdc_mem_, 0, 0, width_, height_, 
                         hdc_window_, 0, 0, SRCCOPY | CAPTUREBLT);
    
    if (!result) {
        return false;
    }
    
    // Get DIB bits
    BITMAPINFO bmi;
    std::memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // Allocate temporary buffer for GetDIBits
    std::vector<uint8_t> temp_buffer(width_ * height_ * 4);
    
    int lines = GetDIBits(hdc_mem_, bitmap_, 0, height_, 
                          temp_buffer.data(), &bmi, DIB_RGB_COLORS);
    
    if (lines != height_) {
        return false;
    }
    
    // Copy to output buffer, converting if needed
    if (width == width_ && height == height_) {
        // Same size - direct copy (already in BGRA format)
        std::memcpy(buffer, temp_buffer.data(), width_ * height_ * 4);
    } else {
        // Different size - need to scale
        // Simple nearest-neighbor scaling for now
        float x_ratio = static_cast<float>(width_) / width;
        float y_ratio = static_cast<float>(height_) / height;
        
        for (int y = 0; y < height; ++y) {
            int src_y = static_cast<int>(y * y_ratio);
            src_y = std::min(src_y, height_ - 1);
            
            for (int x = 0; x < width; ++x) {
                int src_x = static_cast<int>(x * x_ratio);
                src_x = std::min(src_x, width_ - 1);
                
                int src_idx = (src_y * width_ + src_x) * 4;
                int dst_idx = (y * width + x) * 4;
                
                // Copy BGRA pixel
                buffer[dst_idx + 0] = temp_buffer[src_idx + 0];  // B
                buffer[dst_idx + 1] = temp_buffer[src_idx + 1];  // G
                buffer[dst_idx + 2] = temp_buffer[src_idx + 2];  // R
                buffer[dst_idx + 3] = temp_buffer[src_idx + 3];  // A
            }
        }
    }
    
    return true;
}

bool GDICapture::hasChanged() const {
    // TODO: Implement efficient change detection
    // Could use a checksum of the previous frame or GDI's change detection
    // For now, always return true to indicate capture is needed
    return true;
}

int GDICapture::getWidth() const {
    return width_;
}

int GDICapture::getHeight() const {
    return height_;
}

} // namespace vst3bridge
