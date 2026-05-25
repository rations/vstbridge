// yabridge: a Wine plugin bridge
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

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "types.h"

namespace fs = std::filesystem;

// Forward-declare SearchResults to avoid a circular include with files.h
struct SearchResults;

static constexpr const char* CLAP_CHAINLOADER_NAME = "libyabridge-chainloader-clap.so";
static constexpr const char* VST2_CHAINLOADER_NAME = "libyabridge-chainloader-vst2.so";
static constexpr const char* VST3_CHAINLOADER_NAME = "libyabridge-chainloader-vst3.so";
static constexpr const char* YABRIDGE_HOST_EXE_NAME = "yabridge-host.exe";
static constexpr const char* YABRIDGE_HOST_32_EXE_NAME = "yabridge-host-32.exe";

enum class Vst2InstallationLocation { Centralized, Inline };

struct KnownConfig {
    std::string wine_version;
    int64_t yabridge_host_hash{};

    bool operator==(const KnownConfig& o) const {
        return wine_version == o.wine_version && yabridge_host_hash == o.yabridge_host_hash;
    }
};

struct YabridgeFiles {
    fs::path vst2_chainloader;
    LibArchitecture vst2_chainloader_arch;

    // nullopt if yabridge was compiled without VST3 support
    std::optional<std::pair<fs::path, LibArchitecture>> vst3_chainloader;
    // nullopt if yabridge was compiled without CLAP support
    std::optional<std::pair<fs::path, LibArchitecture>> clap_chainloader;

    std::optional<fs::path> yabridge_host_exe;
    std::optional<fs::path> yabridge_host_exe_so;
    std::optional<fs::path> yabridge_host_32_exe;
    std::optional<fs::path> yabridge_host_32_exe_so;
};

struct Config {
    // Path to the directory containing libyabridge-chainloader-*.so
    std::optional<fs::path> yabridge_home;
    // Directories to search for Windows plugins (kept sorted, no duplicates)
    std::set<fs::path> plugin_dirs;
    Vst2InstallationLocation vst2_location = Vst2InstallationLocation::Centralized;
    bool no_verify = false;
    std::set<fs::path> blacklist;
    std::optional<KnownConfig> last_known_config;

    static Config read();
    void write() const;

    YabridgeFiles files() const;

    // Returns a map from plugin directory path → SearchResults for each dir
    std::map<const fs::path*, SearchResults> search_directories() const;
};

// XDG helpers
fs::path yabridge_data_home();  // ~/.local/share/yabridge (or $XDG_DATA_HOME/yabridge)
fs::path yabridgectl_config_dir();  // $XDG_CONFIG_HOME/yabridgectl

fs::path yabridge_vst2_home();
fs::path yabridge_vst3_home();
fs::path yabridge_clap_home();

// Search PATH for an executable; returns its full path or nullopt
std::optional<fs::path> which(const std::string& name);
