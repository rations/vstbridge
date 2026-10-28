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

#pragma once

// The xcb functions the editor and the XDND proxy use. In the PE host these are
// thunks (`xcb-thunks.cpp`) that forward to libxcb in the unixlib through the
// `unixlib::xcb_call` call. xcb's wire structs only use fixed width types, so
// they have the same layout on both sides.
//
// Replies, events and errors are allocated by libxcb with the Linux `malloc()`,
// but the host code frees them with its own `free()`. The thunks copy them
// into memory allocated on the PE side through `xcb_op_copy` (the 32-bit host
// can't read libxcb's memory itself), and then free the original through
// `xcb_op_free`. The connection's setup data is copied the same way, once.

#include <stdint.h>

#include "calls.h"

namespace unixlib {

enum XcbOp : uint32_t {
    xcb_op_connect,
    xcb_op_disconnect,
    xcb_op_flush,
    xcb_op_generate_id,
    xcb_op_get_setup,
    xcb_op_setup_roots_iterator,
    xcb_op_screen_next,
    xcb_op_poll_for_event,
    xcb_op_request_check,

    xcb_op_change_property,
    xcb_op_change_window_attributes,
    xcb_op_configure_window,
    xcb_op_create_window,
    xcb_op_delete_property,
    xcb_op_destroy_window,
    xcb_op_get_input_focus,
    xcb_op_get_input_focus_reply,
    xcb_op_get_keyboard_mapping,
    xcb_op_get_keyboard_mapping_reply,
    xcb_op_get_keyboard_mapping_keysyms,
    xcb_op_get_keyboard_mapping_keysyms_length,
    xcb_op_get_property,
    xcb_op_get_property_reply,
    xcb_op_get_property_value,
    xcb_op_grab_key,
    xcb_op_intern_atom,
    xcb_op_intern_atom_reply,
    xcb_op_map_window,
    xcb_op_query_pointer,
    xcb_op_query_pointer_reply,
    xcb_op_query_tree,
    xcb_op_query_tree_reply,
    xcb_op_reparent_window,
    xcb_op_reparent_window_checked,
    xcb_op_send_event,
    xcb_op_set_input_focus,
    xcb_op_set_selection_owner,
    xcb_op_translate_coordinates,
    xcb_op_translate_coordinates_reply,
    xcb_op_ungrab_key,

    // Frees `args[0]` and `args[1]` (either may be null) with the Linux
    // `free()`
    xcb_op_free,
    // Copies `args[2]` bytes from the Linux side's `args[0]` to the PE side's
    // `args[1]`
    xcb_op_copy,
    xcb_op_connection_has_error,
};

struct XcbArgs {
    uint32_t op;
    uint32_t reserved;
    // The function's arguments in order. Pointers are passed as is, since both
    // sides share the address space. `xcb_connection_t*` is the exception, see
    // `xcb-thunks.cpp`.
    aligned_uint64_t args[13];
    // The return value. Cookies are returned as their sequence number and
    // pointers as the address.
    aligned_uint64_t result;

    // For functions returning a reply or an event, the libxcb allocation and
    // its size in bytes. Set to 0 if there was no reply. Also used for
    // `xcb_op_get_setup`'s setup data, which isn't freed.
    aligned_uint64_t packet;
    aligned_uint64_t packet_size;
    // For functions that return an error through an `xcb_generic_error_t**` or
    // as their return value. The error is always `error_size` bytes.
    aligned_uint64_t error;
    aligned_uint64_t error_size;
};

// See `calls.h`
static_assert(sizeof(XcbArgs) == 152);

}  // namespace unixlib
