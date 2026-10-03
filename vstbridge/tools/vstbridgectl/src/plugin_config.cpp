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

#include "plugin_config.h"

#include <fnmatch.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>

#include <toml++/toml.hpp>

#include "util.h"

namespace {

// The `.so` (or `.clap`) file the host loads. vstbridge searches for
// `vstbridge.toml` from here and matches patterns against this path.
fs::path native_library_path(const Plugin& plugin,
                             const Config& config,
                             const VstbridgeFiles* files) {
    return std::visit(
        [&](const auto& p) -> fs::path {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, Vst2Plugin>) {
                return config.vst2_location == Vst2InstallationLocation::Centralized
                           ? p.centralized_native_target()
                           : p.inline_native_target();
            } else if constexpr (std::is_same_v<T, Vst3Module>) {
                return p.target_native_module_path(files);
            } else {
                return p.native_target();
            }
        },
        plugin);
}

// The path the plugin's section key is built from. For VST3 this is the whole
// bundle, which matches the library inside it thanks to `FNM_LEADING_DIR`.
fs::path section_path(const Plugin& plugin, const Config& config) {
    if (const auto* vst3 = std::get_if<Vst3Module>(&plugin))
        return vst3->target_bundle_home();
    return native_library_path(plugin, config, nullptr);
}

// Where to create `vstbridge.toml` when no existing file applies
fs::path default_config_dir(const Plugin& plugin, const Config& config) {
    return std::visit(
        [&](const auto& p) -> fs::path {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, Vst2Plugin>) {
                return config.vst2_location == Vst2InstallationLocation::Centralized
                           ? vstbridge_vst2_home()
                           : p.inline_native_target().parent_path();
            } else if constexpr (std::is_same_v<T, Vst3Module>) {
                return vstbridge_vst3_home();
            } else {
                return vstbridge_clap_home();
            }
        },
        plugin);
}

// Same as vstbridge's `find_dominating_file()`
std::optional<fs::path> find_dominating_config(fs::path dir) {
    while (dir != "/" && !dir.empty()) {
        const fs::path candidate = dir / "vstbridge.toml";
        if (fs::exists(candidate))
            return candidate;
        dir = dir.parent_path();
    }
    return std::nullopt;
}

std::string escape_glob(const std::string& path) {
    std::string escaped;
    for (const char c : path) {
        if (c == '\\' || c == '*' || c == '?' || c == '[')
            escaped += '\\';
        escaped += c;
    }
    return escaped;
}

bool pattern_matches(const std::string& pattern, const std::string& relative_path) {
    return fnmatch(pattern.c_str(), relative_path.c_str(),
                   FNM_PATHNAME | FNM_LEADING_DIR) == 0;
}

std::string toml_quote(const std::string& s) {
    std::string quoted = "\"";
    for (const char c : s) {
        switch (c) {
            case '"':  quoted += "\\\""; break;
            case '\\': quoted += "\\\\"; break;
            case '\n': quoted += "\\n"; break;
            case '\t': quoted += "\\t"; break;
            case '\r': quoted += "\\r"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    quoted += buf;
                } else {
                    quoted += c;
                }
        }
    }
    return quoted + "\"";
}

struct Section {
    std::string key;
    size_t header_line;  // 0-based
    const toml::table* table;
};

struct ParsedFile {
    toml::table root;
    std::vector<std::string> lines;
    // In file order, which is also the order of precedence
    std::vector<Section> sections;

    explicit ParsedFile(const fs::path& path) {
        const std::string text = read_to_string(path);
        root = toml::parse(text, path.string());

        std::istringstream stream(text);
        for (std::string line; std::getline(stream, line);)
            lines.push_back(line);

        for (const auto& [key, node] : root) {
            if (const toml::table* table = node.as_table()) {
                sections.push_back(Section{
                    std::string(key.str()),
                    static_cast<size_t>(key.source().begin.line) - 1, table});
            }
        }
        std::sort(sections.begin(), sections.end(),
                  [](const Section& a, const Section& b) {
                      return a.header_line < b.header_line;
                  });
    }

    std::optional<size_t> find(const std::string& key) const {
        for (size_t i = 0; i < sections.size(); ++i) {
            if (sections[i].key == key)
                return i;
        }
        return std::nullopt;
    }

    static bool is_blank_or_comment(const std::string& line) {
        const size_t start = line.find_first_not_of(" \t");
        return start == std::string::npos || line[start] == '#';
    }

    // The lines `[begin, end)` belonging to a section. Comments and blank
    // lines just before the next section are left out, since those usually
    // describe that next section.
    std::pair<size_t, size_t> section_lines(size_t index) const {
        const size_t begin = sections[index].header_line;
        size_t end = index + 1 < sections.size() ? sections[index + 1].header_line
                                                 : lines.size();
        while (end > begin + 1 && is_blank_or_comment(lines[end - 1]))
            --end;
        return {begin, end};
    }

