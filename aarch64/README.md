# vstbridge on aarch64

Work in progress on the `arm64` branch. The goal is running Windows plugins on a Raspberry Pi 5
(Pivuan) with FEX: Wine runs natively on aarch64, and only the plugin's x86_64 code is emulated,
through Wine's ARM64EC support and FEX's `libarm64ecfex.dll`.

## What's here

- `versions.sh`: pinned Wine, wine-staging, llvm-mingw and FEX versions.
- `build-wine.sh`: builds wine-staging for `arm64ec,aarch64,i386` with the patches below,
  installed to `/opt/vstbridge/wine`.
- `build-fex.sh`: builds FEX's `libarm64ecfex.dll`, `libwow64fex.dll` and their unixlibs, which
  install into the same Wine tree.
- `wine/patches/`:
  - `0001`: `IDXGIOutput::WaitForVBlank()` waits for a refresh interval instead of returning
    `E_NOTIMPL`. JUCE 8's Direct2D renderer spins on the stub, so without this, plugin editors
    paint one frame and stop responding unless DXVK is installed.
  - `0002`: new prefixes use FEX for x86_64 and i386 code (the `HKLM\Software\Microsoft\Wow64`
    keys), as Proton's Wine does. Upstream points them at stubs.
  - `0003`: a shortcut an installer puts on the user's Desktop moves to the Public Desktop once
    winemenubuilder has written its launcher. Wine's Desktop folder is the Linux desktop, so
    the `.lnk` would otherwise show next to its launcher as a plain file.
  - `0004`: `QueryServiceConfig2W` reports a service's failure actions (none) instead of failing.
    Installers made with WiX's ServiceConfig, such as iLok's, read them before setting them.
  - `0005`: `msiexec.exe` and `rundll32.exe` declare Windows 10 compatibility in their manifests,
    as on Windows. MSI custom actions run in them, and without it they see Windows 8 in a
    Windows 10 prefix. iLok's installer refuses to install below Windows 10.
  - `0006`: on ARM64, the prefix reports the x86-64 CPU FEX emulates (GenuineIntel) in
    `HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor`. The Windows programs and plugins run here
    are x86-64, and installers that find an ARM CPU there (iLok's) install ARM64 files they can't
    load.

- `build-debs.sh`: packages everything for the Pivuan apt repository (below).

The `aarch64 build` GitHub Actions workflow runs on arm64 runners:
- It runs the two build scripts (cached, so Wine is only rebuilt when its scripts, versions or
  patches change) and uploads `vstbridge-wine-aarch64.tar.xz` and `vstbridge-fex-aarch64.tar.xz`.
- It builds vstbridge itself into `vstbridge-aarch64.tar.gz`.
- It packages them as .debs, installs those in a clean Debian trixie container, and runs
  `vstbridgectl sync` there.

The packages reach the Pivuan apt repository through the "vstbridge for Pivuan" workflow in
rations/pivuan. It calls this workflow on the `arm64` branch as a reusable workflow, so the build
runs in rations/pivuan, and then publishes the packages from that same run with its own token and
the `PIVUAN_APT_SIGNING_KEY` secret, as XLibre is published. This repository needs no secrets and
makes no releases for it.

## Pivuan packages

Pivuan Audio installs vstbridge from the Pivuan apt repository. `vstbridge` depends on the rest.

| Package | Contents |
|---|---|
| `wine-fex` | Wine and FEX in `/opt/vstbridge/wine`. `wine`, `winecfg` and Wine's other programs in `/usr/bin`. The "Wine Windows Program Loader" for opening `.exe` and `.msi` files, and Wine Configuration and Uninstall Windows Programs menu entries. |
| `wine-fex-i386` | Wine's 32-bit Windows side, for 32-bit programs such as many installers. |
| `wine-fex-mono` | The wine-mono installer, so creating a Wine prefix needs no download. |
| `vstbridge` | The plugin libraries and the plugin host in `/usr/lib/aarch64-linux-gnu`, plus `vstbridgectl` and `vstbridgectl-gtk` in `/usr/bin`. |

- The Wine packages conflict with Debian's `wine`, because both install `/usr/bin/wine`.
- The apt repository is a GitHub branch, and GitHub refuses files over 100 MiB. That's why Wine is
  split into three packages, and why its Windows DLLs are stripped of their debug info (2.4 GB
  installed before, 1 GB after).
- Everything uses the default Wine prefix, `~/.wine`.

## The plugin host on aarch64

Wine can't run Winelib `.exe.so` programs on aarch64, so vstbridge's plugin host is built as a
regular PE executable instead (`meson setup -Dwine-host=pe --cross-file cross-arm64ec.conf`):

- `vstbridge-host.exe` is ARM64EC code, so it runs natively while the x86_64 plugin it loads
  runs under FEX in the same process.
- `vstbridge-host-unixlib.so` is a native aarch64 library that the host loads through Wine's
  unixlib mechanism. The host calls into it for everything that needs Linux: the Unix domain
  sockets to the native plugin, the shared memory audio buffers, realtime scheduling, and xcb
  for the editor embedding (`vstbridge/src/wine-host/unixlib/`).

Wine's own AF_UNIX support (wine-staging's `ws2_32-af_unix` patches) was measured and rejected:
a 64 byte round trip took about 123 µs, against 13.7 µs through the unixlib and 13.6 µs between
two native Linux processes on the Pi 5.

