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

#include "actions.h"

#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "files.h"
#include "util.h"
#include "vst3_moduleinfo.h"

// ─── add / remove / list ───────────────────────────────────────────────────────

fs::path resolve_added_path(const fs::path& path) {
    // Relative paths would cause odd behaviour, and yabridgectl resolved
    // symlinks here as well
    std::error_code ec;
    auto canonical = fs::canonical(path, ec);
    return ec ? path : canonical;
}

void add_directory(Config& config, fs::path path) {
    config.plugin_dirs.insert(std::move(path));
    config.write();
}

void forget_directory(Config& config, const fs::path& path) {
    config.plugin_dirs.erase(path);
    config.write();
}

std::vector<NativeFile> leftover_so_files(const fs::path& path) {
    return index(path, {}).so_files;
}

void remove_directory(Config& config, const fs::path& path) {
    forget_directory(config, path);

    // Find any leftover .so files in the removed directory
    auto orphans = leftover_so_files(path);
    if (orphans.empty())
        return;

    std::cout << "Warning: Found " << orphans.size()
              << " leftover .so files still in this directory:\n";
    for (const auto& f : orphans)
        std::cout << "- " << f.path.string() << "\n";

    if (!isatty(STDIN_FILENO))
        return;

    std::cout << "\nWould you like to remove these files? Entering anything other than YES will "
                 "leave these files intact: ";
    std::string answer;
    std::getline(std::cin, answer);
    if (answer == "YES") {
        for (const auto& f : orphans)
            remove_file_or_dir(f.path);
        std::cout << "\nRemoved " << orphans.size() << " files\n";
    }
}

void list_directories(const Config& config) {
    for (const auto& p : config.plugin_dirs)
        std::cout << p.string() << "\n";
}

// ─── status ───────────────────────────────────────────────────────────────────

void show_status(const Config& config) {
    auto results = config.search_directories();

    std::cout << "vstbridge path: "
              << (config.vstbridge_home
                      ? ("'" + config.vstbridge_home->string() + "'")
                      : std::string("<auto>"))
              << "\n";

    if (config.vst2_location == Vst2InstallationLocation::Centralized)
        std::cout << "VST2 location: '" << vstbridge_vst2_home().string() << "'\n";
    else
        std::cout << "VST2 location: inline next to the Windows plugin file\n";

    std::cout << "VST3 location: '" << vstbridge_vst3_home().string() << "'\n";
    std::cout << "CLAP location: '" << vstbridge_clap_home().string() << "'\n\n";

    const VstbridgeFiles* files_ptr = nullptr;
    std::optional<VstbridgeFiles> files_storage;
    try {
        files_storage = config.files();
        files_ptr = &*files_storage;
        const auto& f = *files_storage;

        std::cout << VST2_CHAINLOADER_NAME << ": '" << f.vst2_chainloader.string() << "' ("
                  << arch_to_string(f.vst2_chainloader_arch) << ")\n";

        std::cout << VST3_CHAINLOADER_NAME << ": ";
        if (f.vst3_chainloader)
            std::cout << "'" << f.vst3_chainloader->first.string() << "' ("
                      << arch_to_string(f.vst3_chainloader->second) << ")\n";
        else
            std::cout << color::red("<not found>") << "\n";

        std::cout << CLAP_CHAINLOADER_NAME << ": ";
        if (f.clap_chainloader)
            std::cout << "'" << f.clap_chainloader->first.string() << "' ("
                      << arch_to_string(f.clap_chainloader->second) << ")\n\n";
        else
            std::cout << color::red("<not found>") << "\n\n";

        std::cout << VSTBRIDGE_HOST_EXE_NAME << ": ";
        if (f.vstbridge_host_exe && f.vstbridge_host_exe_so)
            std::cout << "'" << f.vstbridge_host_exe->string() << "'\n";
        else
            std::cout << color::red("<not found>") << "\n";

        std::cout << VSTBRIDGE_HOST_32_EXE_NAME << ": ";
        if (f.vstbridge_host_32_exe && f.vstbridge_host_32_exe_so)
            std::cout << "'" << f.vstbridge_host_32_exe->string() << "'\n";
        else
            std::cout << color::red("<not found>") << "\n";
    } catch (const std::exception& e) {
        std::cout << "Could not find vstbridge's files: " << e.what() << "\n";
    }

    for (const auto& [dir_ptr, search_results] : results) {
        std::cout << "\n" << (*dir_ptr / "").string() << "\n";

        for (const auto& [plugin_path, info] : search_results.installation_status(config, files_ptr)) {
            const auto& [plugin_ptr, status] = info;

            std::string plugin_type;
            std::visit(
                [&](const auto& p) {
                    using T = std::decay_t<decltype(p)>;
                    if constexpr (std::is_same_v<T, Vst2Plugin>)
                        plugin_type = color::cyan("VST2") + ", " + arch_to_string(p.architecture);
                    else if constexpr (std::is_same_v<T, Vst3Module>)
                        plugin_type = color::magenta("VST3") + ", " + p.type_str() + ", " +
                                      arch_to_string(p.architecture);
                    else if constexpr (std::is_same_v<T, ClapPlugin>)
                        plugin_type = color::yellow("CLAP") + ", " + arch_to_string(p.architecture);
                },
                *plugin_ptr);

            std::string status_str;
            if (!status)
                status_str = "not yet synced";
            else if (status->kind == NativeFile::Kind::Regular)
                status_str = color::green("synced");
            else if (status->kind == NativeFile::Kind::Symlink)
                status_str = color::yellow("symlink");
            else
                status_str = color::red("invalid");

            auto rel = plugin_path.lexically_relative(*dir_ptr);
            std::string display = !rel.empty() ? rel.string() : plugin_path.string();

            std::cout << "  " << display << " :: " << plugin_type << ", " << status_str << "\n";
        }
    }
}