    // The first line of the comment block directly above a section's header,
    // or the header itself if there is none
    size_t comment_begin(size_t index) const {
        size_t line = sections[index].header_line;
        while (line > 0) {
            const std::string& previous = lines[line - 1];
            const size_t start = previous.find_first_not_of(" \t");
            if (start == std::string::npos || previous[start] != '#')
                break;
            --line;
        }
        return line;
    }

    // Where to insert a section so it comes before every other section,
    // keeping comments directly above the first section attached to it
    size_t top_insertion_line() const {
        return sections.empty() ? lines.size() : comment_begin(0);
    }

    std::string text() const {
        size_t count = lines.size();
        while (count > 0 && lines[count - 1].empty())
            --count;

        std::string out;
        for (size_t i = 0; i < count; ++i)
            out += lines[i] + "\n";
        return out;
    }
};

std::vector<std::string> render_section(const std::string& key,
                                        const PluginOptions& options) {
    std::vector<std::string> out;
    out.push_back("[" + toml_quote(key) + "]");
    if (options.group)
        out.push_back("group = " + toml_quote(*options.group));
    if (options.editor_coordinate_hack)
        out.push_back("editor_coordinate_hack = true");
    if (options.editor_disable_host_scaling)
        out.push_back("editor_disable_host_scaling = true");
    if (options.editor_force_dnd)
        out.push_back("editor_force_dnd = true");
    if (!options.editor_xembed)
        out.push_back("editor_xembed = false");
    if (options.frame_rate) {
        std::ostringstream value;
        if (std::floor(*options.frame_rate) == *options.frame_rate)
            value << static_cast<long long>(*options.frame_rate);
        else
            value << *options.frame_rate;
        out.push_back("frame_rate = " + value.str());
    }
    if (options.hide_daw)
        out.push_back("hide_daw = true");
    if (options.vst3_prefer_32bit)
        out.push_back("vst3_prefer_32bit = true");
    if (!options.environment.empty()) {
        std::string line = "environment = [";
        for (size_t i = 0; i < options.environment.size(); ++i)
            line += (i == 0 ? "" : ", ") + toml_quote(options.environment[i]);
        out.push_back(line + "]");
    }
    for (const auto& [other_key, value] : options.other) {
        const bool bare = !other_key.empty() &&
                          other_key.find_first_not_of(
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                              "0123456789_-") == std::string::npos;
        out.push_back((bare ? other_key : toml_quote(other_key)) + " = " + value);
    }
    return out;
}

void write_atomically(const fs::path& path, const std::string& contents) {
    create_dir_all(path.parent_path());
    const fs::path temp = path.string() + ".tmp";
    write_file(temp, contents);
    fs::rename(temp, path);
}

}  // namespace

std::string plugin_display_name(const Plugin& plugin) {
    return std::visit(
        [](const auto& p) -> std::string {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, Vst3Module>)
                return p.original_module_name();
            else
                return p.path.filename().string();
        },
        plugin);
}

const char* plugin_format_name(const Plugin& plugin) {
    if (std::holds_alternative<Vst2Plugin>(plugin))
        return "VST2";
    if (std::holds_alternative<Vst3Module>(plugin))
        return "VST3";
    return "CLAP";
}

PluginConfigTarget locate_plugin_config(const Plugin& plugin,
                                        const Config& config,
                                        const VstbridgeFiles* files) {
    const fs::path library = native_library_path(plugin, config, files);

    PluginConfigTarget target;
    target.toml_file = find_dominating_config(library.parent_path())
                           .value_or(default_config_dir(plugin, config) /
                                     "vstbridge.toml");

    const fs::path config_dir = target.toml_file.parent_path();
    target.section_key = escape_glob(
        section_path(plugin, config).lexically_relative(config_dir).generic_string());

    if (fs::exists(target.toml_file)) {
        const ParsedFile file(target.toml_file);
        const std::string relative_library =
            library.lexically_relative(config_dir).generic_string();
        for (const auto& section : file.sections) {
            if (pattern_matches(section.key, relative_library)) {
                target.matching_section = section.key;
                break;
            }
        }
        target.has_own_section = file.find(target.section_key).has_value();
    }

    return target;
}