The native plugin starts the host with `/opt/vstbridge/wine/bin/wine` (the `wine-loader` build
option), unless `WINELOADER` is set.

The same host can be built for x86_64 with `--cross-file cross-mingw-x86_64.conf`, for testing it
with x86_64 Wine.

## Installing vstbridge on the Pi

After the Wine and FEX tarballs below are installed:

```sh
# The tarball holds a `vstbridge` directory. install.sh copies it to
# ~/.local/share/vstbridge and adds vstbridgectl to the menu, as for x86_64.
tar -xzf vstbridge-aarch64.tar.gz
vstbridge/install.sh

# The plugins' directory inside the Wine prefix, then set up the bridged copies (or use
# vstbridgectl from the menu)
~/.local/share/vstbridge/vstbridgectl add "$HOME/.wine/drive_c/Program Files/Common Files/VST3"
~/.local/share/vstbridge/vstbridgectl sync
```

Everything uses Wine's default prefix, `~/.wine`. Keep plugins there too: `vstbridgectl sync`
runs the plugin host through Wine without a `WINEPREFIX`, so it creates `~/.wine` if it doesn't
exist yet (the "Wine configuration is being updated" window). Plugins are run in the prefix
they're installed in.

## Trying the Wine build on the Pi

Both tarballs extract at `/` and only write to `/opt/vstbridge/wine`:

```sh
sudo tar -C / -xJf vstbridge-wine-aarch64.tar.xz
sudo tar -C / -xJf vstbridge-fex-aarch64.tar.xz

export PATH=/opt/vstbridge/wine/bin:$PATH
wineboot -i

# Should print libarm64ecfex.dll and libwow64fex.dll
wine reg query 'HKLM\Software\Microsoft\Wow64\amd64'
wine reg query 'HKLM\Software\Microsoft\Wow64\x86'

# An x86_64 JUCE program to test FEX and GUI rendering with (pluginval_Windows.zip from
# https://github.com/Tracktion/pluginval/releases)
wine pluginval.exe
```

Useful while testing:

- `WINEDEBUG=+d3d,+d3d11` shows which Direct3D feature level wined3d gets from the GPU.
- `LIBGL_ALWAYS_SOFTWARE=1` switches to llvmpipe if the V3D GL driver isn't enough.

## Baseline to record on the Pi

```sh
getconf PAGESIZE          # FEX expects 4096
grep -m1 Features /proc/cpuinfo
ls -l /dev/ntsync         # needs the ntsync module loaded
ulimit -r -l              # realtime priority and memlock limits
cat /proc/self/cgroup
```
