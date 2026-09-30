#!/bin/bash
#
# Package vstbridge and its Wine for Pivuan (Devuan excalibur, arm64) as .debs, from the
# tarballs the aarch64 CI workflow builds. Run natively on aarch64 Debian trixie, which has the
# same libraries as excalibur, so the Depends dpkg-shlibdeps writes resolve on Pivuan.
#
# Usage: aarch64/build-debs.sh <out dir> <wine .tar.xz> <fex .tar.xz> <vstbridge .tar.gz> <vstbridge version>
#
# Packages:
#   wine-fex       Wine (ARM64EC) with FEX in $WINE_PREFIX, `wine` and its programs in /usr/bin,
#                  and the menu entries, including the "Wine Windows Program Loader" that
#                  opens .exe and .msi files. Depends on the two below.
#   wine-fex-i386  Wine's 32-bit Windows side, for 32-bit programs (many installers are).
#   wine-fex-mono  The wine-mono installer, so creating a Wine prefix needs no download.
#   vstbridge      The plugin libraries, the plugin host, vstbridgectl and its GUI, installed
#                  system-wide. Depends on wine-fex.
#
# The Pivuan apt repository lives on a GitHub branch, which refuses files over 100 MiB (the
# publish script's limit is 95 MiB). That is why Wine is split up, and why its PE files are
# stripped of their debug info (2.4 GB installed before, 1 GB after).
#
# Needs: dpkg-dev, file, curl, xz-utils, binutils, llvm-mingw (fetched into $TOOLCHAIN_DIR, by
# default a temporary directory), and the libraries Wine was built against (to find the
# packages of the libraries it loads at runtime).

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
. "$here/versions.sh"
. "$here/lib.sh"

out="$(realpath -m "${1:?usage: build-debs.sh <out dir> <wine .tar.xz> <fex .tar.xz> <vstbridge .tar.gz> <vstbridge version>}")"
wine_tarball="$(realpath "$2")"
fex_tarball="$(realpath "$3")"
vstbridge_tarball="$(realpath "$4")"
vstbridge_upstream_version="$5"

# Package versions. These live here rather than in versions.sh, whose hash is the Wine build's
# cache key. Bump a revision for a packaging-only change.
FEX_VERSION=2609.137  # FEX_COMMIT in versions.sh is FEX-2609-137-g0df84d384
# MONO_VERSION and MONO_SHA (arm64) in Wine's dlls/appwiz.cpl/addons.c
WINE_MONO_VERSION=11.3.0
WINE_MONO_SHA256_ARM64=b0c47d374efaf22cbff76cb10488ac2d1627b067e085aef7acec573884120e0f
WINE_FEX_REVISION=1
VSTBRIDGE_REVISION=1

die() {
    echo "::error::$*" >&2
    exit 1
}

arch="$(dpkg --print-architecture)"
multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
wine_fex_version="${WINE_VERSION}+fex${FEX_VERSION}-pivuan${WINE_FEX_REVISION}"
mono_version="${WINE_MONO_VERSION}-pivuan${WINE_FEX_REVISION}"
vstbridge_version="${vstbridge_upstream_version}-pivuan${VSTBRIDGE_REVISION}"
maintainer="Pivuan <https://github.com/rations/pivuan>"
homepage="https://github.com/rations/vstbridge"
# The Pivuan repository's publish script (rations/build .github/pivuan/publish-apt.sh) skips
# larger packages, to stay under GitHub's 100 MiB file limit
max_size=$((95 * 1024 * 1024))

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$out"

fetch_llvm_mingw "${TOOLCHAIN_DIR:-$work/toolchain}"

# ─── Wine and FEX ──────────────────────────────────────────────────────────────

tree="$work/wine-tree"
mkdir -p "$tree"
tar -C "$tree" -xJf "$wine_tarball"
tar -C "$tree" -xJf "$fex_tarball"
wine="$tree$WINE_PREFIX"
[ -x "$wine/bin/wine" ] || die "$wine_tarball has no $WINE_PREFIX/bin/wine"
[ -f "$wine/lib/wine/aarch64-windows/libarm64ecfex.dll" ] || die "$fex_tarball has no libarm64ecfex.dll"

# Only needed to build Winelib programs
rm -rf "$wine/include"
find "$wine" -name '*.a' -delete
for tool in function_grep.pl widl winebuild winecpp wineg++ winegcc winemaker wmc wrc; do
    rm -f "$wine/bin/$tool"
