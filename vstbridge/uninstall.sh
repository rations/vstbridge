#!/bin/sh
# vstbridge uninstaller — removes files installed by install.sh

set -e

DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/vstbridge"
APPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"

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

if [ -d "$DATA_DIR" ] && [ -z "$(ls -A "$DATA_DIR" 2>/dev/null)" ]; then
    rmdir "$DATA_DIR"
fi

echo "Removing $APPS_DIR/vstbridgectl-gtk.desktop"
rm -f "$APPS_DIR/vstbridgectl-gtk.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPS_DIR" 2>/dev/null || true
fi

echo "Done. vstbridge has been removed."
echo ""
echo "Note: your plugin directories config (~/.config/vstbridgectl/config.toml)"
echo "and any per-plugin vstbridge.toml files have not been removed."
