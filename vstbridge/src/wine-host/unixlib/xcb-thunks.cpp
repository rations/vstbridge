// vstbridge: a Wine VST/CLAP plugin bridge
// Copyright (C) 2020-2024 Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// The xcb functions used by the PE plugin host, implemented by forwarding them
// to libxcb in the unixlib. See `xcb-calls.h`.

#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "../xcb-native.h"

#include "pe.h"
#include "xcb-calls.h"

using namespace unixlib;

namespace {

template <typename T>
uint64_t to_arg(T value) {
    if constexpr (std::is_null_pointer_v<T>) {
        return 0;
    } else if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<uintptr_t>(value);
    } else {
        return static_cast<uint64_t>(value);
    }
}

template <typename... Ts>
XcbArgs xcb(XcbOp op, Ts... values) {
    static_assert(sizeof...(Ts) <= std::size(XcbArgs{}.args));

    XcbArgs args{};
    args.op = op;
    size_t i = 0;
    ((args.args[i++] = to_arg(values)), ...);

    call(unixlib::xcb_call, &args);
    return args;
}

/**
 * Copy something libxcb allocated into memory allocated with this side's
 * `malloc()`, so the host code can `free()` it.
 */
void* copy_packet(const void* packet, uint64_t size) {
    if (!packet) {
        return nullptr;
    }

    void* copy = malloc(size);
    memcpy(copy, packet, size);

    return copy;
}

/**
 * Copy the reply and error from a reply function call, and free libxcb's
 * originals.
 */
template <typename T>
T* take_reply(const XcbArgs& args, xcb_generic_error_t** e) {
    T* reply = static_cast<T*>(copy_packet(args.packet, args.packet_size));
    auto* error = static_cast<xcb_generic_error_t*>(
        copy_packet(args.error, args.error_size));
    if (args.packet || args.error) {
        xcb(xcb_op_free, args.packet, args.error);
    }

    if (e) {
        *e = error;
    } else {
        // Without an error pointer libxcb reports the error as an event
        // instead, so this shouldn't happen
        free(error);
    }

    return reply;
}

template <typename Cookie>
Cookie cookie(const XcbArgs& args) {
    return Cookie{static_cast<unsigned int>(args.result)};
}

}  // namespace

extern "C" {

xcb_connection_t* xcb_connect(const char* displayname, int* screenp) {
    return reinterpret_cast<xcb_connection_t*>(
        xcb(xcb_op_connect, displayname, screenp).result);
}

void xcb_disconnect(xcb_connection_t* c) {
    xcb(xcb_op_disconnect, c);
}

int xcb_flush(xcb_connection_t* c) {
    return static_cast<int>(xcb(xcb_op_flush, c).result);
}

uint32_t xcb_generate_id(xcb_connection_t* c) {
    return static_cast<uint32_t>(xcb(xcb_op_generate_id, c).result);
}

// The setup data belongs to the connection and is never freed by the caller,
// so this can point to libxcb's copy
const struct xcb_setup_t* xcb_get_setup(xcb_connection_t* c) {
    return reinterpret_cast<const xcb_setup_t*>(
        xcb(xcb_op_get_setup, c).result);
}

xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t* R) {
    xcb_screen_iterator_t iterator{};
    xcb(xcb_op_setup_roots_iterator, R, &iterator);

    return iterator;
}

void xcb_screen_next(xcb_screen_iterator_t* i) {
    xcb(xcb_op_screen_next, i);
}

xcb_generic_event_t* xcb_poll_for_event(xcb_connection_t* c) {
    const XcbArgs args = xcb(xcb_op_poll_for_event, c);
    if (!args.packet) {
        return nullptr;
    }

    auto* event = static_cast<xcb_generic_event_t*>(
        copy_packet(args.packet, args.packet_size));
    xcb(xcb_op_free, args.packet, nullptr);

    return event;
}

xcb_generic_error_t* xcb_request_check(xcb_connection_t* c,
                                       xcb_void_cookie_t cookie) {
    const XcbArgs args = xcb(xcb_op_request_check, c, cookie.sequence);
    if (!args.error) {
        return nullptr;
    }

    auto* error = static_cast<xcb_generic_error_t*>(
        copy_packet(args.error, args.error_size));
    xcb(xcb_op_free, args.error, nullptr);

    return error;
}

xcb_void_cookie_t xcb_change_property(xcb_connection_t* c,
                                      uint8_t mode,
                                      xcb_window_t window,
                                      xcb_atom_t property,
                                      xcb_atom_t type,
                                      uint8_t format,
                                      uint32_t data_len,
                                      const void* data) {
    return cookie<xcb_void_cookie_t>(xcb(xcb_op_change_property, c, mode,
                                         window, property, type, format,
                                         data_len, data));
}

xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t* c,
                                               xcb_window_t window,
                                               uint32_t value_mask,
                                               const void* value_list) {
    return cookie<xcb_void_cookie_t>(xcb(xcb_op_change_window_attributes, c,
                                         window, value_mask, value_list));
}

xcb_void_cookie_t xcb_configure_window(xcb_connection_t* c,
                                       xcb_window_t window,
                                       uint16_t value_mask,
                                       const void* value_list) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_configure_window, c, window, value_mask, value_list));
}

xcb_void_cookie_t xcb_create_window(xcb_connection_t* c,
                                    uint8_t depth,
                                    xcb_window_t wid,
                                    xcb_window_t parent,
                                    int16_t x,
                                    int16_t y,
                                    uint16_t width,
                                    uint16_t height,
                                    uint16_t border_width,
                                    uint16_t _class,
                                    xcb_visualid_t visual,
                                    uint32_t value_mask,
                                    const void* value_list) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_create_window, c, depth, wid, parent, x, y, width, height,
            border_width, _class, visual, value_mask, value_list));
}

xcb_void_cookie_t xcb_delete_property(xcb_connection_t* c,
                                      xcb_window_t window,
                                      xcb_atom_t property) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_delete_property, c, window, property));
}

xcb_void_cookie_t xcb_destroy_window(xcb_connection_t* c,
                                     xcb_window_t window) {
    return cookie<xcb_void_cookie_t>(xcb(xcb_op_destroy_window, c, window));
}

xcb_get_input_focus_cookie_t xcb_get_input_focus(xcb_connection_t* c) {
    return cookie<xcb_get_input_focus_cookie_t>(
        xcb(xcb_op_get_input_focus, c));
}

xcb_get_input_focus_reply_t* xcb_get_input_focus_reply(
    xcb_connection_t* c,
    xcb_get_input_focus_cookie_t cookie,
    xcb_generic_error_t** e) {
    return take_reply<xcb_get_input_focus_reply_t>(
        xcb(xcb_op_get_input_focus_reply, c, cookie.sequence, e != nullptr),
        e);
}

xcb_get_keyboard_mapping_cookie_t xcb_get_keyboard_mapping(
    xcb_connection_t* c,
    xcb_keycode_t first_keycode,
    uint8_t count) {
    return cookie<xcb_get_keyboard_mapping_cookie_t>(
        xcb(xcb_op_get_keyboard_mapping, c, first_keycode, count));
}

xcb_get_keyboard_mapping_reply_t* xcb_get_keyboard_mapping_reply(
    xcb_connection_t* c,
    xcb_get_keyboard_mapping_cookie_t cookie,
    xcb_generic_error_t** e) {
    return take_reply<xcb_get_keyboard_mapping_reply_t>(
        xcb(xcb_op_get_keyboard_mapping_reply, c, cookie.sequence,
            e != nullptr),
        e);
}

xcb_keysym_t* xcb_get_keyboard_mapping_keysyms(
    const xcb_get_keyboard_mapping_reply_t* R) {
    return reinterpret_cast<xcb_keysym_t*>(
        xcb(xcb_op_get_keyboard_mapping_keysyms, R).result);
}

int xcb_get_keyboard_mapping_keysyms_length(
    const xcb_get_keyboard_mapping_reply_t* R) {
    return static_cast<int>(
        xcb(xcb_op_get_keyboard_mapping_keysyms_length, R).result);
}

xcb_get_property_cookie_t xcb_get_property(xcb_connection_t* c,
                                           uint8_t _delete,
                                           xcb_window_t window,
                                           xcb_atom_t property,
                                           xcb_atom_t type,
                                           uint32_t long_offset,
                                           uint32_t long_length) {
    return cookie<xcb_get_property_cookie_t>(xcb(xcb_op_get_property, c,
                                                 _delete, window, property,
                                                 type, long_offset,
                                                 long_length));
}

xcb_get_property_reply_t* xcb_get_property_reply(
    xcb_connection_t* c,
    xcb_get_property_cookie_t cookie,
    xcb_generic_error_t** e) {
    return take_reply<xcb_get_property_reply_t>(
        xcb(xcb_op_get_property_reply, c, cookie.sequence, e != nullptr), e);
}

void* xcb_get_property_value(const xcb_get_property_reply_t* R) {
    return reinterpret_cast<void*>(xcb(xcb_op_get_property_value, R).result);
}

xcb_void_cookie_t xcb_grab_key(xcb_connection_t* c,
                               uint8_t owner_events,
                               xcb_window_t grab_window,
                               uint16_t modifiers,
                               xcb_keycode_t key,
                               uint8_t pointer_mode,
                               uint8_t keyboard_mode) {
    return cookie<xcb_void_cookie_t>(xcb(xcb_op_grab_key, c, owner_events,
                                         grab_window, modifiers, key,
                                         pointer_mode, keyboard_mode));
}

