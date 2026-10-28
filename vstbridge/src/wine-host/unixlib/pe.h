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

// The PE host's side of the unixlib. See `calls.h`.

#include <optional>
#include <string>
#include <vector>

#include "calls.h"

namespace unixlib {

/**
 * Load `<host exe name>-unixlib.so` from the directory containing the host
 * executable. Must be called before any other function here.
 *
 * @return An error message if the unixlib could not be loaded.
 */
std::optional<std::string> init();

/**
 * Run one of the unixlib's functions on the calling thread.
 */
void call(Call code, void* args);

/**
 * Whether shared memory has to be mapped over memory allocated here, with
 * `allocate_shm_region()`, instead of wherever the Linux side's `mmap()` puts
 * it. That's the case for the 32-bit host on a WoW64 Wine, which can only
 * address the low 4 GiB.
 */
constexpr bool shm_needs_pe_allocation = sizeof(void*) < sizeof(uint64_t);

/**
 * Allocate `size` bytes of committed memory for `ShmMapArgs::fixed_address`.
 * Returns null on failure.
 */
void* allocate_shm_region(size_t size);

/**
 * Release memory from `allocate_shm_region()`, along with the shared memory
 * mapped over it. Does nothing for null.
 */
void free_shm_region(void* address);

/**
 * Wine's `wine_get_dos_file_name()` and `wine_get_unix_file_name()` kernel32
 * exports. These are not in mingw-w64's import libraries, so they're looked up
 * at runtime. The returned strings must be freed with
 * `HeapFree(GetProcessHeap(), 0, ...)`.
 */
wchar_t* wine_get_dos_file_name(const char* unix_path);
char* wine_get_unix_file_name(const wchar_t* dos_path);

/**
 * The command line arguments as UTF-8. The C runtime's `argv` uses the ANSI
 * code page, which would mangle the non-ASCII Linux socket paths passed by the
 * native plugin.
 */
std::vector<std::string> command_line_arguments();

/**
 * Convert a UTF-8 Linux path from the native plugin to a DOS path in the ANSI
 * code page, which is what the host's `LoadLibraryA()` style calls expect.
 * Returns the original path if Wine can't convert it.
 */
std::string unix_path_to_dos_path(const std::string& unix_path);

/**
 * Convert a string from the ANSI code page to UTF-8.
 */
std::string ansi_to_utf8(const std::string& string);

}  // namespace unixlib
