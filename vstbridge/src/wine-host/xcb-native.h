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

// Use the native version of xcb. In the PE plugin host the xcb functions are
// thunks into the unixlib, see `unixlib/xcb-thunks.cpp`.

#ifdef VSTBRIDGE_PE_HOST
// `<xcb/xcb.h>` includes this, and mingw-w64's version refuses to compile
// without `_WIN32`. Its `<pthread.h>` and `<sys/uio.h>` includes are handled
// by `unixlib/include/`.
#include <sys/types.h>
#endif

#pragma push_macro("_WIN32")
#undef _WIN32
#include <xcb/xcb.h>
#pragma pop_macro("_WIN32")
