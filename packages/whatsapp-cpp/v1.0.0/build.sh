#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build"

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

echo "==> Building WhatsApp C++"

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "==> Staging executable"

install -Dm755 "$BUILD_DIR/whatsapp" \
    "$FORGE_STAGE_DIR$FORGE_PREFIX/bin/whatsapp-cpp"

echo "==> Staging icon"

install -Dm644 "$ROOT/whatsapp-cpp.png" \
    "$FORGE_STAGE_DIR$FORGE_PREFIX/share/icons/whatsapp-cpp.png"

echo "==> Creating desktop entry"

DESKTOP_FILE="$FORGE_STAGE_DIR$FORGE_PREFIX/share/applications/whatsapp-cpp.desktop"

install -d "$(dirname "$DESKTOP_FILE")"

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=WhatsApp C++
Comment=WhatsApp desktop wrapper
Exec=$FORGE_PREFIX/bin/whatsapp-cpp
Icon=$FORGE_PREFIX/share/icons/whatsapp-cpp.png
Terminal=false
Categories=Network;InstantMessaging;
StartupNotify=true
EOF

chmod 644 "$DESKTOP_FILE"

echo
echo "==> WhatsApp C++ staged successfully!"
echo "Executable: $FORGE_PREFIX/bin/whatsapp-cpp"
echo "Icon:       $FORGE_PREFIX/share/icons/whatsapp-cpp.png"
echo "Desktop:    $FORGE_PREFIX/share/applications/whatsapp-cpp.desktop"