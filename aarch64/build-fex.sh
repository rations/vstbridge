#!/bin/bash
#
# Build FEX's Wine emulator DLLs for aarch64: libarm64ecfex.dll (x86_64 code in ARM64EC
# processes) and libwow64fex.dll (i386 code under WoW64), plus their aarch64 unixlibs.
# The CMake options are the ones FEX's own CI uses (.github/workflows/wine_build/action.yml).
#
# Usage: aarch64/build-fex.sh <work dir> <output .tar.xz>
# The tarball installs over the Wine tree from build-wine.sh: DLLs go in
# $WINE_PREFIX/lib/wine/aarch64-windows, unixlibs in $WINE_PREFIX/lib/wine/aarch64-unix.

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
. "$here/versions.sh"
. "$here/lib.sh"

work="$(realpath -m "$1")"
out="$(realpath -m "$2")"
mkdir -p "$work"

fetch_llvm_mingw "$work/toolchain"

rm -rf "$work/FEX"
git init -q "$work/FEX"
git -C "$work/FEX" remote add origin https://github.com/FEX-Emu/FEX.git
git -C "$work/FEX" fetch -q --depth 1 origin "$FEX_COMMIT"
git -C "$work/FEX" checkout -q FETCH_HEAD
# Only the submodules the Windows build needs, not the large test binary repos
git -C "$work/FEX" submodule update --init --depth 1 \
    External/fmt External/range-v3 External/rpmalloc External/tracy External/unordered_dense \
    External/vixl External/xxhash External/zydis External/drm-headers External/Vulkan-Headers \
    Source/Common/cpp-optparse

rm -rf "$work/stage"
windows_dir="$work/stage$WINE_PREFIX/lib/wine/aarch64-windows"
unix_dir="$work/stage$WINE_PREFIX/lib/wine/aarch64-unix"
mkdir -p "$windows_dir" "$unix_dir"

for target in arm64ec wow64; do
    case "$target" in
        arm64ec) triple=arm64ec-w64-mingw32 ;;
        wow64) triple=aarch64-w64-mingw32 ;;
    esac

    rm -rf "$work/build_$target"
    cmake -S "$work/FEX" -B "$work/build_$target" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$work/FEX/Data/CMake/toolchain_mingw.cmake" \
        -DMINGW_TRIPLE="$triple" \
        -DENABLE_LTO=False -DENABLE_ASSERTIONS=False -DENABLE_JEMALLOC_GLIBC_ALLOC=False \
        -DBUILD_TESTING=False -DTUNE_ARCH=generic -DTUNE_CPU=none -DRANGES_NATIVE=OFF
    cmake --build "$work/build_$target" -t "${target}fex"

    cp "$work/build_$target/Bin/lib${target}fex.dll" "$windows_dir/"
    "$triple-strip" --strip-unneeded "$windows_dir/lib${target}fex.dll"
done

# The unixlibs are native aarch64 libraries, built with the system compiler
rm -rf "$work/build_unixlib"
cmake -S "$work/FEX/Source/Windows/UnixLib" -B "$work/build_unixlib" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$work/build_unixlib"
cp "$work/build_unixlib/libarm64ecfex.so" "$work/build_unixlib/libwow64fex.so" "$unix_dir/"
strip --strip-unneeded "$unix_dir"/*.so

tar -C "$work/stage" --owner=0 --group=0 -cJf "$out" .
echo "Wrote $out"
