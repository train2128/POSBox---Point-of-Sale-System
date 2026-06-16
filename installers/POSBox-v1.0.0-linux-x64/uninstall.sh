#!/bin/bash
set -e
if [ "$EUID" -ne 0 ]; then
    echo "Error: Run with sudo ./uninstall.sh"
    exit 1
fi
echo "Removing POSBox..."
rm -rf /opt/POSBox
rm -f /usr/local/bin/posbox
rm -f /usr/share/applications/posbox*.desktop
rm -f /usr/share/pixmaps/*icon_colored.png
command -v update-desktop-database &>/dev/null && update-desktop-database /usr/share/applications/ 2>/dev/null || true
echo "POSBox uninstalled."
