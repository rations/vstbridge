#!/bin/sh
# Build the release binary tarball.
#
# WHAT THIS SHIPS. The native plugin libraries and chainloaders, the 64-bit and 32-bit Wine
# plugin hosts, vstbridgectl and vstbridgectl-gtk, the installer and uninstaller, and the
# README, CHANGELOG and LICENSE -- the installed surface and nothing else. No sources, no build
# system, no tests.
#
# HOW IT IS INSTALLED. The archive is named after the version and architecture, but it always
# unpacks into a flat vstbridge/ directory, and installs per user, without root:
#
#     tar -xf vstbridge-<version>-x86_64.tar.gz
#     cd vstbridge
#     ./install.sh
#
# install.sh copies the files to ~/.local/share/vstbridge, where vstbridgectl finds them, and
# writes a desktop entry pointing there. It is the same file that lives next to this script, so
# the installer that ships is the installer that is tested below. The directory is called
# vstbridge so that extracting the archive straight into ~/.local/share is an install too; that
# is checked below as well.
#
# x86_64 ONLY. This is the Winelib build. aarch64 can't build Winelib executables and is
# released as Debian packages instead; see ../aarch64/build-debs.sh.
#
# THE VERSION IS THE GIT TAG. meson.build still carries yabridge's version, so it can't be the
# source. On a tagged commit (v0.0.3) the archive is vstbridge-0.0.3-x86_64; anywhere else it is
# a test build named after `git describe`, e.g. vstbridge-0.0.2-16-gc41c2b1-x86_64, so a tester's
# copy always says exactly which commit it came from. For the same reason the tracked tree has to
# be clean: a version that names a commit is a lie if the binaries were built from edits on top.
#
# WHAT A BINARY TARBALL COSTS, stated rather than discovered. Everything is dynamically linked
# against the glibc of the machine that built it, so a recipient on an older distribution gets a
# loader error rather than a plugin. The gates below MEASURE the minimum glibc and the exact set
# of shared libraries and print them. Published releases are built on Devuan Daedalus for that
# reason; pass --max-glibc there to make a higher floor a failure instead of a number. A build on
# a newer machine is fine for testing.
#
# The archive is reproducible as far as this script controls it: member order, owners, modes,
# mtimes (the HEAD commit time) and the gzip header are normalised, and source paths are mapped
# out of the binaries. Whether the toolchain produces identical binaries is not claimed.
#
# This script is POSIX sh but uses GNU tar, findutils, coreutils and binutils. Portability is a
# property the ARCHIVE needs; the script only ever runs on the machine cutting the release.

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$root/.." && pwd)
outdir=$root/dist
verify=1
max_glibc=

usage() {
    cat <<USAGE
usage: make-release.sh [--out DIR] [--max-glibc VERSION] [--no-verify]

  --out DIR            where to write the tarball (default: dist/)
  --max-glibc VERSION  fail if the binaries need a newer glibc than this,
                       e.g. 2.34 when cutting a published release
  --no-verify          skip the unpack-and-install check (not recommended)

Builds vstbridge and vstbridgectl from scratch, out of tree, and packages them
as vstbridge-<version>-x86_64.tar.gz with a .sha256 next to it. The version
comes from 'git describe', so an untagged commit gives a test build.
USAGE
}

while [ $# -gt 0 ]; do
    case $1 in
        --out) outdir=$2; shift 2 ;;
        --out=*) outdir=${1#--out=}; shift ;;
        --max-glibc) max_glibc=$2; shift 2 ;;
        --max-glibc=*) max_glibc=${1#--max-glibc=}; shift ;;
        --no-verify) verify=0; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'make-release.sh: unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

die() { printf 'make-release.sh: %s\n' "$*" >&2; exit 1; }
say() { printf '  %s\n' "$*"; }

# Sorts dotted versions numerically and prints the highest
maxver() { sort -t. -k1,1n -k2,2n -k3,3n | tail -1; }

# ---------------------------------------------------------------------------------------------
# Version and architecture.
# ---------------------------------------------------------------------------------------------
arch=$(uname -m)
[ "$arch" = x86_64 ] ||
    die "this builds the x86_64 Winelib release; on $arch use ../aarch64/build-debs.sh"