PluginOptions read_plugin_options(const PluginConfigTarget& target) {
    PluginOptions options;
    if (!target.matching_section)
        return options;

    const ParsedFile file(target.toml_file);
    const auto index = file.find(*target.matching_section);
    if (!index)
        return options;

    for (const auto& [key, node] : *file.sections[*index].table) {
        const std::string name(key.str());
        bool parsed = true;
        if (name == "group" && node.is_string()) {
            options.group = node.value<std::string>();
        } else if (name == "editor_coordinate_hack" && node.is_boolean()) {
            options.editor_coordinate_hack = *node.value<bool>();
        } else if (name == "editor_disable_host_scaling" && node.is_boolean()) {
            options.editor_disable_host_scaling = *node.value<bool>();
        } else if (name == "editor_force_dnd" && node.is_boolean()) {
            options.editor_force_dnd = *node.value<bool>();
        } else if (name == "editor_xembed" && node.is_boolean()) {
            options.editor_xembed = *node.value<bool>();
        } else if (name == "frame_rate" && node.is_number()) {
            options.frame_rate = node.value<double>();
        } else if (name == "hide_daw" && node.is_boolean()) {
            options.hide_daw = *node.value<bool>();
        } else if (name == "vst3_prefer_32bit" && node.is_boolean()) {
            options.vst3_prefer_32bit = *node.value<bool>();
        } else if (name == "environment" && node.is_array()) {
            std::vector<std::string> entries;
            for (const auto& element : *node.as_array()) {
                if (!element.is_string()) {
                    parsed = false;
                    break;
                }
                entries.push_back(*element.value<std::string>());
            }
            if (parsed)
                options.environment = std::move(entries);
        } else {
            parsed = false;
        }

        // Anything we can't represent is kept as is, so saving from the GUI
        // never drops part of the user's configuration
        if (!parsed) {
            std::ostringstream value;
            node.visit([&](const auto& n) { value << n; });
            options.other.emplace_back(name, value.str());
        }
    }

    return options;
}

std::optional<std::string> validate_environment_entry(const std::string& entry) {
    const size_t separator = entry.find('=');
    if (separator == std::string::npos)
        return "'" + entry + "' is missing a '=' (expected KEY=VALUE)";
    if (separator == 0)
        return "'" + entry + "' has an empty variable name";
    if (entry.find_first_of(" \t") < separator)
        return "'" + entry + "' has whitespace in the variable name";
    return std::nullopt;
}

void write_plugin_options(const PluginConfigTarget& target,
                          const PluginOptions& options) {
    for (const auto& entry : options.environment) {
        if (const auto error = validate_environment_entry(entry))
            throw std::runtime_error(*error);
    }

    const std::vector<std::string> section = render_section(target.section_key, options);

    if (!fs::exists(target.toml_file)) {
        std::string contents;
        for (const auto& line : section)
            contents += line + "\n";
        write_atomically(target.toml_file, contents);
        return;
    }

    ParsedFile file(target.toml_file);
    const auto own = file.find(target.section_key);

    // An earlier section that also matches this plugin would take precedence,
    // so in that case the plugin's section has to move to the top
    const bool shadowed = target.matching_section &&
                          *target.matching_section != target.section_key;

    if (own && !shadowed) {
        const auto [begin, end] = file.section_lines(*own);
        file.lines.erase(file.lines.begin() + begin, file.lines.begin() + end);
        file.lines.insert(file.lines.begin() + begin, section.begin(), section.end());
    } else {
        // The plugin's own section always comes after the insertion point, so
        // removing it first doesn't move that point. Its comments move along.
        const size_t insert_at = file.top_insertion_line();
        std::vector<std::string> block;
        if (own) {
            const size_t begin = file.comment_begin(*own);
            const size_t end = file.section_lines(*own).second;
            block.assign(file.lines.begin() + begin,
                         file.lines.begin() + file.sections[*own].header_line);
            file.lines.erase(file.lines.begin() + begin, file.lines.begin() + end);
            if (begin < file.lines.size() && file.lines[begin].empty() &&
                begin > 0 && file.lines[begin - 1].empty())
                file.lines.erase(file.lines.begin() + begin);
        }

        block.insert(block.end(), section.begin(), section.end());
        if (insert_at < file.lines.size())
            block.push_back("");
        else if (!file.lines.empty() && !file.lines.back().empty())
            block.insert(block.begin(), "");
        file.lines.insert(file.lines.begin() + insert_at, block.begin(), block.end());
    }

    write_atomically(target.toml_file, file.text());
}

void remove_plugin_options(const PluginConfigTarget& target) {
    if (!fs::exists(target.toml_file))
        return;

    ParsedFile file(target.toml_file);
    const auto own = file.find(target.section_key);
    if (!own)
        return;

    const auto [begin, end] = file.section_lines(*own);
    file.lines.erase(file.lines.begin() + begin, file.lines.begin() + end);
    // Don't leave a double blank line behind
    if (begin < file.lines.size() && file.lines[begin].empty() &&
        (begin == 0 || file.lines[begin - 1].empty()))
        file.lines.erase(file.lines.begin() + begin);

    write_atomically(target.toml_file, file.text());
}