// ─── set ──────────────────────────────────────────────────────────────────────

void set_settings(Config& config, const SetOptions& opts) {
    if (opts.path)
        config.vstbridge_home = opts.path;
    if (opts.path_auto)
        config.vstbridge_home = std::nullopt;
    if (opts.vst2_location) {
        config.vst2_location = (*opts.vst2_location == Vst2Location::Inline)
                                   ? Vst2InstallationLocation::Inline
                                   : Vst2InstallationLocation::Centralized;
    }
    if (opts.no_verify)
        config.no_verify = *opts.no_verify;
    config.write();
}

// ─── sync helpers ─────────────────────────────────────────────────────────────

// True if `path` lies strictly inside `root`, comparing the paths as written.
static bool lexically_inside(const fs::path& path, const fs::path& root) {
    auto rel = path.lexically_normal().lexically_relative(root.lexically_normal());
    return !rel.empty() && rel != "." && *rel.begin() != "..";
}

// Limits what sync may create or delete: anything inside the vstbridge plugin
// homes (~/.vst/vstbridge, ~/.vst3/vstbridge, ~/.clap/vstbridge), and .so files
// inside the configured plugin directories (inline VST2 targets and leftover
// .so files). Paths inside a home are also checked with the symlinks in their
// parent directories resolved, so neither a '..' in a computed path nor a
// symlinked directory inside a home can lead a write or delete elsewhere, such
// as onto the original plugin files.
class TargetGuard {
   public:
    TargetGuard(std::vector<fs::path> homes, const std::set<fs::path>& plugin_dirs)
        : homes_(std::move(homes)), plugin_dirs_(plugin_dirs.begin(), plugin_dirs.end()) {
        for (const auto& home : homes_) {
            std::error_code ec;
            auto resolved = fs::weakly_canonical(home, ec);
            resolved_homes_.push_back(ec ? home : resolved);
        }
    }

    bool allows(const fs::path& path) const {
        for (size_t i = 0; i < homes_.size(); ++i) {
            if (!lexically_inside(path, homes_[i]))
                continue;
            std::error_code ec;
            auto parent = fs::weakly_canonical(path.parent_path(), ec);
            if (!ec && (parent == resolved_homes_[i] || lexically_inside(parent, resolved_homes_[i])))
                return true;
        }
        if (path.extension() == ".so")
            for (const auto& dir : plugin_dirs_)
                if (lexically_inside(path, dir))
                    return true;
        return false;
    }

