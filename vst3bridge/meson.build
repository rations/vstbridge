# vst3bridge: A modern Wine VST3 bridge for Linux
# Copyright (C) 2024
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

project(
    'vst3bridge',
    'cpp',
    version : '0.1.0',
    meson_version : '>=0.60',
    default_options : [
        'warning_level=3',
        'cpp_std=c++20',
        'build.cpp_std=c++20',
        'b_lto=true',
    ],
)

# ============================================================================
# Build Options
# ============================================================================

# Check if we're on a 64-bit system
is_64bit_system = build_machine.cpu_family() not in ['x86', 'arm']
with_bitbridge = get_option('bitbridge')

# Plugin name constants
vst3_plugin_name = 'vst3bridge'
host_name_64bit = 'vst3bridge-host'
host_name_32bit = 'vst3bridge-host-32'

# ============================================================================
# Compiler Options
# ============================================================================

base_cpp_args = [
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
    '-msse2',
]

# Native (Linux) compiler options
native_cpp_args = base_cpp_args + [
    '-DNDEBUG',
]

# Wine (Windows) compiler options
wine_cpp_args = base_cpp_args + [
    '-DNOMINMAX',
    '-Wno-attributes',
    '-Wno-ignored-attributes',
    '-D__WINE_WINSOCKAPI_STDLIB_H',
    '-D_WINSOCKAPI_',
    '-D__IFileOperation_INTERFACE_DEFINED__',
    '-D__WINE_SAL_H__',
]

wine_32bit_cpp_args = wine_cpp_args + ['-m32', '-malign-double']
wine_64bit_cpp_args = wine_cpp_args + ['-m64']

# Debug builds
if get_option('buildtype') == 'debug'
    base_cpp_args += ['-D_GLIBCXX_DEBUG']
endif

if with_bitbridge
    base_cpp_args += '-DWITH_BITBRIDGE'
endif

# ============================================================================
# Wine Checks
# ============================================================================

# Verify we're using the Wine cross-compiler
winelib_check = '''#ifndef __WINE__
#error 1
#endif'''

if not meson.get_compiler('cpp').compiles(winelib_check)
    error('You need to set up a Wine cross-compiler. See docs/BUILDING.md')
endif

# Get Wine version
wine_version = run_command(
    'sh', '-c', '''wine --version | grep -oE '[0-9]+\.[0-9]+' | head -n1''',
    check : false
)

if wine_version.returncode() == 0
    wine_ver = wine_version.stdout().strip()
    message('Targeting Wine @0@'.format(wine_ver))
    
    # Require Wine 10.0+ for best compatibility
    if wine_ver.version_compare('<10.0')
        warning('Wine @0@ detected. Wine 10.0+ recommended for best results.'.format(wine_ver))
    endif
else
    warning('Unable to determine Wine version')
endif

# ============================================================================
# Dependencies
# ============================================================================

include_dir = include_directories('src/include', is_system : true)

# Native Linux dependencies
xcb_dep = dependency('xcb')
xcb_composite_dep = dependency('xcb-composite')
xcb_xfixes_dep = dependency('xcb-xfixes')
x11_dep = dependency('x11')
x11_xcb_dep = dependency('x11-xcb')

threads_dep = dependency('threads')

# Check for XShm extension
xext_dep = dependency('xext', required : false)
has_xshm = xext_dep.found() and cc.has_header('X11/extensions/XShm.h')

if has_xshm
    native_cpp_args += '-DHAVE_XSHM'
endif

# Wine-specific dependencies (found via winegcc)
winegcc = meson.get_compiler('cpp', native : false)
wine_threads_dep = declare_dependency(link_args : '-lpthread')
wine_uuid_dep = declare_dependency(link_args : '-luuid')
wine_ole32_dep = declare_dependency(link_args : '-lole32')
wine_shell32_dep = declare_dependency(link_args : '-lshell32')

# ============================================================================
# Subdirectories
# ============================================================================

subdir('src/common')
subdir('src/native')
subdir('src/wine-host')

# ============================================================================
# Native Linux Plugin Library
# ============================================================================

shared_library(
    vst3_plugin_name,
    native_sources,
    native : true,
    include_directories : include_dir,
    dependencies : [
        xcb_dep,
        xcb_composite_dep,
        xcb_xfixes_dep,
        x11_dep,
        x11_xcb_dep,
        threads_dep,
    ],
    cpp_args : native_cpp_args,
    install : true,
    install_dir : get_option('libdir') / 'vst3bridge',
)

# ============================================================================
# Wine Host Executables
# ============================================================================

# 64-bit host
if is_64bit_system
    executable(
        host_name_64bit,
        wine_sources,
        native : false,
        include_directories : include_dir,
        dependencies : [
            wine_threads_dep,
            wine_uuid_dep,
            wine_ole32_dep,
            wine_shell32_dep,
        ],
        cpp_args : wine_64bit_cpp_args,
        link_args : ['-m64', '-mwindows'],
        install : true,
        install_dir : get_option('bindir'),
    )
endif

# 32-bit host (bitbridge)
if with_bitbridge
    executable(
        host_name_32bit,
        wine_sources,
        native : false,
        include_directories : include_dir,
        dependencies : [
            wine_threads_dep,
            wine_uuid_dep,
            wine_ole32_dep,
            wine_shell32_dep,
        ],
        cpp_args : wine_32bit_cpp_args,
        link_args : ['-m32', '-mwindows'],
        install : true,
        install_dir : get_option('bindir'),
    )
endif

# ============================================================================
# Summary
# ============================================================================

summary({
    'Wine version': wine_ver,
    '64-bit host': is_64bit_system,
    '32-bit bitbridge': with_bitbridge,
    'XShm support': has_xshm,
}, section : 'Configuration')
