#!/bin/sh
# vstbridge installer — installs pre-built binaries from a release tarball.
#
# Usage:
#   ./install.sh              # user install (~/.local/share/vstbridge, ~/.local/share/applications)
#   ./install.sh --uninstall  # remove installed files

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ── Parse arguments ──────────────────────────────────────────────────────────

UNINSTALL=0

for arg in "$@"; do
    case "$arg" in
        --uninstall) UNINSTALL=1 ;;
        --help|-h)
            echo "Usage: $0 [--uninstall]"
            echo "  (no flags)   Install for the current user"
            echo "  --uninstall  Remove installed files"
            exit 0
            ;;
    esac
done

DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/vstbridge"
APPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"

# ── Uninstall ────────────────────────────────────────────────────────────────

if [ "$UNINSTALL" -eq 1 ]; then
    echo "Removing bridge and tool files from $DATA_DIR"
    rm -f \
        "$DATA_DIR"/libvstbridge-vst2.so \
        "$DATA_DIR"/libvstbridge-vst3.so \
        "$DATA_DIR"/libvstbridge-clap.so \
        "$DATA_DIR"/libvstbridge-chainloader-vst2.so \
        "$DATA_DIR"/libvstbridge-chainloader-vst3.so \
        "$DATA_DIR"/libvstbridge-chainloader-clap.so \
        "$DATA_DIR"/vstbridge-host.exe \
        "$DATA_DIR"/vstbridge-host.exe.so \
        "$DATA_DIR"/vstbridge-host-32.exe \
        "$DATA_DIR"/vstbridge-host-32.exe.so \
        "$DATA_DIR"/vstbridgectl \
        "$DATA_DIR"/vstbridgectl-gtk

    echo "Removing $APPS_DIR/vstbridgectl-gtk.desktop"
    rm -f "$APPS_DIR/vstbridgectl-gtk.desktop"

    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$APPS_DIR" 2>/dev/null || true
    fi
    echo "Done."
    exit 0
fi

# ── Verify we are running from the extracted tarball ─────────────────────────

for f in \
    libvstbridge-vst2.so \
    libvstbridge-chainloader-vst2.so \
    vstbridge-host.exe \
    vstbridgectl \
    vstbridgectl-gtk
do
    if [ ! -f "$SCRIPT_DIR/$f" ]; then
        echo "Error: $f not found in $SCRIPT_DIR" >&2
        echo "Run this script from the extracted vstbridge release directory." >&2
        exit 1
    fi
done

# ── Install all files to ~/.local/share/vstbridge/ ───────────────────────────

mkdir -p "$DATA_DIR"

for f in \
    libvstbridge-vst2.so \
    libvstbridge-vst3.so \
    libvstbridge-clap.so \
    libvstbridge-chainloader-vst2.so \
    libvstbridge-chainloader-vst3.so \
    libvstbridge-chainloader-clap.so \
    vstbridge-host.exe \
    vstbridge-host.exe.so \
    vstbridge-host-32.exe \
    vstbridge-host-32.exe.so \
    vstbridgectl \
    vstbridgectl-gtk
do
    if [ -f "$SCRIPT_DIR/$f" ]; then
        cp "$SCRIPT_DIR/$f" "$DATA_DIR/$f"
        chmod 755 "$DATA_DIR/$f"
    fi
done

echo "Installed files to: $DATA_DIR"

# ── Install desktop entry (with full path to binary) ─────────────────────────

mkdir -p "$APPS_DIR"
cat > "$APPS_DIR/vstbridgectl-gtk.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=vstbridgectl
GenericName=Wine Plugin Manager
Comment=Manage vstbridge Wine plugin installations
Exec=$DATA_DIR/vstbridgectl-gtk
Icon=preferences-system
Categories=AudioVideo;Audio;Multimedia;
Terminal=false
Keywords=vst;vst3;clap;wine;vstbridge;plugin;audio;
EOF

echo "Installed desktop entry: $APPS_DIR/vstbridgectl-gtk.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPS_DIR" 2>/dev/null || true
fi

# ── Done ──────────────────────────────────────────────────────────────────────

echo ""
echo "vstbridge installed successfully."
echo ""
echo "Next steps:"
echo "  1. Tell vstbridgectl where vstbridge is installed:"
echo "     $DATA_DIR/vstbridgectl set --path=$DATA_DIR"
echo "  2. Add your Windows plugin directories, for example:"
echo "     $DATA_DIR/vstbridgectl add ~/.wine/drive_c/Program\ Files/VstPlugins"
echo "  3. Sync plugins:"
echo "     $DATA_DIR/vstbridgectl sync"
echo "  Or launch the GUI from your application menu, or run:"
echo "     $DATA_DIR/vstbridgectl-gtk"
