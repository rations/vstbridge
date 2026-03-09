# Copyright (C) 2026
# VST3 Bridge - Wine VST3 Host Bridge
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.

# @file meson.build
# @brief Meson build configuration for VST3 Bridge

project('vst3bridge', 'cpp',
    version : '0.1.0',
    meson_version : '>=0.56',
    default_options : [
        'warning_level=3',
        'cpp_std=c++17',
        'buildtype=release',
        'b_pie=false',
        'b_lto=true',
    ]
)

# Build options
with_bitbridge = get_option('bitbridge')

# Detect architecture
host_arch = host_machine.cpu_family()
is_64bit = host_arch == 'x86_64'

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------

common_cpp_args = [
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
    '-fno-strict-aliasing',
    '-msse2',
]

if get_option('buildtype').contains('debug')
    common_cpp_args += ['-DDEBUG', '-D_DEBUG']
else
    common_cpp_args += ['-DNDEBUG']
endif

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------

inc_common  = include_directories('src/common', 'src/vst3-sdk')
inc_native  = include_directories('src/native')
inc_wine    = include_directories('src/wine-host')

# ---------------------------------------------------------------------------
# Common sources (shared between native and Wine builds)
# ---------------------------------------------------------------------------

common_sources = files(
    'src/common/socket.cpp',
    'src/common/shared_memory.cpp',
    'src/common/gui_events.cpp',
    'src/common/stream.cpp',
)

# ---------------------------------------------------------------------------
# Native Linux plugin library
# ---------------------------------------------------------------------------

native_sources = files(
    'src/native/vst3_entry.cpp',
    'src/native/vst3_plugin.cpp',
    'src/native/plugin_factory.cpp',
    'src/native/plugin_proxy.cpp',
    'src/native/x11_window.cpp',
    'src/native/x11_capture.cpp',
    'src/native/socket_client.cpp',
    'src/native/audio_shm.cpp',
    'src/native/gui_handler.cpp',
    'src/native/ipc_client.cpp',
)

native_cpp_args = common_cpp_args + [
    '-DBUILDING_NATIVE',
]

# Native library dependencies
thread_dep        = dependency('threads')
x11_dep           = dependency('x11',         required : true,  static : false)
xcb_dep           = dependency('xcb',         required : true,  static : false)
xcb_keysyms_dep   = dependency('xcb-keysyms', required : false, static : false)
xext_dep          = dependency('xext',        required : false, static : false)  # XShm

native_deps = [thread_dep, x11_dep, xcb_dep]
if xcb_keysyms_dep.found()
    native_deps += xcb_keysyms_dep
endif
if xext_dep.found()
    native_deps += xext_dep
endif

vst3bridge_lib = shared_library('vst3bridge',
    common_sources,
    native_sources,
    include_directories : [inc_common, inc_native],
    cpp_args            : native_cpp_args,
    dependencies        : native_deps,
    link_args           : ['-Wl,-Bdynamic', '-lrt'],
    install             : not meson.is_cross_build(),
    install_dir         : 'lib/vst3/vst3bridge',
    native              : true,
)

# ---------------------------------------------------------------------------
# Wine host executable (cross-compiled for Windows / Wine)
# ---------------------------------------------------------------------------

if host_machine.system() == 'linux' or host_machine.system() == 'freebsd'

    wine_sources = files(
        'src/wine-host/host_main.cpp',
        'src/wine-host/vst3_host.cpp',
        'src/wine-host/plugin_instance.cpp',
        'src/wine-host/host_application.cpp',
        'src/wine-host/component_handler.cpp',
        'src/wine-host/window_manager.cpp',
        'src/wine-host/gdi_capture.cpp',
        'src/wine-host/socket_server.cpp',
        'src/wine-host/audio_shm_host.cpp',
        'src/wine-host/audio_processor.cpp',
        'src/wine-host/gui_event_receiver.cpp',
        'src/wine-host/ipc_host.cpp',
        'src/wine-host/parameter_changes.cpp',
    )

    # All common sources are compiled again for the Wine target.
    # The Wine host does NOT use the POSIX socket.cpp — it has its own
    # Winsock socket implementation in host_main.cpp.  However, it does
    # use shared_memory.cpp (POSIX SHM) and gui_events.cpp / stream.cpp.
    wine_common_sources = files(
        'src/common/shared_memory.cpp',
        'src/common/gui_events.cpp',
        'src/common/stream.cpp',
    )

    wine_cpp_args = common_cpp_args + [
        '-DBUILDING_WINE_HOST',
        '-DNOMINMAX',
        '-D_WIN32_WINNT=0x0601',
        # Suppress overly strict warnings that triggered by Wine/Windows headers
        '-Wno-unknown-pragmas',
        '-Wno-pedantic',
    ]

    # Include paths as strings for wineg++ custom_target
    wine_inc_paths = [
        '-I' + meson.source_root() + '/src/common',
        '-I' + meson.source_root() + '/src/vst3-sdk',
        '-I' + meson.source_root() + '/src/wine-host',
    ]

    winegpp = find_program('wineg++', required : false)

    if winegpp.found()
        all_wine_sources = wine_common_sources + wine_sources

        wine_host_exe = custom_target('vst3bridge-host',
            output   : 'vst3bridge-host.exe',
            input    : all_wine_sources,
            command  : [winegpp, '@INPUT@', '-o', '@OUTPUT@',
                        '-std=c++17'] +
                       wine_inc_paths +
                       wine_cpp_args +
                       ['-lstdc++', '-lm'],
            build_by_default : true,
            install  : false,
        )

        if with_bitbridge and is_64bit
            wine_host_32 = custom_target('vst3bridge-host-32',
                output   : 'vst3bridge-host-32.exe',
                input    : all_wine_sources,
                command  : [winegpp, '@INPUT@', '-o', '@OUTPUT@',
                            '-m32', '-std=c++17'] +
                           wine_inc_paths +
                           wine_cpp_args +
                           ['-lstdc++', '-lm'],
                build_by_default : false,
                install  : false,
            )
        endif
    else
        message('wineg++ not found — Wine host build skipped')
    endif

endif

# ---------------------------------------------------------------------------
# Chainloader helper (loads the bridged .so from the real plugin path)
# ---------------------------------------------------------------------------

chainloader_sources = files(
    'src/chainloader/vst3_chainloader.cpp',
)

if false  # Disabled by default; enable via build option if needed
    chainloader_lib = shared_library('vst3bridge-chain',
        chainloader_sources,
        include_directories : inc_common,
        cpp_args            : native_cpp_args,
        install             : false,
    )
endif

# ---------------------------------------------------------------------------
# Unit tests (optional, requires gtest)
# ---------------------------------------------------------------------------

gtest_dep = dependency('gtest', required : false)
if gtest_dep.found()
    test_sources = files(
        'tests/test_protocol.cpp',
        'tests/test_shared_memory.cpp',
    )

    test_exe = executable('vst3bridge-tests',
        test_sources,
        common_sources,
        include_directories : inc_common,
        dependencies        : [gtest_dep, thread_dep],
        cpp_args            : native_cpp_args + ['-lgtest_main'],
        link_args           : ['-lrt'],
    )
    test('unit-tests', test_exe)
endif
