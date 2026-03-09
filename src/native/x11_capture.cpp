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
 * @file x11_capture.cpp
 * @brief X11 window content capture for off-screen Wine windows
 * 
 * Uses X11 Composite extension to capture Wine window contents without
 * requiring window embedding.
 */

#include "x11_capture.h"
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace vst3bridge {

X11Capture::X11Capture(Display* display, Window target_window)
    : display_(display), target_window_(target_window) {
}

X11Capture::~X11Capture() {
    cleanup();
}

bool X11Capture::initialize() {
    if (!display_ || !target_window_) {
        return false;
    }
    
    // Check for Composite extension
    int event_base, error_base;
    if (!XCompositeQueryExtension(display_, &event_base, &error_base)) {
        std::cerr << "XComposite extension not available\n";
        return false;
    }
    
    int major = 0, minor = 0;
    XCompositeQueryVersion(display_, &major, &minor);
    
    // Redirect the window to off-screen
    XCompositeRedirectWindow(display_, target_window_, CompositeRedirectAutomatic);
    
    // Create pixmap from window
    XWindowAttributes attrs;
    XGetWindowAttributes(display_, target_window_, &attrs);
    width_ = attrs.width;
    height_ = attrs.height;
    
    pixmap_ = XCompositeNameWindowPixmap(display_, target_window_);
    if (!pixmap_) {
        std::cerr << "Failed to create pixmap from window\n";
        return false;
    }
    
    // Set up XShm if available
    setupXShm();
    
    return true;
}

void X11Capture::cleanup() {
    if (image_) {
        XDestroyImage(image_);
        image_ = nullptr;
    }
    
    if (pixmap_) {
        XFreePixmap(display_, pixmap_);
        pixmap_ = 0;
    }
    
    if (target_window_) {
        XCompositeUnredirectWindow(display_, target_window_, CompositeRedirectAutomatic);
    }
}

bool X11Capture::setupXShm() {
    // XShm setup for efficient pixel transfer
    // Check if XShm extension is available (optional optimization)
    int major, minor;
    int pixmaps;
    
    if (XShmQueryVersion(display_, &major, &minor, &pixmaps)) {
        std::cout << "XShm extension available: " << major << "." << minor
                  << " (pixmaps: " << (pixmaps ? "yes" : "no") << ")\n";
        // TODO: Implement XShm image creation for better performance
        // This would use XShmCreateImage and shared memory segment
        // For now, fall back to standard XGetImage
    }
    
    return false; // Standard XGetImage will be used
}

bool X11Capture::captureFrame(uint8_t* buffer, int width, int height) {
    if (!display_ || !pixmap_ || !buffer) {
        return false;
    }
    
    // Get image from pixmap
    XImage* image = XGetImage(display_, pixmap_, 0, 0, width_, height_,
                               AllPlanes, ZPixmap);
    if (!image) {
        return false;
    }
    
    // Convert to BGRA format
    // X11 typically returns in host byte order, need to convert to standardized BGRA
    const int src_width = width_;
    const int src_height = height_;
    const int dst_width = width;
    const int dst_height = height;
    
    // Simple normalization factors for scaling (if dimensions differ)
    const double x_scale = static_cast<double>(src_width) / dst_width;
    const double y_scale = static_cast<double>(src_height) / dst_height;
    
    // Convert pixel data to BGRA
    for (int y = 0; y < dst_height; ++y) {
        for (int x = 0; x < dst_width; ++x) {
            // Sample from source using nearest neighbor for simplicity
            const int src_x = static_cast<int>(x * x_scale);
            const int src_y = static_cast<int>(y * y_scale);
            
            // Get pixel from source (handle potential bounds issues)
            if (src_x >= 0 && src_x < src_width && src_y >= 0 && src_y < src_height) {
                unsigned long pixel = XGetPixel(image, src_x, src_y);
                
                // Convert from whatever format X11 returned to BGRA
                // This is simplified - assumes 24/32-bit RGB in host order
                const uint8_t r = (pixel >> 16) & 0xFF;
                const uint8_t g = (pixel >> 8) & 0xFF;
                const uint8_t b = pixel & 0xFF;
                
                // Write BGRA to destination buffer
                const int dst_idx = (y * dst_width + x) * 4;
                buffer[dst_idx + 0] = b;     // Blue
                buffer[dst_idx + 1] = g;     // Green
                buffer[dst_idx + 2] = r;     // Red
                buffer[dst_idx + 3] = 0xFF;  // Alpha (opaque)
            }
        }
    }
    
    XDestroyImage(image);
    return true;
}

bool X11Capture::hasNewContent() {
    // Check if window has been damaged (new content available)
    // TODO: Implement damage checking
    return true;
}

void X11Capture::updatePixmap() {
    // Update pixmap if window has resized
    if (!pixmap_) return;
    
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display_, target_window_, &attrs)) {
        return;
    }
    
    if (attrs.width != width_ || attrs.height != height_) {
        // Window resized, recreate pixmap
        XFreePixmap(display_, pixmap_);
        pixmap_ = XCompositeNameWindowPixmap(display_, target_window_);
        width_ = attrs.width;
        height_ = attrs.height;
    }
}

int X11Capture::getWidth() const {
    return width_;
}

int X11Capture::getHeight() const {
    return height_;
}

} // namespace vst3bridge