    void check(const fs::path& path) const {
        if (!allows(path))
            throw std::runtime_error("Refusing to modify '" + path.string() +
                                     "' because it is outside of the vstbridge plugin "
                                     "directories. This is a bug in vstbridgectl, please report "
                                     "it.");
    }

    // True if `dir` is a directory strictly inside a home or a plugin
    // directory, so it may be removed once it's empty.
    bool may_remove_empty_dir(const fs::path& dir) const {
        for (const auto& root : homes_)
            if (lexically_inside(dir, root))
                return true;
        for (const auto& root : plugin_dirs_)
            if (lexically_inside(dir, root))
                return true;
        return false;
    }

   private:
    std::vector<fs::path> homes_;
    std::vector<fs::path> resolved_homes_;
    std::vector<fs::path> plugin_dirs_;
};

// Walk everything below `root` for the orphan scan. Symlinks below `root` are
// not followed, so a symlink inside a plugin home can't lead the scan
// somewhere else or into a loop, and unreadable directories are skipped
// instead of stopping the sync. `visit` returns false to not descend into a
// directory.
static void walk_plugin_home(const fs::path& root,
                             const std::function<bool(const fs::directory_entry&)>& visit) {
    std::error_code ec;
    if (!fs::exists(root, ec))
        return;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!visit(*it))
            it.disable_recursion_pending();
    }
    if (ec)
        std::cerr << wrap_text(color::red("WARNING") + ": Could not scan all of '" +
                               root.string() + "' for leftover files: " + ec.message())
                  << "\n\n";
}

enum class InstallMethod { Copy, Symlink };

// Install (copy or symlink) from→to. Returns true if the file was newly
// created/updated, false if it was already up-to-date.
static bool install_file(const TargetGuard& guard, bool force, InstallMethod method,
                         const fs::path& from, std::optional<int64_t> from_hash,
                         const fs::path& to) {
    guard.check(to);
    std::error_code ec;
    auto to_st = fs::symlink_status(to, ec);
    bool target_exists = !ec;

    if (target_exists) {
        if (!force) {
            if (method == InstallMethod::Copy) {
                if (from_hash && fs::is_regular_file(to_st)) {
                    try {
                        if (hash_file(to) == *from_hash)
                            return false;
                    } catch (...) {
                    }
                }
            } else {  // Symlink
                if (fs::is_symlink(to_st)) {
                    std::error_code rec;
                    auto target = fs::read_symlink(to, rec);
                    if (!rec && target == from)
                        return false;
                }
            }
        }
        remove_file_or_dir(to);
    }

    if (method == InstallMethod::Copy)
        copy_or_reflink(from, to);
    else
        make_symlink(from, to);

    return true;
}

// ─── do_sync ──────────────────────────────────────────────────────────────────

