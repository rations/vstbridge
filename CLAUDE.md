# VST3 GDI Capture Bridge — Project State

## What This Is

A patch on top of **yabridge 5.1.1** (`/home/human/vst3bridge/yabridge-5.1.1/`) that
makes Windows VST3 plugin GUIs work with Wine >= 9.21. The original yabridge
window-embedding approach (`xcb_reparent_window`) broke in that Wine version.

**The fix strategy:** GDI capture — the plugin renders inside a hidden Win32
HWND; a capture thread reads frames via `BitBlt(SRCCOPY|CAPTUREBLT)` into a
POSIX SHM ring buffer; the native Linux side reads those frames and blits them
into an X11 child window under the DAW's panel.

**Build directory:** `yabridge-5.1.1/build-mod/`  
**Deploy targets:** `~/.local/share/yabridge/libyabridge-vst3.so` and
`~/.local/share/yabridge/yabridge-host.exe.so` + `yabridge-host.exe`

---

## Files We Added / Changed

| File | What changed |
|------|-------------|
| `src/common/frame_shm.h/.cpp` | POSIX SHM triple-buffer ring + input event SPSC ring |
| `src/wine-host/gdi_capture.h/.cpp` | `BitBlt(SRCCOPY|CAPTUREBLT)` GDI capture into 32bpp BGRA buffer |
| `src/wine-host/editor.h/.cpp` | GDI mode branch: hidden container at (-10000,-10000), SHM open, Win32Thread capture loop, `ChildWindowFromPointEx` mouse routing, `SW_SHOWNOACTIVATE` in `show()` |
| `src/wine-host/bridges/vst3.cpp` | Passes `frame_shm_name` to Editor constructor |
| `src/common/serialization/vst3/plug-view/plug-view.h` | `frame_shm_name` field in `Attached` struct |
| `src/plugin/bridges/vst3-impls/plug-view-proxy.h/.cpp` | Creates SHM, XCB connection, render window, GC; starts render thread; `onFocus(false)` intercepted → always sends `true` in GDI mode; periodic `xcb_get_window_attributes` health check triggers `render_reconnect()` |

---

## What Works

- Single plugin: GUI displays, clicks/drags/wheel work
- Two plugins on the **same track**: both display and switch correctly, dials work
- Two plugins on **different tracks**: both display and work — but Track 1 initially
  freezes when Track 2 loads. After any fix action on Track 1 (see below), both
  tracks work simultaneously.

---

## Remaining Bug — Different-track initial freeze

### Symptom

1. Load Plugin A on Track 1 — works
2. Open Track 2 FX window with Plugin B — **Track 1 freezes** (stops updating)
3. Fix action on Track 1: close + reopen its FX window, or add/switch a plugin
   in Track 1's FX list → **both tracks work** simultaneously

### What is confirmed

- The Wine-side GDI capture for Track 1 keeps running during the freeze
  (briefly removing Track 1's plugin reveals `gdi_hide_container_` at top-left
  with the plugin rendering correctly)
- After the fix action, both tracks genuinely work at the same time — multiple
  dials on both tracks respond to mouse input

### What has been ruled out

- **XCB connection/BadDrawable errors**: `render_reconnect()` handles these;
  health check also polls `xcb_get_window_attributes` for `UNVIEWABLE` state
- **Win32 `WM_KILLFOCUS` stealing focus**: `show()` now uses `SW_SHOWNOACTIVATE`
  so Track 2's window appearing does not send `WM_KILLFOCUS` to Track 1's plugin
- **`IPlugView::onFocus(false)` pausing rendering**: intercepted on native side,
  always sends `true` in GDI mode
- **Raising render_window_ Z-order**: tried, interfered with Reaper's own
  stacking management
- **`PrintWindow(PW_RENDERFULLCONTENT)`**: returns black — tested plugins do not
  implement `WM_PRINT`
- **`BitBlt` without `CAPTUREBLT`**: Wine does not maintain backing stores for
  off-screen windows without it — returns black
- **Overlapping `gdi_hide_container_` positions**: tried unique positions per
  plugin, broke Track 2 independently
- **Periodic `InvalidateRect`**: does not help when plugin has already paused its
  rendering thread internally

### Current hypothesis

The freeze is triggered by something in the Wine/Win32 layer when Track 2's
Editor is created on the shared `main_context_` thread — possibly Win32 window
activation state, a Wine-internal lock during `plug_view_->attached()`, or
message queue interactions between the two plugin HWNDs in the same process.
The fix action works because it recreates Track 1's resources (`attached()`
creates fresh SHM + XCB + render_window_), which is visible and updating.

### Next thing to try

Investigate what specific Win32/Wine event during Track 2's `attached()` causes
Track 1's display to stall. Options:
- Log `BitBlt` return value in the capture thread to see if it fails during the
  freeze (would confirm Wine-side vs native-side cause)
- Check if `main_context_.run_in_context(...)` for Track 2's `attached()` blocks
  Track 1's Win32 message pump long enough to lose frames from SHM ring
- Try calling `RedrawWindow(..., RDW_ALLCHILDREN | RDW_UPDATENOW)` on Track 1's
  `win32_window_` from within Track 2's `attached()` handler on the Wine side

---

## Steps Completed (Do Not Redo)

1. ✅ Replaced Xlib (`XOpenDisplay`) in render thread with XCB
2. ✅ Replaced `WindowFromPoint` with `ChildWindowFromPointEx` in capture thread
3. ✅ `render_reconnect()` on XCB error events (BadWindow/BadMatch/BadDrawable)
4. ✅ `SW_SHOWNOACTIVATE` in `Editor::show()` for GDI mode
5. ✅ `onFocus(false)` → `onFocus(true)` intercept in `plug-view-proxy.cpp`
6. ✅ Periodic `xcb_get_window_attributes` health check in render thread

---

## Build & Deploy Commands

```bash
cd /home/human/vst3bridge/yabridge-5.1.1
ninja -C build-mod 2>&1 | tail -10

# Deploy native side (plug-view-proxy changes):
cp build-mod/libyabridge-vst3.so ~/.local/share/yabridge/

# Deploy Wine side (editor/gdi_capture changes):
cp build-mod/yabridge-host.exe.so ~/.local/share/yabridge/
cp build-mod/yabridge-host.exe ~/.local/share/yabridge/

# Force rebuild all changed files:
touch src/wine-host/editor.cpp src/wine-host/gdi_capture.cpp \
      src/plugin/bridges/vst3-impls/plug-view-proxy.cpp
ninja -C build-mod
```

---

## Known Non-Issues

- `gdi_hide_container_` briefly appears at top-left when a plugin is removed —
  Wine repositions the HWND as a top-level window for ~1 second before
  `DeferredWin32Window` posts `WM_CLOSE`. Expected.
- Nembrini Audio preset paths may require the Windows VST3 preset directory to
  exist under the Wine prefix.
- Mouse wheel works on some plugins (ML Sound Lab) but not others (Nembrini) —
  deferred until the multi-track freeze is resolved.
