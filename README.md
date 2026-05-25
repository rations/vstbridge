# vstbridge

vstbridge is a fork of [yabridge](https://github.com/robbert-vdh/yabridge) by Robbert van der Helm — a Wine-based bridge that allows Linux DAWs to load Windows VST2, VST3, and CLAP plugins as if they were native plugins.

Yabridge is an excellent project and this fork would not exist without it. All credit for the core bridging architecture, the IPC design, the chainloader system, and the VST3/CLAP compatibility work belongs to Robbert and the yabridge contributors. If you are on a distro with yabridge packages (Arch, Manjaro, etc.) and do not need the fixes below, use upstream yabridge.

---

## What vstbridge adds

### Wine ≥ 9.21 plugin GUI fix

Upstream yabridge 5.1.1 breaks with Wine ≥ 9.21: plugins render their GUI but are completely non-interactive. Knobs, buttons, and sliders do not respond to mouse input at all.

**Root cause:** When a plugin window is embedded via XEmbed, Wine's `make_window_embedded()` sets an internal `wm_state_serial` field and waits for a `WM_STATE` PropertyNotify from the window manager. For embedded (non-root) windows the window manager never sees the window, so `wm_state_serial` is never cleared. This permanently blocks `window_update_client_config` — the function that updates the Win32 HWND's screen position. The HWND stays at `{0, 0, w, h}` instead of its true screen position `{abs_x, abs_y, abs_x+w, abs_y+h}`. When the user clicks at `(abs_x+dx, abs_y+dy)`, Wine translates it to client coordinates relative to `(0, 0)`, producing `(abs_x+dx, abs_y+dy)` — coordinates outside the plugin window — so the click is discarded.

**The fix** (`src/wine-host/editor.cpp`): In XEmbed mode, `fix_local_coordinates()` calls `SetWindowPos()` directly instead of sending a `ConfigureNotify` event. `SetWindowPos()` updates the Win32 HWND rect without going through the `wm_state_serial`-gated path. Wine's embedded-window guard in `window_set_config` then prevents the actual X11 window from being moved visually, so the window stays correctly positioned on screen. Mouse coordinates are translated correctly and input works.

XEmbed is also enabled by default in this fork (`editor_xembed = true` in `src/common/configuration.h`).

For the full technical analysis see [readme-fixes.md](readme-fixes.md).

### GTK3 GUI for vstbridgectl

The upstream yabridgectl management tool is command-line only. vstbridge adds a GTK3 graphical interface (`vstbridgectl-gtk`) for users who prefer not to use the terminal.

The GUI has four tabs:

- **Directories** — add and remove the directories vstbridge scans for Windows plugins
- **Sync** — run a sync with options for force, prune, verbose output, and skipping the compatibility check; live output is shown in the window
- **Status** — show the current vstbridge installation status and detected Wine version
- **Settings** — set the vstbridge installation path, choose between centralized and inline VST2 install modes, and toggle the Wine/vstbridge compatibility check

---

## Requirements

- Wine Staging. This fork is tested with Wine Staging 11.09.
- Winetricks for DXVK
- A 64-bit Linux DAW (REAPER, Ardour etc.)
- GTK3 (for the vstbridgectl-gtk GUI only)

---

## Installation

Download the latest release tarball from the [releases page](https://github.com/rations/vstbridge/releases), then extract and run the installer:

```bash
tar -xf vstbridge-0.0.1.tar.gz
cd vstbridge
chmod +x install.sh
./install.sh
```

The installer copies everything to `~/.local/share/vstbridge/` and adds `vstbridgectl-gtk` to your application menu. No root or sudo is required.

After installing, open **vstbridgectl** from your application menu, or run it directly:

```bash
~/.local/share/vstbridge/vstbridgectl-gtk
```

### Directories tab

Add the folders on your system that contain Windows VST2, VST3, or CLAP plugin files. Typical locations inside a Wine prefix are:

- `~/.wine/drive_c/Program Files/VstPlugins` — VST2 plugins
- `~/.wine/drive_c/Program Files/Common Files/VST3` — VST3 plugins
- `~/.wine/drive_c/Program Files/Common Files/CLAP` — CLAP plugins

Click **Add** to open a folder browser. Hidden directories such as `.wine` are visible in the browser. Add as many directories as needed, then move to the Sync tab.

### Sync tab

Click **Sync** to create the bridge `.so` files for every plugin found in your directories. The output log shows what was added, skipped, or failed. The available options are:

- **Force** — recreate all bridges even if they are already up to date
- **Prune** — remove bridges for plugins that no longer exist
- **Verbose** — show detailed output for every plugin processed
- **No verify** — skip the Wine/vstbridge compatibility check (useful if the check gives a false warning)

Run a sync whenever you install or remove Windows plugins. If the output says "found N leftover files", tick **Prune** and sync again to remove old bridge files that no longer correspond to any plugin.

### Status tab

Shows the currently installed vstbridge version, the detected Wine version, and whether the bridge files are found at the configured path. Use this to confirm everything is set up correctly.

### Settings tab

The vstbridge path is pre-configured to `~/.local/share/vstbridge` by the installer and does not need to be changed. You would only use this tab if you have moved the bridge files to a different location.

VST2 plugins are installed in centralized mode — vstbridgectl creates `~/.vst/vstbridge/` and mirrors your plugin directory structure inside it, placing the bridge `.so` alongside a copy of the Windows `.dll`. For example, a plugin at `~/.wine/drive_c/Program Files/VstPlugins/Toontrack/Superior Drummer.dll` becomes `~/.vst/vstbridge/Toontrack/Superior Drummer.so`.

### Configuring your DAW

After syncing, point your DAW at the vstbridge output directories so it can find the bridge plugins:

| Plugin format | DAW scan path |
|---------------|--------------|
| VST2 | `~/.vst/vstbridge/` |
| VST3 | `~/.vst3/vstbridge/` |
| CLAP | `~/.clap/vstbridge/` |

Add whichever paths apply to your plugin formats, then do a full plugin rescan in your DAW.

If you are migrating from yabridge, update your DAW's scan paths from `~/.vst/yabridge/`, `~/.vst3/yabridge/`, and `~/.clap/yabridge/` to the vstbridge equivalents above.

---

## Uninstalling

From the extracted release directory, run:

```bash
chmod +x uninstall.sh
./uninstall.sh
```

This removes all files from `~/.local/share/vstbridge/` and the desktop entry. Your plugin configuration (`~/.config/vstbridgectl/config.toml`) and any per-plugin `vstbridge.toml` files are left untouched.

---

## Building from source

vstbridge uses Meson + Ninja and cross-compiles the Wine-side host binary with `winegcc-stable`/`wineg++-stable`.

```bash
git clone https://github.com/rations/vstbridge
cd vstbridge

# Configure
meson setup build --buildtype=release --cross-file=cross-wine.conf

# Build
ninja -C build
```

### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `bitbridge` | false | Build a 32-bit host for 32-bit Windows plugins |
| `clap` | true | Build CLAP plugin support |
| `vst3` | true | Build VST3 plugin support |
| `system-asio` | false | Use the system `<asio.hpp>` instead of the bundled subproject |
| `winedbg` | false | Run the Wine host under winedbg |

Example with all plugin formats and 32-bit support:

```bash
meson setup build --buildtype=release --cross-file=cross-wine.conf \
  -Dclap=true -Dvst3=true -Dbitbridge=true
ninja -C build
```

### Build outputs

| File | Description |
|------|-------------|
| `libvstbridge-vst2.so` | Native .so loaded by the DAW for VST2 plugins |
| `libvstbridge-vst3.so` | Native .so loaded by the DAW for VST3 plugins |
| `libvstbridge-clap.so` | Native .so loaded by the DAW for CLAP plugins |
| `libvstbridge-chainloader-vst2.so` | Tiny stub copied per-plugin (VST2) |
| `libvstbridge-chainloader-vst3.so` | Tiny stub copied per-plugin (VST3) |
| `libvstbridge-chainloader-clap.so` | Tiny stub copied per-plugin (CLAP) |
| `vstbridge-host.exe` | Wine 64-bit plugin host process |
| `vstbridge-host-32.exe` | Wine 32-bit plugin host process (requires `bitbridge=true`) |

### Building vstbridgectl

```bash
cd tools/vstbridgectl

# Fetch dependencies and build both CLI and GUI
make
```

---

## Configuration

Plugin-level configuration is done via a `vstbridge.toml` file placed alongside plugin files or in a parent directory. See the upstream [yabridge configuration documentation](https://github.com/robbert-vdh/yabridge#configuration) for the available options — the TOML structure is unchanged.

### Environment variables

| Variable | Description |
|----------|-------------|
| `VSTBRIDGE_DEBUG_FILE` | Redirect log output to a file |
| `VSTBRIDGE_DEBUG_LEVEL` | Log verbosity: 0 (default) to 3 |
| `VSTBRIDGE_NO_WATCHDOG` | Disable the watchdog timer |
| `VSTBRIDGE_TEMP_DIR` | Override the temp directory used for sockets |

---

## License

GPL-3.0 — same as upstream yabridge. Copyright in individual source files is retained by the original author, Robbert van der Helm.
