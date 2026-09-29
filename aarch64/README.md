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

The `aarch64 Wine and FEX` GitHub Actions workflow runs both scripts on arm64 runners and uploads
`vstbridge-wine-aarch64.tar.xz` and `vstbridge-fex-aarch64.tar.xz`. The `aarch64 build` workflow
builds the native vstbridge libraries and vstbridgectl. The plugin host (an ARM64EC PE executable)
doesn't exist yet.

## Trying the Wine build on the Pi

Both tarballs extract at `/` and only write to `/opt/vstbridge/wine`:

```sh
sudo tar -C / -xJf vstbridge-wine-aarch64.tar.xz
sudo tar -C / -xJf vstbridge-fex-aarch64.tar.xz

export PATH=/opt/vstbridge/wine/bin:$PATH
export WINEPREFIX=$HOME/.wine-vstbridge-arm64
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
