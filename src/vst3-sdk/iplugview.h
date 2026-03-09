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
 * @file iplugview.h
 * @brief VST3 Plugin View interface
 * 
 * Based on Steinberg VST3 SDK specification
 */

#pragma once

#include "funknown.h"

namespace Steinberg {

// Forward declarations
class IPlugView;
class IPlugFrame;

// String types
using FIDString = const char8*;

/**
 * @brief Platform types for plugin views
 */
namespace PlatformType {
    const char8 kHWND[] = "HWND";       ///< Windows HWND
    const char8 kHIView[] = "HIView";   ///< macOS HIView
    const char8 kNSView[] = "NSView";   ///< macOS NSView
    const char8 kUIView[] = "UIView";   ///< iOS UIView
    const char8 kX11WindowID[] = "X11WindowID";  ///< Linux X11
    const char8 kLV2UI_Widget[] = "LV2UI_Widget"; ///< LV2
} // namespace PlatformType

/**
 * @brief View types
 */
namespace ViewType {
    const char8 kEditor[] = "editor";
} // namespace ViewType

/**
 * @brief View rectangle structure
 */
struct ViewRect {
    int32 left;
    int32 top;
    int32 right;
    int32 bottom;

    ViewRect() : left(0), top(0), right(0), bottom(0) {}
    ViewRect(int32 l, int32 t, int32 r, int32 b) : left(l), top(t), right(r), bottom(b) {}

    int32 getWidth() const { return right - left; }
    int32 getHeight() const { return bottom - top; }
};

/**
 * @brief IPlugFrame - Host interface for plugin view
 */
class IPlugFrame : public FUnknown {
public:
    /**
     * @brief Resize the view
     * @param view The view to resize
     * @param newSize New size
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* newSize) = 0;

    static const FUID iid;
};

/**
 * @brief IPlugView - Plugin GUI interface
 * 
 * Handles the plugin's visual editor.
 */
class IPlugView : public FUnknown {
public:
    /**
     * @brief Check if platform is supported
     * @param type Platform type string
     * @return kResultOk if supported
     */
    virtual tresult PLUGIN_API isPlatformTypeSupported(FIDString type) = 0;

    /**
     * @brief Attach the view to a parent window
     * @param parent Platform-specific parent window handle (e.g., X11 Window ID)
     * @param type Platform type
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API attached(void* parent, FIDString type) = 0;

    /**
     * @brief Detach the view from parent
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API removed() = 0;

    /**
     * @brief Handle on-wheel event
     * @param distance Scroll distance
     * @return kResultTrue if handled, kResultFalse if not
     */
    virtual tresult PLUGIN_API onWheel(float distance) = 0;

    /**
     * @brief Handle horizontal on-wheel event
     * @param distance Scroll distance
     * @return kResultTrue if handled, kResultFalse if not
     */
    virtual tresult PLUGIN_API onHorizWheel(float distance) = 0;

    /**
     * @brief Handle keyboard event
     * @param key Key character
     * @param keyCode Key code
     * @param modifiers Modifier keys
     * @return kResultTrue if handled, kResultFalse if not
     */
    virtual tresult PLUGIN_API onKeyDown(char16 key, int16 keyCode, int16 modifiers) = 0;

    /**
     * @brief Handle key up event
     * @param key Key character
     * @param keyCode Key code
     * @param modifiers Modifier keys
     * @return kResultTrue if handled, kResultFalse if not
     */
    virtual tresult PLUGIN_API onKeyUp(char16 key, int16 keyCode, int16 modifiers) = 0;

    /**
     * @brief Get view size
     * @param size Output size
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API getSize(ViewRect* size) = 0;

    /**
     * @brief Set view size
     * @param newSize New size
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API onSize(ViewRect* newSize) = 0;

    /**
     * @brief Focus in
     * @param state true for focus in, false for focus out
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API onFocus(bool state) = 0;

    /**
     * @brief Set frame (host interface for view)
     * @param frame Frame interface
     * @return kResultOk on success
     */
    virtual tresult PLUGIN_API setFrame(IPlugFrame* frame) = 0;

    /**
     * @brief Can the view be resized?
     * @return kResultTrue if resizable, kResultFalse if not
     */
    virtual tresult PLUGIN_API canResize() = 0;

    /**
     * @brief Check if size is valid
     * @param rect Size to check
     * @return kResultTrue if valid, kResultFalse if not
     */
    virtual tresult PLUGIN_API checkSizeConstraint(ViewRect* rect) = 0;

    static const FUID iid;
};

// Interface IDs
inline constexpr FUID IPlugView_iid(0x5BC6C74C, 0x6F2D4E45, 0xA3D9F7E2, 0x4B8C5A1E);
inline constexpr FUID IPlugFrame_iid(0x7A8F9B3C, 0x4E5D6F18, 0xB7A2C9E3, 0x5D4F8B1A);

} // namespace Steinberg
