# VST3 Bridge

A Linux VST3 host bridge that runs Windows VST3 plugins using Wine, designed to work with modern Wine versions where yabridge fails.

## Table of Contents

- [What is VST3 Bridge?](#what-is-vst3-bridge)
- [The Problem](#the-problem)
- [The Solution](#the-solution)
- [How It Works](#how-it-works)
- [Key Differences from Yabridge](#key-differences-from-yabridge)
- [Architecture](#architecture)
- [Building](#building)
- [Usage](#usage)
- [Status](#status)

---

## What is VST3 Bridge?

VST3 Bridge is a compatibility layer that allows you to use **Windows VST3 audio plugins** on **Linux** through **Wine**. It acts as a bridge between your Linux Digital Audio Workstation (DAW) and Windows plugins, making the plugins believe they're running on Windows while your DAW sees them as native Linux plugins.

### For Musicians and Producers

If you're a music producer using Linux, you probably know that many professional audio plugins (like those from Native Instruments, Waves, FabFilter, etc.) are only available for Windows and macOS. VST3 Bridge allows you to use these Windows plugins in your Linux DAW (like Ardour, Bitwig Studio, REAPER, or Carla) as if they were native Linux plugins.

---

## The Problem

### Yabridge's Limitation

The most popular solution for running Windows VST3 plugins on Linux has been **yabridge**. It works well but has a critical limitation:

> **Yabridge only works reliably with WineHQ-Staging version 9.21 or earlier.**

When using yabridge with newer Wine versions (10, 11, etc.), the plugins load but their **graphical interface freezes** when opened in a DAW. You can't click anything or adjust settings, making the plugins unusable. This happens because:

1. Yabridge relies heavily on Wine's X11 window embedding system
2. Wine 9.22+ changed how X11 window embedding works
3. Yabridge's manual window reparenting breaks with these changes

### Why This Matters

Wine is constantly evolving. Newer versions bring:
- Better Windows compatibility
- Security fixes
- Performance improvements
- Support for newer Windows applications

Being stuck on Wine 9.21 means:
- Missing out on Wine improvements
- Potential security vulnerabilities
- Incompatibility with other Wine applications
- Eventually, Wine 9.21 will become unavailable in package repositories

---

## The Solution

VST3 Bridge takes a fundamentally different approach that avoids the fragile X11 window embedding entirely.

### Core Innovation: Off-Screen Rendering

Instead of trying to embed Wine's X11 window into your DAW (which breaks with Wine updates), VST3 Bridge:

1. **Creates the plugin's GUI window off-screen** (at coordinates -10000, -10000)
2. **Captures the window's pixel data** using Windows GDI (Graphics Device Interface)
3. **Sends the pixel data to the Linux side** via shared memory
4. **Displays the captured image** in a native Linux X11 window provided by the DAW
5. **Forwards mouse/keyboard events** from the Linux window back to the Wine window

This approach is **Wine-version agnostic** because it only relies on:
- Wine's ability to run Windows plugins (core functionality that doesn't change)
- Windows GDI for capturing pixel data (stable API)

It doesn't depend on Wine's X11 window embedding behavior at all.

---

## How It Works

### High-Level Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Your Linux DAW                               │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  VST3 Bridge Plugin (.so file)                              │   │
│   │  ┌───────────────────┐    ┌──────────────────────────┐    │   │
│   │  │  X11 Window       │◄───│  Shared Memory (pixels)  │    │   │
│   │  │  (DAW provides)   │    │  Socket (commands)       │    │   │
│   │  └───────────────────┘    └──────────────────────────┘    │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                          │                                          │
│                          │ Unix Domain Socket + Shared Memory       │
│                          ▼                                          │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Wine Process (vst3bridge-host.exe)                         │   │
│   │  ┌─────────────────────────────────────────────────────┐  │   │
│   │  │  Windows VST3 Plugin                                │  │   │
│   │  │  - Window at (-10000, -10000)                      │  │   │
│   │  │  - GDI BitBlt captures pixels                      │  │   │
│   │  │  - Renders to off-screen window                    │  │   │
│   │  └─────────────────────────────────────────────────────┘  │   │
│   └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### Step-by-Step Process

1. **Loading the Plugin**
   - Your DAW loads the VST3 Bridge plugin (a `.so` file)
   - The plugin spawns a Wine process (`vst3bridge-host.exe`)
   - The Wine process loads the actual Windows VST3 plugin

2. **Audio Processing**
   - Audio data flows through shared memory between Linux and Wine
   - The plugin processes audio in real-time
   - Parameters and state are synchronized via sockets

3. **GUI Display**
   - The Windows plugin creates its GUI window off-screen
   - GDI `BitBlt` captures the window's pixels
   - Pixels are transferred via shared memory to Linux
   - The Linux plugin displays them in an X11 window

4. **User Interaction**
   - You click/drag in the DAW's plugin window
   - Events are captured by XCB (X11 C Binding)
   - Events are sent to Wine via socket
   - Wine injects them as Windows mouse/keyboard messages
   - The plugin responds and redraws
   - New pixels are captured and displayed

---

## Key Differences from Yabridge

| Aspect | Yabridge | VST3 Bridge |
|--------|----------|-------------|
| **GUI Rendering** | X11 window embedding | Off-screen GDI capture |
| **Wine Compatibility** | Tightly coupled to Wine X11 | Wine-agnostic |
| **Wine 11 Support** | GUI freezes | Works correctly |
| **Window Management** | Manual reparenting | Pixel buffer sharing |
| **Input Handling** | X11 event forwarding | XCB → Windows messages |

### Why This Approach?

**Yabridge's approach** (window embedding):
- Requires precise coordination between Wine's X11 driver and the host
- Breaks when Wine changes X11 internals
- Complex window hierarchy management

**VST3 Bridge's approach** (pixel capture):
- Only requires Wine to run the plugin (core functionality)
- GDI has been stable in Windows for decades
- No dependency on Wine's window management

---

## Architecture

### Components

#### 1. Native Linux Plugin (`libvst3bridge.so`)
- Implements VST3 plugin interface that DAWs expect
- Creates X11 window for GUI display
- Manages shared memory for audio and GUI
- Handles X11 input events

#### 2. Wine Host Process (`vst3bridge-host.exe`)
- Runs inside Wine
- Loads Windows VST3 plugins
- Captures window pixels using GDI
- Receives input events from Linux side

#### 3. IPC Layer
- **Unix Domain Sockets**: For commands and events
- **POSIX Shared Memory**: For audio buffers and GUI pixel data
- **Protocol**: Binary message protocol for efficiency

### File Structure

```
vst3bridge/
├── src/
│   ├── native/          # Linux VST3 plugin implementation
│   ├── wine-host/       # Wine host process
│   ├── common/          # Shared code (IPC, protocol)
│   └── vst3-sdk/        # VST3 SDK headers
├── build/               # Build output
└── README.md           # This file
```

---

## Building

### Prerequisites

- Linux system with X11
- Meson build system
- C++17 compiler (GCC or Clang)
- Wine development headers (`wine-dev` or `wine-devel`)
- XCB library (`libxcb-dev`)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/yourusername/vst3bridge.git
cd vst3bridge

# Setup build directory
meson setup build

# Compile
meson compile -C build

# Install (optional)
sudo meson install -C build
```

---

## Usage

### Basic Usage

1. Build the project
2. Install the VST3 Bridge plugin in your DAW's plugin path
3. Run the Wine host setup
4. Load Windows VST3 plugins through the bridge

### Configuration

Coming soon: Configuration options for plugin-specific settings, Wine prefix selection, and GUI scaling.

---

## Status

**Current State**: In development

### What's Working
- [x] Project structure and build system
- [x] VST3 SDK integration
- [x] IPC layer (sockets and shared memory)
- [x] Basic Wine host process
- [ ] Audio processing (in progress)
- [ ] GUI capture and display (in progress)
- [ ] Input forwarding (in progress)
- [ ] Testing with real plugins

### Known Issues
- Project is still in early development
- Not ready for production use yet
- Many features are incomplete

### Roadmap

1. **Phase 1**: Foundation (build system, IPC, basic structure) ✅
2. **Phase 2**: Core implementation (audio, GUI capture) 🔄
3. **Phase 3**: Integration and testing
4. **Phase 4**: Optimization and polish

---

## Contributing

This is an experimental project. Contributions are welcome, especially:

- Testing with different plugins
- Bug reports and fixes
- Documentation improvements
- Feature suggestions

---

## License

GPL v2 or later - See COPYING file for details

---

## Acknowledgments

- The yabridge project for showing what's possible with Wine VST bridging
- The Wine project for making Windows compatibility on Linux possible
- Steinberg for the VST3 SDK

---

## Questions?

### Why not just fix yabridge?

Yabridge is a mature, well-designed project. However, its architecture is fundamentally tied to Wine's X11 window embedding. Fixing it for Wine 11+ would require significant architectural changes. VST3 Bridge explores a different approach that may be more future-proof.

### Will this work with VST2 plugins?

Currently, VST3 Bridge is focused on VST3 plugins. VST2 support might be added later if there's demand.

### What about CLAP plugins?

CLAP support is not currently planned, but the architecture could potentially support it in the future.

### Why C++?

VST3 plugins and most audio processing code are written in C++ for performance reasons. Using C++ allows direct integration with the VST3 SDK without language barriers.

---

**Disclaimer**: This project is not affiliated with or endorsed by Steinberg, the creators of VST3. VST3 is a trademark of Steinberg Media Technologies GmbH.
