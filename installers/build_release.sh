#!/bin/bash
# ============================================================================
# POSBox Release Builder
# Builds all apps in release mode, bundles Qt libs, creates installable tarball
# ============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build/Desktop_Qt_6_9_3-Release"
QT_DIR="/home/train/Qt/6.9.3/gcc_64"
VERSION="${1:-1.0.0}"

ARCHIVE_NAME="POSBox-v${VERSION}-linux-x64"
STAGING="$SCRIPT_DIR/staging/$ARCHIVE_NAME"

echo "============================================"
echo "  POSBox Release Builder v${VERSION}"
echo "============================================"

# Step 1: Build
echo ""
echo "[1/6] Building release..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
"$QT_DIR/bin/qmake" "$PROJECT_DIR/POSBox.pro" -spec linux-g++ CONFIG+=release
make -j$(nproc)

# Step 2: Create staging
echo ""
echo "[2/6] Creating bundle..."
rm -rf "$STAGING"
mkdir -p "$STAGING"/{bin,lib,plugins/{platforms,sqldrivers,printsupport,imageformats},translations,icons}

# Step 3: Binaries
echo "[3/6] Copying binaries..."
for app in Register Settings Inventory Tickets Financial; do
    cp "$BUILD_DIR/$app/$app" "$STAGING/bin/"
    strip "$STAGING/bin/$app"
done
cp "$BUILD_DIR/lib/libCore.so.1.0.0" "$STAGING/lib/"
strip "$STAGING/lib/libCore.so.1.0.0"
ln -sf libCore.so.1.0.0 "$STAGING/lib/libCore.so.1.0"
ln -sf libCore.so.1.0.0 "$STAGING/lib/libCore.so.1"
ln -sf libCore.so.1.0.0 "$STAGING/lib/libCore.so"
echo "  5 apps + libCore"

# Step 4: Qt libs
echo "[4/6] Bundling Qt libraries..."
QT_LIBS=(
    libQt6Core.so.6
    libQt6Gui.so.6
    libQt6Widgets.so.6
    libQt6Sql.so.6
    libQt6SerialPort.so.6
    libQt6Network.so.6
    libQt6DBus.so.6
    libQt6PrintSupport.so.6
    libQt6Svg.so.6
    libQt6XcbQpa.so.6
    libicui18n.so.73
    libicuuc.so.73
    libicudata.so.73
)
for lib in "${QT_LIBS[@]}"; do
    [ -f "$QT_DIR/lib/$lib" ] && cp "$QT_DIR/lib/$lib" "$STAGING/lib/"
done
echo "  ${#QT_LIBS[@]} libraries"

# Step 5: Qt plugins
echo "[5/6] Bundling plugins..."
cp "$QT_DIR/plugins/platforms/libqxcb.so" "$STAGING/plugins/platforms/"
cp "$QT_DIR/plugins/platforms/libqminimal.so" "$STAGING/plugins/platforms/"
cp "$QT_DIR/plugins/sqldrivers/libqsqlite.so" "$STAGING/plugins/sqldrivers/"
cp "$QT_DIR/plugins/printsupport/libcupsprintersupport.so" "$STAGING/plugins/printsupport/" 2>/dev/null || true
for f in libqjpeg.so libqgif.so libqico.so libqsvg.so; do
    cp "$QT_DIR/plugins/imageformats/$f" "$STAGING/plugins/imageformats/" 2>/dev/null || true
done
echo "  done"

# Step 6: Resources + wrappers
echo "[6/6] Finalizing..."

# Translations
cp "$PROJECT_DIR/Resources/translations/posbox_"*.qm "$STAGING/translations/" 2>/dev/null || true

# Icons
for icon in register_icon_colored settings_icon_colored inventory_icon_colored tickets_icon_colored financial_icon_colored; do
    cp "$PROJECT_DIR/Resources/images/icons/${icon}.png" "$STAGING/icons/" 2>/dev/null || true
done

# Main launcher
cat > "$STAGING/bin/posbox" << 'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "$0")/.." && pwd)"
export LD_LIBRARY_PATH="$DIR/lib"
export QT_PLUGIN_PATH="$DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
exec "$DIR/bin/Register" "$@"
EOF
chmod +x "$STAGING/bin/posbox"

# App launchers
for app in Register Settings Inventory Tickets Financial; do
    LOWER=$(echo "$app" | tr '[:upper:]' '[:lower:]')
    cat > "$STAGING/bin/posbox-$LOWER" << EOF
#!/bin/bash
DIR="\$(cd "\$(dirname "\$0")/.." && pwd)"
export LD_LIBRARY_PATH="\$DIR/lib"
export QT_PLUGIN_PATH="\$DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="\$DIR/plugins/platforms"
exec "\$DIR/bin/$app" "\$@"
EOF
    chmod +x "$STAGING/bin/posbox-$LOWER"
done

# Desktop files
mkdir -p "$STAGING/desktop"
for app in "" "-settings" "-inventory" "-tickets" "-financial"; do
    case "$app" in
        "")          NAME="POSBox";           EXEC="Register";    ICON="register_icon_colored";;
        "-settings") NAME="POSBox Settings";  EXEC="Settings";    ICON="settings_icon_colored";;
        "-inventory")NAME="POSBox Inventory"; EXEC="Inventory";   ICON="inventory_icon_colored";;
        "-tickets")  NAME="POSBox Tickets";   EXEC="Tickets";     ICON="tickets_icon_colored";;
        "-financial")NAME="POSBox Financial"; EXEC="Financial";   ICON="financial_icon_colored";;
    esac
    cat > "$STAGING/desktop/posbox${app}.desktop" << EOF
[Desktop Entry]
Type=Application
Name=${NAME}
Exec=/opt/POSBox/bin/${EXEC}
Icon=${ICON}.png
Terminal=false
Categories=Office;Finance;
EOF
done

# Install/uninstall scripts
cat > "$STAGING/install.sh" << 'INSTALLEOF'
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
INSTALLEOF
chmod +x "$STAGING/install.sh"

cat > "$STAGING/uninstall.sh" << 'UNINSTALLEOF'
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
UNINSTALLEOF
chmod +x "$STAGING/uninstall.sh"

# Create tarball
echo ""
cd "$SCRIPT_DIR/staging"
tar -czf "$SCRIPT_DIR/$ARCHIVE_NAME.tar.gz" "$ARCHIVE_NAME"
SIZE=$(du -h "$SCRIPT_DIR/$ARCHIVE_NAME.tar.gz" | cut -f1)
rm -rf "$STAGING"

echo "============================================"
echo "  Done! $ARCHIVE_NAME.tar.gz ($SIZE)"
echo "============================================"
