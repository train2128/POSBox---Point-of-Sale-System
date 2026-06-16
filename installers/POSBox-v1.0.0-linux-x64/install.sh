#!/bin/bash
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"

if [ "$EUID" -ne 0 ]; then
    echo "Error: Run with sudo ./install.sh"
    exit 1
fi

echo "Installing POSBox to /opt/POSBox..."
rm -rf /opt/POSBox
mkdir -p /opt/POSBox
cp -r "$DIR/bin" "$DIR/lib" "$DIR/plugins" "$DIR/translations" "$DIR/icons" /opt/POSBox/
chmod +x /opt/POSBox/bin/*

echo "Creating desktop shortcuts..."
cp "$DIR/icons/"*.png /usr/share/pixmaps/ 2>/dev/null || true
cp "$DIR/desktop/"*.desktop /usr/share/applications/ 2>/dev/null || true
chmod 644 /usr/share/applications/posbox*.desktop 2>/dev/null || true

ln -sf /opt/POSBox/bin/posbox /usr/local/bin/posbox

command -v update-desktop-database &>/dev/null && update-desktop-database /usr/share/applications/ 2>/dev/null || true
command -v gtk-update-icon-cache &>/dev/null && gtk-update-icon-cache -f -t /usr/share/pixmaps/ 2>/dev/null || true

echo ""
echo "POSBox installed successfully!"
echo "  Run: posbox"
echo "  Or find it in your application menu"
echo "  Uninstall: sudo /opt/POSBox/uninstall.sh"
