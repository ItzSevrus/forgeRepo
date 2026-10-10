
#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build"
PREFIX="$HOME/.local"

ICON_DIR="$PREFIX/share/icons"
DESKTOP_DIR="$PREFIX/share/applications"

echo "==> Building WhatsApp C++"

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "==> Installing executable"

install -Dm755 "$BUILD_DIR/whatsapp" \
    "$PREFIX/bin/whatsapp-cpp"

echo "==> Installing icon"

install -Dm644 "$ROOT/whatsapp-cpp.png" \
    "$ICON_DIR/whatsapp-cpp.png"

echo "==> Creating desktop entry"

install -d "$DESKTOP_DIR"

cat > "$DESKTOP_DIR/whatsapp-cpp.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=WhatsApp C++
Comment=WhatsApp desktop wrapper
Exec=$PREFIX/bin/whatsapp-cpp
Icon=$ICON_DIR/whatsapp-cpp.png
Terminal=false
Categories=Network;InstantMessaging;
StartupNotify=true
EOF

chmod 644 "$DESKTOP_DIR/whatsapp-cpp.desktop"

echo
echo "==> WhatsApp C++ installed successfully!"
echo "Executable: $PREFIX/bin/whatsapp-cpp"
echo "Icon:       $ICON_DIR/whatsapp-cpp.png"
echo "Desktop:    $DESKTOP_DIR/whatsapp-cpp.desktop"
