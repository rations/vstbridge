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

// Reading and writing a single plugin's section in `vstbridge.toml`. This
// follows the same lookup rules as vstbridge itself (see `Configuration` in
// `src/common/configuration.h`): the closest `vstbridge.toml` above the
// plugin's native library is used, and within that file the first section
// whose glob pattern matches the library's relative path wins.
//
// Sections are edited as text so comments and the rest of the user's file are
// left alone.

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "config.h"
#include "files.h"

namespace fs = std::filesystem;

// The options from a `vstbridge.toml` section. Defaults match vstbridge's.
struct PluginOptions {
    std::optional<std::string> group;
    bool editor_coordinate_hack = false;
    bool editor_disable_host_scaling = false;
    bool editor_force_dnd = false;
    bool editor_xembed = true;
    std::optional<double> frame_rate;
    bool hide_daw = false;
    bool vst3_prefer_32bit = false;
    std::vector<std::string> environment;

    // Keys this struct doesn't know about (like `disable_pipes`), as
    // `(key, TOML value)` pairs. These are written back unchanged.
    std::vector<std::pair<std::string, std::string>> other;
};

struct PluginConfigTarget {
    // The `vstbridge.toml` file vstbridge would read for this plugin. It may
    // not exist yet.
    fs::path toml_file;
    // The section key for this plugin, as an escaped glob pattern
    std::string section_key;
    // The section that currently applies to this plugin, if any. This is
    // `section_key` when the plugin already has its own section.
    std::optional<std::string> matching_section;
    // Whether the file already contains a section with `section_key`. That
    // section may still be shadowed by an earlier, broader one.
    bool has_own_section = false;
};

// The plugin's display name, e.g. `Foo.vst3`
std::string plugin_display_name(const Plugin& plugin);
// `VST2`, `VST3` or `CLAP`
const char* plugin_format_name(const Plugin& plugin);

PluginConfigTarget locate_plugin_config(const Plugin& plugin,
                                        const Config& config,
                                        const VstbridgeFiles* files);

// Read the options from `target.matching_section`, or the defaults if no
// section matches. Throws if the file can't be parsed.
PluginOptions read_plugin_options(const PluginConfigTarget& target);

// Returns a description of the problem, or nothing if `entry` is a valid
// `KEY=VALUE` environment entry.
std::optional<std::string> validate_environment_entry(const std::string& entry);

// Write the plugin's own section, replacing it if it already exists. The
// section is moved to the top of the file when another section earlier in the
// file would otherwise take precedence.
void write_plugin_options(const PluginConfigTarget& target,
                          const PluginOptions& options);

// Remove the plugin's own section, if it has one
void remove_plugin_options(const PluginConfigTarget& target);