cd "$repo"
git rev-parse --git-dir >/dev/null 2>&1 || die "not a git checkout: $repo"
git diff --quiet HEAD -- ||
    die "the tracked tree has uncommitted changes; commit or stash them first, since the
version names a commit and the binaries have to be built from exactly that commit"
describe=$(git describe --tags --match 'v[0-9]*' 2>/dev/null) ||
    die "no v<version> tag is reachable from HEAD"
version=${describe#v}
name=vstbridge-$version-$arch
# The directory the archive unpacks into, which has to match ~/.local/share/vstbridge
top=vstbridge

# A tagged release has to have its CHANGELOG entry. A test build doesn't.
if git describe --tags --exact-match --match 'v[0-9]*' >/dev/null 2>&1; then
    grep -q "^## \[$version\]" "$root/CHANGELOG.md" ||
        die "v$version is tagged but CHANGELOG.md has no '## [$version]' section"
    kind=release
else
    kind="test build"
fi

printf '\n%s %s (%s)\n\n' "Building release tarball for" "$name" "$kind"

for tool in meson ninja make pkg-config objdump sha256sum; do
    command -v "$tool" >/dev/null 2>&1 || die "$tool is required"
done
tar --version 2>/dev/null | head -1 | grep -q 'GNU tar' ||
    die "GNU tar is required to create the archive (the archive itself is plain ustar)"
pkg-config --exists gtk+-3.0 || die "GTK 3 development files are required for vstbridgectl-gtk"

stage=$(mktemp -d "${TMPDIR:-/tmp}/vstbridge-release.XXXXXX")
trap 'rm -rf "$stage"' EXIT INT TERM

# ---------------------------------------------------------------------------------------------
# Build. Out of tree and from scratch, never the working build/ directories: those point at
# whatever was configured by hand last, and several of them at old checkouts.
#
# The prefix maps strip this machine's source path out of __FILE__. Meson passes sources
# relative to the build directory, so both the absolute and the relative spelling are mapped.
# ---------------------------------------------------------------------------------------------
printf 'Building\n'
build=$stage/build
mkdir "$build"
rel=$(realpath --relative-to="$build" "$root")
prefix_map="-ffile-prefix-map=$root/= -ffile-prefix-map=$rel/="

if ! meson setup "$build" "$root" \
        --buildtype=release \
        --cross-file="$root/cross-wine.conf" \
        -Dbitbridge=true \
        "-Dcpp_args=$prefix_map" \
        "-Dbuild.cpp_args=$prefix_map" >"$stage/meson.log" 2>&1 ||
   ! meson compile -C "$build" >>"$stage/meson.log" 2>&1; then
    cp "$stage/meson.log" "${TMPDIR:-/tmp}/vstbridge-release-meson.log"
    tail -40 "$stage/meson.log" >&2
    die "the vstbridge build failed (full log: ${TMPDIR:-/tmp}/vstbridge-release-meson.log)"
fi
say "vstbridge: clean release build with the 32-bit host"

# CXXFLAGS goes through the environment: the Makefile appends it to its own flags, whereas a
# command-line CXXFLAGS would replace them. The dependencies are the pinned checkouts in deps/,
# fetched on first use.
ctl=$stage/vstbridgectl
if ! CXXFLAGS="-ffile-prefix-map=$root/tools/vstbridgectl/=" \
        make -C "$root/tools/vstbridgectl" -j"$(nproc 2>/dev/null || echo 2)" \
        BUILD_DIR="$ctl" >"$stage/make.log" 2>&1; then
    cp "$stage/make.log" "${TMPDIR:-/tmp}/vstbridge-release-make.log"
    tail -40 "$stage/make.log" >&2
    die "the vstbridgectl build failed (full log: ${TMPDIR:-/tmp}/vstbridge-release-make.log)"
fi
say "vstbridgectl: clean build of the CLI and the GUI"

# ---------------------------------------------------------------------------------------------
# Stage.
# ---------------------------------------------------------------------------------------------
printf 'Staging\n'
out=$stage/$top
mkdir "$out"

libraries='
libvstbridge-vst2.so
libvstbridge-vst3.so
libvstbridge-clap.so
libvstbridge-chainloader-vst2.so
libvstbridge-chainloader-vst3.so
libvstbridge-chainloader-clap.so
vstbridge-host.exe.so
vstbridge-host-32.exe.so
'
# winegcc's launcher scripts for the two hosts
launchers='
vstbridge-host.exe
vstbridge-host-32.exe
'
tools='
vstbridgectl
vstbridgectl-gtk
'
scripts='
install.sh
uninstall.sh
'

for f in $libraries $launchers; do cp "$build/$f" "$out/$f"; done
for f in $tools; do cp "$ctl/$f" "$out/$f"; done
for f in $scripts; do cp "$root/$f" "$out/$f"; done
cp "$repo/README.md" "$repo/LICENSE" "$root/CHANGELOG.md" "$out/"
say "$(find "$out" -type f | wc -l) files"

# ---------------------------------------------------------------------------------------------
# Gates. Each one is a thing that has to be true of the archive, asserted rather than assumed.
# ---------------------------------------------------------------------------------------------
printf 'Checking the staged tree\n'

# 1. The manifest, EXACTLY: a file that appears fails as surely as one that goes missing. It is
#    also the list install.sh and uninstall.sh have to agree with, which is checked below.
printf '%s\n' $libraries $launchers $tools $scripts README.md LICENSE CHANGELOG.md |
    sort > "$stage/expected"
( cd "$out" && find . -type f | sed 's|^\./||' | sort ) > "$stage/actual"
if ! diff -u "$stage/expected" "$stage/actual" > "$stage/manifest.diff"; then
    printf '%s\n' "the staged tree is not the manifest (- expected, + present):" >&2
    sed -n '3,$p' "$stage/manifest.diff" >&2
    die "update the lists in this script"
fi
say "the staged tree is exactly the manifest ($(wc -l < "$stage/actual") files)"

# 2. Nothing from this machine escaped, in the documents or in the binaries. A source path baked
#    into a binary is the mistake that actually happens, and a text-only grep can't see it.
leaks=$(grep -rl --binary-files=text \
             -e 'CLAUDE\.md' -e '/home/' -e '/root/' "$out" 2>/dev/null || true)
if [ -n "$leaks" ]; then
    die "shipped files name a local-only document or an absolute home path:
$(printf '%s\n' "$leaks" | sed "s|^$out/|  |")"
fi
say "no file names a local-only document or a build-machine path"

# 3. Every relative link in the shipped documents points at a shipped file. The README is
#    written for GitHub, where the whole repository is there to link to; in the tarball only
#    these files are, and a dead link is worse than none.
dead=$(cd "$out" && for doc in *.md; do
    grep -o '](\([^)]*\))' "$doc" | sed 's/^](\(.*\))$/\1/' |
        grep -v -e '://' -e '^#' -e '^mailto:' | sed 's/#.*//' |
        while read -r target; do
            [ -e "$target" ] || printf '  %s -> %s\n' "$doc" "$target"
        done
done)
[ -z "$dead" ] || die "shipped documents link to files that are not in the archive:
$dead
Link to them on GitHub instead."
say "every relative link in the shipped documents resolves inside the archive"

# 4. No setuid or setgid bits, and no symlinks.
if find "$out" \( -perm -4000 -o -perm -2000 \) -print | grep -q .; then
    die "a setuid or setgid bit is set in the staged tree"
fi
if find "$out" -type l -print | grep -q .; then
    die "the staged tree contains a symlink"
fi
say "no setuid or setgid bits, no symlinks"

# 5. Every path fits ustar's 100-byte name plus 155-byte prefix, split at a '/'.
too_long=$(cd "$stage" && find "$top" | awk '
    { p = $0; n = length(p)
      if (n <= 100) next
      ok = 0
      for (i = 1; i < n; i++)
          if (substr(p, i, 1) == "/" && i - 1 <= 155 && n - i <= 100) { ok = 1; break }
      if (!ok) print p }')
[ -z "$too_long" ] || die "path does not fit the ustar name/prefix split:
$too_long"
say "every path fits ustar"

# 6. install.sh and uninstall.sh name every file they need to. A library added to the build
#    but not to the installer would ship and then never be installed.
for f in $libraries $launchers $tools; do
    grep -q "$f" "$out/install.sh" || die "install.sh does not install $f"
    grep -q "$f" "$out/uninstall.sh" || die "uninstall.sh does not remove $f"
done
say "install.sh and uninstall.sh cover every shipped binary"

# ---------------------------------------------------------------------------------------------
# 7. What the binaries need from the recipient's machine.
# ---------------------------------------------------------------------------------------------
printf 'Measuring what the binaries need\n'

needed() { objdump -p "$1" | awk '/NEEDED/ { print $2 }'; }
glibc_of() { objdump -p "$1" | sed -n 's/.*GLIBC_\([0-9][0-9.]*\).*/\1/p' | maxver; }

# An allowlist: a new NEEDED entry is a new thing every user has to already have, and that
# should be decided rather than discovered. GTK is only needed by vstbridgectl-gtk, and libxcb
# by the hosts, which Wine users have anyway.
allowed='libc.so.6 libm.so.6 libstdc++.so.6 libgcc_s.so.1
         ld-linux-x86-64.so.2 ld-linux.so.2
         libxcb.so.1
         libgtk-3.so.0 libgobject-2.0.so.0 libglib-2.0.so.0'
glibc=
for f in $libraries $tools; do
    for lib in $(needed "$out/$f"); do
        case " $(echo $allowed) " in
            *" $lib "*) ;;
            *) die "$f links a library that is not on the allowlist: $lib