void do_sync(Config& config, const SyncOptions& opts) {
    auto files = config.files();
    int64_t vst2_hash = hash_file(files.vst2_chainloader);

    std::optional<int64_t> vst3_hash;
    if (files.vst3_chainloader)
        vst3_hash = hash_file(files.vst3_chainloader->first);

    std::optional<int64_t> clap_hash;
    if (files.clap_chainloader)
        clap_hash = hash_file(files.clap_chainloader->first);

    // Print which chainloaders we're using
    if (files.vst3_chainloader && files.clap_chainloader) {
        std::cout << "Setting up VST2, VST3, and CLAP plugins using:\n";
        std::cout << "- " << files.vst2_chainloader.string() << "\n";
        std::cout << "- " << files.vst3_chainloader->first.string() << "\n";
        std::cout << "- " << files.clap_chainloader->first.string() << "\n\n";
    } else if (files.vst3_chainloader) {
        std::cout << "Setting up VST2 and VST3 plugins using:\n";
        std::cout << "- " << files.vst2_chainloader.string() << "\n";
        std::cout << "- " << files.vst3_chainloader->first.string() << "\n\n";
    } else if (files.clap_chainloader) {
        std::cout << "Setting up VST2 and CLAP plugins using:\n";
        std::cout << "- " << files.vst2_chainloader.string() << "\n";
        std::cout << "- " << files.clap_chainloader->first.string() << "\n\n";
    } else {
        std::cout << "Setting up VST2 plugins using:\n";
        std::cout << "- " << files.vst2_chainloader.string() << "\n\n";
    }

    auto results = config.search_directories();

    // Safety check: plugin homes must not be symlinks pointing into plugin dirs
    auto vst2_home = vstbridge_vst2_home();
    auto vst3_home = vstbridge_vst3_home();
    auto clap_home = vstbridge_clap_home();

    auto bail_if_unsafe = [&](const fs::path& plugin_home, const char* display) {
        std::error_code ec;
        auto canon = fs::canonical(plugin_home, ec);
        if (ec || canon == plugin_home)
            return;
        for (const auto& dir : config.plugin_dirs) {
            std::error_code dec;
            auto dir_canon = fs::canonical(dir, dec);
            if (!dec && dir_canon.string().find(canon.string()) == 0) {
                throw std::runtime_error(std::string("'") + display + "' is a symlink to '" +
                                         canon.string() + "'. This conflicts with '" +
                                         dir.string() +
                                         "' from your plugin directories, so the syncing process "
                                         "will now be aborted.");
            }
        }
    };
    bail_if_unsafe(vst2_home, "~/.vst/vstbridge");
    bail_if_unsafe(vst3_home, "~/.vst3/vstbridge");
    bail_if_unsafe(clap_home, "~/.clap/vstbridge");

    const TargetGuard guard({vst2_home, vst3_home, clap_home}, config.plugin_dirs);
    auto create_parent_dirs = [&](const fs::path& file) {
        guard.check(file);
        create_dir_all(file.parent_path());
    };

    std::unordered_set<std::string> managed_plugins;
    std::unordered_set<std::string> new_plugins;
    size_t skipped_count = 0;

    // Track files placed in centralized dirs for orphan detection
    std::unordered_set<std::string> known_vst2_files;
    std::unordered_map<std::string, std::unordered_set<std::string>> known_vst3_bundle_files;
    std::unordered_set<std::string> known_clap_files;

    std::vector<NativeFile> orphan_files;

    for (const auto& [dir_ptr, search_results] : results) {
        // Inline VST2 orphans (so files in plugin dirs without a matching .dll)
        for (const auto* f : search_results.vst2_inline_orphans(config))
            orphan_files.push_back(*f);

        skipped_count += search_results.skipped_files.size();

        if (opts.verbose)
            std::cout << (*dir_ptr / "").string() << "\n";

        for (const auto& plugin : search_results.plugins) {
            fs::path plugin_path;

            std::visit(
                [&](const auto& p) {
                    using T = std::decay_t<decltype(p)>;

                    if constexpr (std::is_same_v<T, Vst2Plugin>) {
                        if (config.vst2_location == Vst2InstallationLocation::Centralized) {
                            auto native = p.centralized_native_target();
                            auto windows = p.centralized_windows_target();
                            auto norm = normalize_path(native);

                            bool is_new = known_vst2_files.insert(native.string()).second;
                            is_new &= known_vst2_files.insert(windows.string()).second;
                            if (!is_new) {
                                std::cerr << "\n"
                                          << wrap_text(color::red("WARNING") + ": '" +
                                                       windows.string() +
                                                       "' has already been provided by another "
                                                       "Wine prefix or plugin directory, skipping "
                                                       "it\n")
                                          << "\n";
                                return;
                            }

                            create_parent_dirs(native);
                            if (install_file(guard, opts.force, InstallMethod::Copy,
                                             files.vst2_chainloader, vst2_hash, native))
                                new_plugins.insert(norm.string());
                            managed_plugins.insert(norm.string());

                            install_file(guard, true, InstallMethod::Symlink, p.path, std::nullopt,
                                         windows);
                        } else {
                            auto target = p.inline_native_target();
                            auto norm = normalize_path(target);
                            if (install_file(guard, opts.force, InstallMethod::Copy,
                                             files.vst2_chainloader, vst2_hash, target))
                                new_plugins.insert(norm.string());
                            managed_plugins.insert(norm.string());
                        }
                        plugin_path = p.path;

                    } else if constexpr (std::is_same_v<T, Vst3Module>) {
                        if (!vst3_hash)
                            return;

                        auto bundle_home = p.target_bundle_home();
                        auto native = p.target_native_module_path(&files);
                        auto windows = p.target_windows_module_path();
                        auto norm = normalize_path(native);

                        auto& bundle_files = known_vst3_bundle_files[bundle_home.string()];
                        if (!bundle_files.insert(windows.string()).second) {
                            std::cerr << "\n"
                                      << wrap_text(color::red("WARNING") + ": The " +
                                                   arch_to_string(p.architecture) +
                                                   " version of '" + bundle_home.string() +
                                                   "' has already been provided by another Wine "
                                                   "prefix or plugin directory, skipping '" +
                                                   p.original_module_path().string() + "'\n")
                                      << "\n";
                            return;
                        }

                        create_parent_dirs(native);
                        if (install_file(guard, opts.force, InstallMethod::Copy,
                                         files.vst3_chainloader->first, vst3_hash, native))
                            new_plugins.insert(norm.string());
                        managed_plugins.insert(norm.string());
                        bundle_files.insert(native.string());

                        create_parent_dirs(windows);
                        install_file(guard, true, InstallMethod::Symlink, p.original_module_path(),
                                     std::nullopt, windows);
                        bundle_files.insert(windows.string());

                        if (auto res_dir = p.original_resources_dir()) {
                            auto target_res = p.target_resources_dir();
                            install_file(guard, false, InstallMethod::Symlink, *res_dir, std::nullopt,
                                         target_res);
                            bundle_files.insert(target_res.string());
                        }

                        if (auto moduleinfo_src = p.original_moduleinfo_path()) {
                            auto moduleinfo_dst = p.target_moduleinfo_path();
                            try {
                                auto json_text = read_to_string(*moduleinfo_src);
                                auto converted = rewrite_moduleinfo_uid_byte_orders(json_text);
                                guard.check(moduleinfo_dst);
                                write_file(moduleinfo_dst, converted);
                                bundle_files.insert(moduleinfo_dst.string());
                            } catch (const std::exception& e) {
                                std::cerr << "Error converting '" << moduleinfo_src->string()
                                          << "', skipping...\n"
                                          << e.what() << "\n";
                            }
                        }

                        plugin_path = p.original_path();

                    } else if constexpr (std::is_same_v<T, ClapPlugin>) {
                        if (!clap_hash)
                            return;

                        auto native = p.native_target();
                        auto windows = p.windows_target();
                        auto norm = normalize_path(native);

                        bool is_new = known_clap_files.insert(native.string()).second;
                        is_new &= known_clap_files.insert(windows.string()).second;
                        if (!is_new) {
                            std::cerr << "\n"
                                      << wrap_text(color::red("WARNING") + ": '" +
                                                   windows.string() +
                                                   "' has already been provided by another Wine "
                                                   "prefix or plugin directory, skipping it\n")
                                      << "\n";
                            return;
                        }

                        create_parent_dirs(native);
                        if (install_file(guard, opts.force, InstallMethod::Copy,
                                         files.clap_chainloader->first, clap_hash, native))
                            new_plugins.insert(norm.string());
                        managed_plugins.insert(norm.string());

                        install_file(guard, true, InstallMethod::Symlink, p.path, std::nullopt, windows);
                        plugin_path = p.path;
                    }
                },
                plugin);

            if (opts.verbose && !plugin_path.empty()) {
                auto rel = plugin_path.lexically_relative(*dir_ptr);
                std::cout << "  " << (!rel.empty() ? rel.string() : plugin_path.string())
                          << "\n";
            }
        }

        if (opts.verbose)
            std::cout << "\n";
    }

    if (opts.verbose && skipped_count > 0) {
        std::cout << "Skipped " << skipped_count << " non-plugin .dll files\n\n";
    }

    // ── Orphan detection in centralized directories ─────────────────────────

    // VST2 and CLAP: any file in the home dir not in known set is an orphan
    auto collect_orphans_flat = [&](const fs::path& home,
                                    const std::unordered_set<std::string>& known,
                                    const std::vector<std::string>& valid_exts) {
        walk_plugin_home(home, [&](const fs::directory_entry& entry) {
            const auto& p = entry.path();
            std::error_code ec;
            if (fs::is_directory(entry.symlink_status(ec)))
                return true;
            const auto ext = p.extension().string();
            if (std::find(valid_exts.begin(), valid_exts.end(), ext) == valid_exts.end())
                return true;
            if (!known.count(p.string()))
                if (auto ft = get_file_type(p))
                    orphan_files.push_back(*ft);
            return true;
        });
    };

    collect_orphans_flat(vst2_home, known_vst2_files, {".so", ".dll"});
    collect_orphans_flat(clap_home, known_clap_files, {".clap", ".clap-win", ".so"});

    // VST3: every .vst3 bundle in vst3_home, including the ones in
    // subdirectories, is either unknown and removed as a whole, or checked for
    // files that no longer belong to it
    walk_plugin_home(vst3_home, [&](const fs::directory_entry& entry) {
        std::error_code ec;
        if (!fs::is_directory(entry.symlink_status(ec)))
            return true;
        const auto& bundle = entry.path();
        if (bundle.extension() != ".vst3")
            return true;

        auto it = known_vst3_bundle_files.find(bundle.string());
        if (it == known_vst3_bundle_files.end()) {
            orphan_files.push_back(NativeFile::make_directory(bundle));
        } else {
            const auto& managed = it->second;
            walk_plugin_home(bundle, [&](const fs::directory_entry& bentry) {
                std::error_code bec;
                if (fs::is_directory(bentry.symlink_status(bec)))
                    return true;
                if (!managed.count(bentry.path().string()))
                    if (auto ft = get_file_type(bentry.path()))
                        orphan_files.push_back(*ft);
                return true;
            });
        }
        // A bundle doesn't contain other bundles
        return false;
    });

    // ── Report / prune orphans ─────────────────────────────────────────────

    if (!orphan_files.empty()) {
        auto count = orphan_files.size();
        std::string files_str = (count == 1) ? "1 leftover file" : std::to_string(count) + " leftover files";

        if (opts.prune)
            std::cout << "Removing " << files_str << ":\n";
        else
            std::cout << "Found " << files_str
                      << ", rerun with the '--prune' option to remove them:\n";

        // Sort in reverse lexicographical order so children come before parents
        std::sort(orphan_files.begin(), orphan_files.end(), [](const NativeFile& a, const NativeFile& b) {
            return b.path < a.path;
        });

        for (const auto& f : orphan_files) {
            std::cout << "- " << f.path.string() << "\n";
            if (opts.prune) {
                if (!guard.allows(f.path)) {
                    std::cerr << wrap_text(color::red("WARNING") + ": Not removing '" +
                                           f.path.string() +
                                           "' because it is outside of the vstbridge plugin "
                                           "directories\n");
                    continue;
                }
                if (f.kind == NativeFile::Kind::Directory)
                    remove_dir_all(f.path);
                else
                    remove_file_or_dir(f.path);

                // Prune empty parent directories
                auto parent = f.path.parent_path();
                while (guard.may_remove_empty_dir(parent) &&
                       fs::is_directory(fs::symlink_status(parent))) {
                    std::error_code pec;
                    if (!fs::remove(parent, pec) || pec)
                        break;
                    parent = parent.parent_path();
                }
            }
        }
        std::cout << "\n";
    }

    std::cout << "Finished setting up " << managed_plugins.size() << " plugins ("
              << new_plugins.size() << " new), skipped " << skipped_count
              << " non-plugin .dll files\n";

    if (opts.no_verify || config.no_verify)
        return;

    verify_path_setup(config);
    verify_wine_setup(config);
    verify_external_dependencies();
}
