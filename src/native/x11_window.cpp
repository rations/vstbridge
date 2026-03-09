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
 * @file x11_window.cpp
 * @brief X11 window management for plugin GUI
 */

#include "x11_window.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstring>
#include <iostream>

namespace vst3bridge {

X11Window::X11Window() = default;

X11Window::~X11Window() {
    destroy();
}

bool X11Window::create(Window parent, int x, int y, int width, int height) {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        return false;
    }
    
    int screen = DefaultScreen(display_);
    Window root = RootWindow(display_, screen);
    
    // Use provided parent or root
    Window parent_window = parent ? parent : root;
    
    XSetWindowAttributes attrs;
    attrs.background_pixel = WhitePixel(display_, screen);
    attrs.border_pixel = BlackPixel(display_, screen);
    attrs.event_mask = ExposureMask | StructureNotifyMask | 
                       ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | KeyPressMask | KeyReleaseMask;
    
    window_ = XCreateWindow(display_, parent_window,
                            x, y, width, height,
                            0, 
                            DefaultDepth(display_, screen),
                            InputOutput,
                            DefaultVisual(display_, screen),
                            CWBackPixel | CWBorderPixel | CWEventMask,
                            &attrs);
    
    if (!window_) {
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }
    
    // Set window title
    XStoreName(display_, window_, "VST3 Plugin");
    
    // Select input events
    XSelectInput(display_, window_, attrs.event_mask);
    
    // Map window
    XMapWindow(display_, window_);
    XFlush(display_);
    
    width_ = width;
    height_ = height;
    
    return true;
}

void X11Window::destroy() {
    if (display_) {
        if (window_) {
            XDestroyWindow(display_, window_);
            window_ = 0;
        }
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

void X11Window::show() {
    if (display_ && window_) {
        XMapRaised(display_, window_);
        XFlush(display_);
    }
}

void X11Window::hide() {
    if (display_ && window_) {
        XUnmapWindow(display_, window_);
        XFlush(display_);
    }
}

void X11Window::resize(int width, int height) {
    if (display_ && window_) {
        XResizeWindow(display_, window_, width, height);
        XFlush(display_);
        width_ = width;
        height_ = height;
    }
}

void X11Window::processEvents() {
    if (!display_) return;
    
    XEvent event;
    while (XPending(display_) > 0) {
        XNextEvent(display_, &event);
        handleEvent(event);
    }
}

void X11Window::handleEvent(const XEvent& event) {
    switch (event.type) {
        case Expose:
            if (event.xexpose.count == 0) {
                // Window needs redraw
                // TODO: Trigger redraw
            }
            break;
            
        case ConfigureNotify:
            if (event.xconfigure.width != width_ || 
                event.xconfigure.height != height_) {
                width_ = event.xconfigure.width;
                height_ = event.xconfigure.height;
                // TODO: Notify of resize
            }
            break;
            
        case ButtonPress:
        case ButtonRelease:
            // TODO: Forward to Wine host
            break;
            
        case MotionNotify:
            // TODO: Forward to Wine host
            break;
            
        case KeyPress:
        case KeyRelease:
            // TODO: Forward to Wine host
            break;
            
        case ClientMessage:
            // Check for window manager close
            // TODO: Handle close
            break;
    }
}

Window X11Window::getNativeWindow() const {
    return window_;
}

Display* X11Window::getDisplay() const {
    return display_;
}

int X11Window::getWidth() const {
    return width_;
}

int X11Window::getHeight() const {
    return height_;
}

} // namespace vst3bridge
