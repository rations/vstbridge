#!/bin/bash
#
# Build wine-staging for aarch64 with ARM64EC support, plus vstbridge's Wine patches
# (aarch64/wine/patches). Run natively on aarch64 Debian trixie; the CI workflow
# .github/workflows/aarch64-wine.yml runs it on GitHub's arm64 runners.
#
# ARM64EC lets x86_64 plugin DLLs run under FEX while Wine itself stays native aarch64.
# i386 is included for 32-bit plugins under WoW64 (libwow64fex).
#
# Usage: aarch64/build-wine.sh <work dir> <output .tar.xz>
# The tarball contains the installed tree under $WINE_PREFIX.

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
. "$here/versions.sh"
. "$here/lib.sh"

work="$(realpath -m "$1")"
out="$(realpath -m "$2")"
mkdir -p "$work"

fetch_llvm_mingw "$work/toolchain"

rm -rf "$work/wine" "$work/wine-staging"
git clone --depth 1 --branch "wine-$WINE_VERSION" https://gitlab.winehq.org/wine/wine.git "$work/wine"
git clone --depth 1 --branch "$WINE_STAGING_TAG" https://gitlab.winehq.org/wine/wine-staging.git "$work/wine-staging"

staging_base="$(cat "$work/wine-staging/staging/upstream-commit")"
wine_head="$(git -C "$work/wine" rev-parse HEAD)"
if [ "$staging_base" != "$wine_head" ]; then
    echo "wine-staging $WINE_STAGING_TAG is based on $staging_base, but wine-$WINE_VERSION is $wine_head" >&2
    exit 1
fi

cd "$work/wine"
# Also runs autoreconf and tools/make_requests afterwards
python3 "$work/wine-staging/staging/patchinstall.py" --all
for patch in "$here"/wine/patches/*.patch; do
    echo "Applying $(basename "$patch")"
    patch -p1 --forward < "$patch"
done

# The explicit --with-* options turn a missing dependency into a configure error instead
# of silently building Wine without it
rm -rf "$work/build"
mkdir -p "$work/build"
cd "$work/build"
../wine/configure \
    --prefix="$WINE_PREFIX" \
    --with-mingw=clang \
    --enable-archs=arm64ec,aarch64,i386 \
    --disable-tests \
    --with-x --with-xcomposite --with-xcursor --with-xfixes --with-xinerama --with-xinput2 \
    --with-xrandr --with-xrender --with-xshape --with-xshm --with-xxf86vm \
    --with-opengl --with-vulkan \
    --with-freetype --with-fontconfig --with-gnutls \
    --with-alsa --with-dbus --with-udev \
    --without-wayland
make -j"$(nproc)"

rm -rf "$work/stage"
make install DESTDIR="$work/stage"
tar -C "$work/stage" --owner=0 --group=0 -cJf "$out" .
echo "Wrote $out"