Either it belongs in the release's requirements -- add it above and to the README -- or it
was linked by accident." ;;
        esac
    done
    glibc=$(printf '%s\n%s\n' "$glibc" "$(glibc_of "$out/$f")" | grep . | maxver)
done
say "plugin libraries link: $(needed "$out/libvstbridge-vst3.so" | grep -v '^ld-linux' | tr '\n' ' ')"
say "vstbridgectl-gtk links: $(needed "$out/vstbridgectl-gtk" | grep -v '^ld-linux' | tr '\n' ' ')"
say "the 32-bit host needs the i386 libc, libstdc++ and libxcb (32-bit plugins only)"

[ -n "$glibc" ] || die "could not read the glibc symbol versions out of the binaries"
if [ -n "$max_glibc" ] &&
   [ "$(printf '%s\n%s\n' "$glibc" "$max_glibc" | maxver)" != "$max_glibc" ]; then
    die "the binaries need glibc >= $glibc, newer than --max-glibc $max_glibc.
Build the release on an older distribution (Devuan Daedalus)."
fi
say "needs glibc >= $glibc on $arch"

# ---------------------------------------------------------------------------------------------
# 8. Normalise modes and timestamps, so the archive is a function of the files in it.
# ---------------------------------------------------------------------------------------------
find "$out" -type d -exec chmod 755 {} +
find "$out" -type f -exec chmod 644 {} +
for f in $libraries $launchers $tools $scripts; do chmod 755 "$out/$f"; done