xcb_intern_atom_cookie_t xcb_intern_atom(xcb_connection_t* c,
                                         uint8_t only_if_exists,
                                         uint16_t name_len,
                                         const char* name) {
    return cookie<xcb_intern_atom_cookie_t>(
        xcb(xcb_op_intern_atom, c, only_if_exists, name_len, name));
}

xcb_intern_atom_reply_t* xcb_intern_atom_reply(xcb_connection_t* c,
                                               xcb_intern_atom_cookie_t cookie,
                                               xcb_generic_error_t** e) {
    return take_reply<xcb_intern_atom_reply_t>(
        xcb(xcb_op_intern_atom_reply, c, cookie.sequence, e != nullptr), e);
}

xcb_void_cookie_t xcb_map_window(xcb_connection_t* c, xcb_window_t window) {
    return cookie<xcb_void_cookie_t>(xcb(xcb_op_map_window, c, window));
}

xcb_query_pointer_cookie_t xcb_query_pointer(xcb_connection_t* c,
                                             xcb_window_t window) {
    return cookie<xcb_query_pointer_cookie_t>(
        xcb(xcb_op_query_pointer, c, window));
}

xcb_query_pointer_reply_t* xcb_query_pointer_reply(
    xcb_connection_t* c,
    xcb_query_pointer_cookie_t cookie,
    xcb_generic_error_t** e) {
    return take_reply<xcb_query_pointer_reply_t>(
        xcb(xcb_op_query_pointer_reply, c, cookie.sequence, e != nullptr), e);
}

xcb_query_tree_cookie_t xcb_query_tree(xcb_connection_t* c,
                                       xcb_window_t window) {
    return cookie<xcb_query_tree_cookie_t>(xcb(xcb_op_query_tree, c, window));
}

xcb_query_tree_reply_t* xcb_query_tree_reply(xcb_connection_t* c,
                                             xcb_query_tree_cookie_t cookie,
                                             xcb_generic_error_t** e) {
    return take_reply<xcb_query_tree_reply_t>(
        xcb(xcb_op_query_tree_reply, c, cookie.sequence, e != nullptr), e);
}

xcb_void_cookie_t xcb_reparent_window(xcb_connection_t* c,
                                      xcb_window_t window,
                                      xcb_window_t parent,
                                      int16_t x,
                                      int16_t y) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_reparent_window, c, window, parent, x, y));
}

xcb_void_cookie_t xcb_reparent_window_checked(xcb_connection_t* c,
                                              xcb_window_t window,
                                              xcb_window_t parent,
                                              int16_t x,
                                              int16_t y) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_reparent_window_checked, c, window, parent, x, y));
}

xcb_void_cookie_t xcb_send_event(xcb_connection_t* c,
                                 uint8_t propagate,
                                 xcb_window_t destination,
                                 uint32_t event_mask,
                                 const char* event) {
    return cookie<xcb_void_cookie_t>(xcb(xcb_op_send_event, c, propagate,
                                         destination, event_mask, event));
}

xcb_void_cookie_t xcb_set_input_focus(xcb_connection_t* c,
                                      uint8_t revert_to,
                                      xcb_window_t focus,
                                      xcb_timestamp_t time) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_set_input_focus, c, revert_to, focus, time));
}

xcb_void_cookie_t xcb_set_selection_owner(xcb_connection_t* c,
                                          xcb_window_t owner,
                                          xcb_atom_t selection,
                                          xcb_timestamp_t time) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_set_selection_owner, c, owner, selection, time));
}

xcb_translate_coordinates_cookie_t xcb_translate_coordinates(
    xcb_connection_t* c,
    xcb_window_t src_window,
    xcb_window_t dst_window,
    int16_t src_x,
    int16_t src_y) {
    return cookie<xcb_translate_coordinates_cookie_t>(
        xcb(xcb_op_translate_coordinates, c, src_window, dst_window, src_x,
            src_y));
}

xcb_translate_coordinates_reply_t* xcb_translate_coordinates_reply(
    xcb_connection_t* c,
    xcb_translate_coordinates_cookie_t cookie,
    xcb_generic_error_t** e) {
    return take_reply<xcb_translate_coordinates_reply_t>(
        xcb(xcb_op_translate_coordinates_reply, c, cookie.sequence,
            e != nullptr),
        e);
}

xcb_void_cookie_t xcb_ungrab_key(xcb_connection_t* c,
                                 xcb_keycode_t key,
                                 xcb_window_t grab_window,
                                 uint16_t modifiers) {
    return cookie<xcb_void_cookie_t>(
        xcb(xcb_op_ungrab_key, c, key, grab_window, modifiers));
}

}  // extern "C"
