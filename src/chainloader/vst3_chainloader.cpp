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
 * @file vst3_chainloader.cpp
 * @brief VST3 chainloader - small stub that loads the main library
 * 
 * This is a minimal VST3 plugin that loads the actual vst3bridge-native
 * library and forwards all calls to it. This approach:
 * - Allows updating vst3bridge without reinstalling all plugins
 * - Minimizes disk space usage (only this small stub is copied per plugin)
 * - Makes plugin management easier
 */

#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <string>

// VST3 interface definition (minimal)
namespace Steinberg {
    typedef uint32_t uint32;
    typedef int32_t int32;
    typedef uint64_t uint64;
    
    struct FUID {
        uint32_t data1, data2, data3, data4;
        bool operator==(const FUID& o) const {
            return data1 == o.data1 && data2 == o.data2 && 
                   data3 == o.data3 && data4 == o.data4;
        }
    };
    
    class FUnknown {
    public:
        virtual uint32 PLUGIN_API queryInterface(const FUID& _iid, void** obj) = 0;
        virtual uint32 PLUGIN_API addRef() = 0;
        virtual uint32 PLUGIN_API release() = 0;
        static const FUID iid;
    };
    const FUID FUnknown_iid = {0x00000000, 0x00000000, 0xC0000000, 0x00000046};
    
    class IPluginFactory : public FUnknown {
    public:
        static const FUID iid;
    };
    const FUID IPluginFactory_iid = {0x7A4D8F64, 0x6D7E4E08, 0xB4E2F84B, 0xD9C8F9F4};
}

// Macros for export
#ifdef _WIN32
    #define SMTG_EXPORT_SYMBOL __declspec(dllexport)
#else
    #define SMTG_EXPORT_SYMBOL __attribute__ ((visibility ("default")))
#endif

namespace vst3bridge {

// Global state
static void* g_library_handle = nullptr;
static Steinberg::IPluginFactory* g_factory = nullptr;

// Plugin path (set by management tool)
static char g_plugin_path[4096] = {0};

/**
 * @brief Get the path to this shared library
 * @return Path to this .so file
 */
static std::string getLibraryPath() {
    // Read from /proc/self/maps
    // Format: address perms offset dev inode pathname
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return "";
    
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        // Look for line containing this library
        if (strstr(line, "vst3bridge.vst3") || strstr(line, "vst3bridge.so")) {
            // Extract path (after the 5th field)
            char* path = strchr(line, '/');
            if (path) {
                // Remove newline
                char* nl = strchr(path, '\n');
                if (nl) *nl = '\0';
                fclose(f);
                return path;
            }
        }
    }
    
    fclose(f);
    return "";
}

/**
 * @brief Get the path to the vst3bridge-native library
 * @return Path to main library
 */
static std::string getMainLibraryPath() {
    // Try standard locations
    const char* home = getenv("HOME");
    if (home) {
        std::string path = std::string(home) + "/.local/share/vst3bridge/libvst3bridge-native.so";
        if (FILE* f = fopen(path.c_str(), "r")) {
            fclose(f);
            return path;
        }
    }
    
    // Try relative to this library
    std::string lib_path = getLibraryPath();
    if (!lib_path.empty()) {
        // Remove filename, add native library name
        size_t last_slash = lib_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string dir = lib_path.substr(0, last_slash);
            std::string path = dir + "/libvst3bridge-native.so";
            if (FILE* f = fopen(path.c_str(), "r")) {
                fclose(f);
                return path;
            }
        }
    }
    
    // Try system paths
    const char* paths[] = {
        "/usr/lib/vst3bridge/libvst3bridge-native.so",
        "/usr/local/lib/vst3bridge/libvst3bridge-native.so",
        "/usr/lib/x86_64-linux-gnu/vst3bridge/libvst3bridge-native.so",
        "/usr/lib64/vst3bridge/libvst3bridge-native.so",
    };
    
    for (const auto& path : paths) {
        if (FILE* f = fopen(path, "r")) {
            fclose(f);
            return path;
        }
    }
    
    return "";
}

/**
 * @brief Get the path to the Windows plugin
 * @return Path to .vst3 file in Wine prefix
 */
static std::string getWindowsPluginPath() {
    // First check environment variable set by management tool
    const char* env_path = getenv("VST3BRIDGE_PLUGIN_PATH");
    if (env_path) {
        return env_path;
    }
    
    // Check embedded path
    if (g_plugin_path[0] != '\0') {
        return g_plugin_path;
    }
    
    // Try to derive from library path
    std::string lib_path = getLibraryPath();
    if (lib_path.empty()) {
        return "";
    }
    
    // Replace .so extension with .vst3 path in Wine
    // This requires the management tool to set up a consistent mapping
    // For now, return empty - the management tool must set the env var
    return "";
}

/**
 * @brief Load the main vst3bridge library
 * @return true on success
 */
static bool loadMainLibrary() {
    if (g_library_handle) {
        return true;
    }
    
    std::string lib_path = getMainLibraryPath();
    if (lib_path.empty()) {
        return false;
    }
    
    g_library_handle = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!g_library_handle) {
        return false;
    }
    
    return true;
}

/**
 * @brief Get the plugin factory from the main library
 * @return Factory pointer or nullptr
 */
static Steinberg::IPluginFactory* getFactory() {
    if (g_factory) {
        return g_factory;
    }
    
    if (!loadMainLibrary()) {
        return nullptr;
    }
    
    // Get GetPluginFactory function
    typedef Steinberg::IPluginFactory* (*GetPluginFactoryFunc)();
    auto* get_factory = reinterpret_cast<GetPluginFactoryFunc>(
        dlsym(g_library_handle, "GetPluginFactory"));
    
    if (!get_factory) {
        return nullptr;
    }
    
    // Set plugin path environment variable
    std::string plugin_path = getWindowsPluginPath();
    if (!plugin_path.empty()) {
        setenv("VST3BRIDGE_PLUGIN_PATH", plugin_path.c_str(), 1);
    }
    
    g_factory = get_factory();
    return g_factory;
}

} // namespace vst3bridge

// =============================================================================
// VST3 Entry Points
// =============================================================================

extern "C" {

/**
 * @brief VST3 GetPluginFactory entry point
 * @return Plugin factory from main library
 */
SMTG_EXPORT_SYMBOL Steinberg::IPluginFactory* GetPluginFactory() {
    return vst3bridge::getFactory();
}

/**
 * @brief Module initialization
 * @return true on success
 */
SMTG_EXPORT_SYMBOL bool InitDll() {
    return vst3bridge::loadMainLibrary();
}

/**
 * @brief Module cleanup
 * @return true on success
 */
SMTG_EXPORT_SYMBOL bool ExitDll() {
    if (vst3bridge::g_library_handle) {
        dlclose(vst3bridge::g_library_handle);
        vst3bridge::g_library_handle = nullptr;
    }
    vst3bridge::g_factory = nullptr;
    return true;
}

/**
 * @brief Embedded plugin path setter (called by management tool)
 * @param path Windows plugin path
 */
SMTG_EXPORT_SYMBOL void VST3Bridge_SetPluginPath(const char* path) {
    if (path) {
        strncpy(vst3bridge::g_plugin_path, path, sizeof(vst3bridge::g_plugin_path) - 1);
        vst3bridge::g_plugin_path[sizeof(vst3bridge::g_plugin_path) - 1] = '\0';
    }
}

} // extern "C"