done

# Wine's PE modules are built with -g. Stripping keeps the "Wine builtin DLL" marker in the DOS
# header that Wine recognizes its own modules by. Every new prefix gets a copy of these, so
# this also shrinks each ~/.wine.
find "$wine/lib/wine" -path '*-windows/*' -type f -print0 | xargs -0 llvm-strip --strip-debug

# ─── Packaging helpers ────────────────────────────────────────────────────────

# The ELF files in a staged tree
elf_files() {
    find "$1" -type f -exec sh -c 'file -b "$1" | grep -q "^ELF" && echo "$1"' _ {} \;
}

# shlibs_depends <stage> <private library dir>...: the Depends dpkg-shlibdeps finds for the ELF
# files in <stage>. Libraries in the private directories belong to the package itself.
shlibs_depends() {
    local stage="$1" dir flags=()
    shift
    for dir in "$@"; do flags+=("-l$dir"); done
    mapfile -t elfs < <(elf_files "$stage")
    ((${#elfs[@]})) || return 0
    mkdir -p "$work/shlibs/debian"
    printf 'Source: shlibs\n\nPackage: shlibs\nArchitecture: any\n' > "$work/shlibs/debian/control"
    (cd "$work/shlibs" && dpkg-shlibdeps -O --ignore-missing-info "${flags[@]}" "${elfs[@]/#/-e}" 2> "$work/shlibdeps.log") \
        | sed -n 's/^shlibs:Depends=//p' || { cat "$work/shlibdeps.log" >&2; die "dpkg-shlibdeps failed for $stage"; }
}

# build_deb <stage> <package> <version> <architecture> <depends> <summary> <description>
# Optional, from the environment: RECOMMENDS, CONFLICTS, SECTION, TRIGGERS.
build_deb() {
    local stage="$1" pkg="$2" version="$3" deb_arch="$4" depends="$5" summary="$6" description="$7"
    local elf deb

    while IFS= read -r elf; do
        file -b "$elf" | grep -q "ARM aarch64" || die "$pkg: ${elf#"$stage"} is not an aarch64 binary: $(file -b "$elf")"
        strip --strip-unneeded "$elf"
    done < <(elf_files "$stage")

    mkdir -p "$stage/DEBIAN"
    [ -z "${TRIGGERS:-}" ] || printf '%s\n' "$TRIGGERS" > "$stage/DEBIAN/triggers"
    (cd "$stage" && find . -path ./DEBIAN -prune -o -type f -printf '%P\0' | xargs -0 -r md5sum > DEBIAN/md5sums)
    {
        echo "Package: $pkg"
        echo "Version: $version"
        echo "Architecture: $deb_arch"
        echo "Maintainer: $maintainer"
        echo "Installed-Size: $(du -sk --exclude=DEBIAN "$stage" | cut -f1)"
        [ -z "$depends" ] || echo "Depends: $depends"
        [ -z "${RECOMMENDS:-}" ] || echo "Recommends: $RECOMMENDS"
        [ -z "${CONFLICTS:-}" ] || echo "Conflicts: $CONFLICTS"
        echo "Section: ${SECTION:-otherosfs}"
        echo "Priority: optional"
        echo "Homepage: $homepage"
        echo "Description: $summary"
        echo " $description"
    } > "$stage/DEBIAN/control"

    deb="$out/${pkg}_${version}_${deb_arch}.deb"
    dpkg-deb --root-owner-group -Zxz -z9 --build "$stage" "$deb" > /dev/null
    [ "$(stat -c %s "$deb")" -le "$max_size" ] || die "$(basename "$deb") is $(du -h "$deb" | cut -f1), over the Pivuan repository's 95 MiB limit"
    echo "== $(basename "$deb") ($(du -h "$deb" | cut -f1))"
    dpkg-deb -f "$deb" Depends
}

# copyright <stage> <package> <text>
copyright() {
    install -d "$1/usr/share/doc/$2"
    printf '%s\n' "$3" > "$1/usr/share/doc/$2/copyright"
}

wine_copyright="Wine $WINE_VERSION with the wine-staging $WINE_STAGING_TAG patches and vstbridge's patches
(https://github.com/rations/vstbridge, aarch64/wine/patches), built for ARM64EC.
Wine is licensed under the GNU Lesser General Public License, version 2.1 or later; see
/usr/share/common-licenses/LGPL-2.1 and https://gitlab.winehq.org/wine/wine."

# ─── wine-fex-i386 ────────────────────────────────────────────────────────────

stage="$work/wine-fex-i386"
mkdir -p "$stage$WINE_PREFIX/lib/wine"
mv "$wine/lib/wine/i386-windows" "$stage$WINE_PREFIX/lib/wine/"
copyright "$stage" wine-fex-i386 "$wine_copyright"
SECTION=otherosfs build_deb "$stage" wine-fex-i386 "$wine_fex_version" "$arch" "" \
    "Wine for Pivuan: 32-bit Windows programs" \
    "The 32-bit (i386) Windows side of wine-fex. FEX runs these programs' x86 code."

# ─── wine-fex-mono ────────────────────────────────────────────────────────────

stage="$work/wine-fex-mono"
mono_msi="wine-mono-${WINE_MONO_VERSION}-arm64.msi"
# Wine looks for it in <datadir>/wine/mono (dlls/appwiz.cpl/addons.c, install_from_default_dir)
mkdir -p "$stage$WINE_PREFIX/share/wine/mono"
curl -fsSL -o "$stage$WINE_PREFIX/share/wine/mono/$mono_msi" \
    "https://dl.winehq.org/wine/wine-mono/${WINE_MONO_VERSION}/$mono_msi"
echo "$WINE_MONO_SHA256_ARM64  $stage$WINE_PREFIX/share/wine/mono/$mono_msi" | sha256sum -c -
copyright "$stage" wine-fex-mono "wine-mono $WINE_MONO_VERSION (https://gitlab.winehq.org/mono/wine-mono), an open source
.NET Framework implementation for Wine, as published by WineHQ. Mono and wine-mono are
licensed under the MIT license and other free licenses listed in the installer."
SECTION=otherosfs build_deb "$stage" wine-fex-mono "$mono_version" all "" \
    "Wine for Pivuan: .NET Framework (wine-mono)" \
    "The wine-mono installer for wine-fex. Wine installs it into each new prefix from here
 instead of downloading it."

# ─── wine-fex ─────────────────────────────────────────────────────────────────

stage="$work/wine-fex"
mv "$tree" "$stage"

# `wine` and the programs people start by name. Shortcuts that Wine creates for installed
# programs run `wine` from the PATH (programs/winemenubuilder), and vstbridgectl uses winedump.
mkdir -p "$stage/usr/bin"
for program in wine wineserver wineboot winecfg wineconsole winedbg winefile winepath \
               msiexec notepad regedit regsvr32 winedump; do
    [ -e "$stage$WINE_PREFIX/bin/$program" ] || die "$WINE_PREFIX/bin/$program is missing"
    ln -s "$WINE_PREFIX/bin/$program" "$stage/usr/bin/$program"
done

# Wine's logo (the one its Android driver uses as the launcher icon)
install -d "$stage/usr/share/icons/hicolor/scalable/apps"
curl -fsSL -o "$stage/usr/share/icons/hicolor/scalable/apps/wine.svg" \
    "https://gitlab.winehq.org/wine/wine/-/raw/wine-${WINE_VERSION}/dlls/wineandroid.drv/wine.svg"
grep -q '<svg' "$stage/usr/share/icons/hicolor/scalable/apps/wine.svg" || die "could not download Wine's icon"

# The program loader is what file managers offer for Windows programs ("Open With"). Wine's own
# entry lists application/x-ms-dos-executable, which shared-mime-info now only has as an alias
# of application/x-msdownload, the type .exe files get.
install -d "$stage/usr/share/applications"
cat > "$stage/usr/share/applications/wine.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Wine Windows Program Loader
Comment=Run Windows programs
Exec=wine start /unix %f
MimeType=application/x-msdownload;application/x-ms-dos-executable;application/x-msi;application/x-ms-shortcut;application/x-bat;application/x-mswinurl;
Icon=wine
NoDisplay=true
StartupNotify=true
EOF
cat > "$stage/usr/share/applications/wine-winecfg.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Wine Configuration
Comment=Change Wine's settings for Windows programs
Exec=winecfg
Icon=wine
Terminal=false
Categories=Settings;
EOF
cat > "$stage/usr/share/applications/wine-uninstaller.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Uninstall Windows Programs
Comment=Remove programs installed with Wine
Exec=wine uninstaller
Icon=wine
Terminal=false
Categories=Settings;
EOF

copyright "$stage" wine-fex "$wine_copyright

FEX ($FEX_COMMIT, https://github.com/FEX-Emu/FEX) runs the x86 code of Windows programs. It is
licensed under the MIT license:

$(curl -fsSL "https://raw.githubusercontent.com/FEX-Emu/FEX/$FEX_COMMIT/LICENSE")"

# Wine loads most of its libraries with dlopen(), so dpkg-shlibdeps can't see them. Their
# sonames are strings in Wine's Unix libraries; each one found on this system (Wine was built
# against it) becomes a dependency.
dlopen_depends=()
missing=()
while IFS= read -r soname; do
    if pkg="$(dpkg -S "*/$soname" 2> /dev/null | head -n1 | cut -d: -f1)" && [ -n "$pkg" ]; then
        dlopen_depends+=("$pkg")
    else
        missing+=("$soname")
    fi
done < <(find "$stage$WINE_PREFIX/lib/wine/aarch64-unix" "$stage$WINE_PREFIX/bin" -type f \
             -exec grep -ahoE 'lib[A-Za-z0-9_+.-]*\.so\.[0-9]+' {} + | sort -u)
((${#missing[@]} == 0)) || echo "Not on this system, so not dependencies: ${missing[*]}"

depends="$(shlibs_depends "$stage" "$stage$WINE_PREFIX/lib/wine/aarch64-unix")"
((${#dlopen_depends[@]})) || die "found none of the libraries Wine loads at runtime"
depends="$depends, $(printf '%s\n' "${dlopen_depends[@]}" | sort -u | paste -sd, | sed 's/,/, /g')"
depends="$depends, wine-fex-i386 (= $wine_fex_version), wine-fex-mono (= $mono_version)"
RECOMMENDS="libgl1-mesa-dri" \
CONFLICTS="wine, wine64, wine32, winehq-stable, winehq-staging, winehq-devel" \
SECTION=otherosfs build_deb "$stage" wine-fex "$wine_fex_version" "$arch" "$depends" \
    "Wine for Pivuan, with FEX for x86 Windows programs" \
    "Wine $WINE_VERSION (staging) built for ARM64EC: Wine runs natively, and FEX runs only the
 x86 code of the Windows programs and plugins. Includes vstbridge's fix for plugin editors
 that stop responding with Wine's Direct3D. Installed in $WINE_PREFIX, with wine and its
 programs in /usr/bin and a program loader for .exe and .msi files."

# ─── vstbridge ────────────────────────────────────────────────────────────────

stage="$work/vstbridge"
mkdir -p "$work/vstbridge-tarball"
tar -C "$work/vstbridge-tarball" -xzf "$vstbridge_tarball"
from="$work/vstbridge-tarball/vstbridge"

# The plugin libraries and the host in the multiarch library directory. The chainloaders,
# the plugin and vstbridgectl all look for the host next to the plugin libraries.
libdir="$stage/usr/lib/$multiarch"
install -d "$libdir" "$stage/usr/bin"
for f in libvstbridge-vst2.so libvstbridge-vst3.so libvstbridge-clap.so \
         libvstbridge-chainloader-vst2.so libvstbridge-chainloader-vst3.so libvstbridge-chainloader-clap.so \
         vstbridge-host-unixlib.so; do
    install -m 0644 "$from/$f" "$libdir/$f"
done
install -m 0755 "$from/vstbridge-host.exe" "$libdir/vstbridge-host.exe"
install -m 0755 "$from/vstbridgectl" "$from/vstbridgectl-gtk" "$stage/usr/bin/"
install -Dm 0644 "$here/../vstbridge/tools/vstbridgectl/vstbridgectl-gtk.desktop" \
    "$stage/usr/share/applications/vstbridgectl-gtk.desktop"
copyright "$stage" vstbridge "vstbridge (https://github.com/rations/vstbridge), based on yabridge by Robbert van der Helm.

$(cat "$here/../vstbridge/COPYING")"

depends="$(shlibs_depends "$stage"), wine-fex (>= ${WINE_VERSION})"
TRIGGERS="activate-noawait ldconfig" \
SECTION=sound build_deb "$stage" vstbridge "$vstbridge_version" "$arch" "$depends" \
    "Windows VST2, VST3 and CLAP plugins in Linux audio programs" \
    "Bridges Windows plugins into native Linux plugin hosts such as JackDAW, through wine-fex.
 vstbridgectl (and its GUI, in the menu) sets up the plugins found in a Wine prefix."