if [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    epoch=$SOURCE_DATE_EPOCH
else
    epoch=$(git log -1 --format=%ct)
fi
find "$out" -exec touch -d "@$epoch" {} +
say "mtime pinned to $(date -u -d "@$epoch" '+%Y-%m-%d %H:%M:%S UTC')"

# ---------------------------------------------------------------------------------------------
# Archive.
# ---------------------------------------------------------------------------------------------
printf 'Archiving\n'
mkdir -p "$outdir"
outdir=$(CDPATH= cd -- "$outdir" && pwd)
tar -C "$stage" \
    --format=ustar \
    --sort=name \
    --owner=0 --group=0 --numeric-owner \
    --mtime="@$epoch" \
    -cf "$stage/$name.tar" "$top"

# -n leaves the filename and timestamp out of the gzip header, so the .gz is reproducible too
gzip -9nc "$stage/$name.tar" > "$outdir/$name.tar.gz"
( cd "$outdir" && sha256sum "$name.tar.gz" > "$name.sha256" )
say "$outdir/$name.tar.gz ($(du -h "$outdir/$name.tar.gz" | cut -f1))"
say "$outdir/$name.sha256"

install_cmd="tar -xf $name.tar.gz && cd $top && ./install.sh"

if [ "$verify" -eq 0 ]; then
    printf '\nSkipped verification (--no-verify).\n\n'
    printf 'Install with:\n  %s\n\n' "$install_cmd"
    exit 0
fi

# ---------------------------------------------------------------------------------------------
# Verify. Unpack what was actually written and install it the way a user would, into a scratch
# home directory. install.sh is the first thing a tester runs and the only shipped file whose
# bugs land on their machine, so it is run here rather than read.
# ---------------------------------------------------------------------------------------------
printf 'Verifying the written archive\n'
check=$stage/check
mkdir "$check"
tar -C "$check" -xf "$outdir/$name.tar.gz"
[ "$(ls "$check")" = "$top" ] || die "the archive does not unpack into a single $top/ directory"
( cd "$check/$top" && find . -type f | sed 's|^\./||' | sort ) > "$stage/unpacked"
cmp -s "$stage/expected" "$stage/unpacked" ||
    die "the written archive does not hold the manifest"
say "unpacks into $top/ with exactly the manifest"

"$check/$top/vstbridgectl" --help >/dev/null 2>"$stage/help.log" || {
    cat "$stage/help.log" >&2
    die "the shipped vstbridgectl does not run"
}
say "the shipped vstbridgectl runs (--help)"

fake=$stage/home
mkdir "$fake"
data=$fake/.local/share/vstbridge
apps=$fake/.local/share/applications
run_as_tester() {
    env -u XDG_DATA_HOME -u XDG_CONFIG_HOME HOME="$fake" "$@"
}

if ! ( cd "$check/$top" && run_as_tester sh ./install.sh ) >"$stage/install.log" 2>&1; then
    tail -30 "$stage/install.log" >&2
    die "the shipped install.sh failed"
fi
for f in $libraries $launchers $tools; do
    [ -x "$data/$f" ] || die "install.sh did not install $f as an executable"
    cmp -s "$check/$top/$f" "$data/$f" || die "install.sh installed a different $f"
done
grep -qx "Exec=$data/vstbridgectl-gtk" "$apps/vstbridgectl-gtk.desktop" ||
    die "the desktop entry does not point at the installed vstbridgectl-gtk"
say "install.sh installs every binary and a desktop entry that points at it"

# vstbridgectl finds the installed files without being told where they are, which is the whole
# point of installing them to that directory
run_as_tester "$data/vstbridgectl" status >"$stage/status.log" 2>&1 || true
for f in libvstbridge-chainloader-vst2.so libvstbridge-chainloader-vst3.so \
         libvstbridge-chainloader-clap.so vstbridge-host.exe vstbridge-host-32.exe; do
    grep -q "$data/$f" "$stage/status.log" || {
        cat "$stage/status.log" >&2
        die "the installed vstbridgectl does not find $f in $data"
    }
done
say "the installed vstbridgectl finds the plugin libraries and both hosts"

# Both ways of removing it leave nothing behind. mimeinfo.cache is the shared index
# update-desktop-database keeps for all of a user's desktop entries; it isn't ours to remove.
leftovers() { find "$fake/.local" -type f ! -name mimeinfo.cache -print; }
( cd "$check/$top" && run_as_tester sh ./install.sh --uninstall ) >/dev/null 2>&1 ||
    die "install.sh --uninstall failed"
left=$(leftovers)
[ -z "$left" ] || die "install.sh --uninstall left files behind:
$(printf '%s\n' "$left" | sed "s|^$fake|  ~|")"

( cd "$check/$top" && run_as_tester sh ./install.sh ) >/dev/null 2>&1
( cd "$check/$top" && run_as_tester sh ./uninstall.sh ) >/dev/null 2>&1 ||
    die "uninstall.sh failed"
left=$(leftovers)
[ -z "$left" ] || die "uninstall.sh left files behind:
$(printf '%s\n' "$left" | sed "s|^$fake|  ~|")"
[ -d "$data" ] && die "uninstall.sh left $data behind"
say "install.sh --uninstall and uninstall.sh both remove every file"

# Extracting the archive straight into ~/.local/share also has to work, which is why it
# unpacks into vstbridge/
tar -C "$fake/.local/share" -xf "$outdir/$name.tar.gz"
run_as_tester "$data/vstbridgectl" status >"$stage/status-extracted.log" 2>&1 || true
grep -q "$data/libvstbridge-chainloader-vst3.so" "$stage/status-extracted.log" || {
    cat "$stage/status-extracted.log" >&2
    die "vstbridgectl does not find the files when the archive is extracted into ~/.local/share"
}
say "extracting the archive into ~/.local/share is also a working install"

printf '\n%s\n\n' "vstbridge $version ($arch, $kind) is built and verified in $outdir."
printf 'Needs glibc >= %s.\n\n' "$glibc"
printf 'Install with:\n  %s\n\n' "$install_cmd"
