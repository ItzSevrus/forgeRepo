#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

CONFIG_SOURCE="$ROOT/nvim"
LAUNCHER_SOURCE="$ROOT/aether-nvim"
ICON_SOURCE="$ROOT/aether-nvim.png"
DESKTOP_SOURCE="$ROOT/aether-nvim.desktop"

# Validate package sources.
for path in "$CONFIG_SOURCE" "$LAUNCHER_SOURCE" \
            "$ICON_SOURCE" "$DESKTOP_SOURCE"; do
    if [[ ! -e "$path" ]]; then
        echo "Error: required package file not found: $path" >&2
        exit 1
    fi
done

if [[ ! -f "$CONFIG_SOURCE/init.lua" ]]; then
    echo "Error: nvim/init.lua is missing." >&2
    exit 1
fi

# Forge installs these paths relative to FORGE_PREFIX.
STAGE_PREFIX="$FORGE_STAGE_DIR$FORGE_PREFIX"

CONFIG_TARGET="$STAGE_PREFIX/config/nvim"
LAUNCHER_TARGET="$STAGE_PREFIX/bin/aether-nvim"
ICON_TARGET="$STAGE_PREFIX/share/icons/hicolor/512x512/apps/aether-nvim.png"
DESKTOP_TARGET="$STAGE_PREFIX/share/applications/aether-nvim.desktop"

echo "==> Staging Aether.nvim..."

# 1. Neovim configuration.
echo "==> Installing Neovim configuration..."
mkdir -p "$CONFIG_TARGET"
cp -a "$CONFIG_SOURCE/." "$CONFIG_TARGET/"

# 2. Dedicated launcher.
echo "==> Installing launcher..."
install -Dm755 "$LAUNCHER_SOURCE" "$LAUNCHER_TARGET"

# 3. Application icon.
echo "==> Installing application icon..."
install -Dm644 "$ICON_SOURCE" "$ICON_TARGET"

# 4. Desktop entry.
echo "==> Installing desktop entry..."
install -Dm644 "$DESKTOP_SOURCE" "$DESKTOP_TARGET"

# Verify staged files.
for path in \
    "$CONFIG_TARGET/init.lua" \
    "$LAUNCHER_TARGET" \
    "$ICON_TARGET" \
    "$DESKTOP_TARGET"; do
    if [[ ! -f "$path" ]]; then
        echo "Error: staging verification failed: $path" >&2
        exit 1
    fi
done

if [[ ! -x "$LAUNCHER_TARGET" ]]; then
    echo "Error: staged launcher is not executable." >&2
    exit 1
fi

echo
echo "==> Aether.nvim staged successfully!"
echo "    Configuration: $CONFIG_TARGET"
echo "    Launcher:      $LAUNCHER_TARGET"
echo "    Icon:           $ICON_TARGET"
echo "    Desktop entry:  $DESKTOP_TARGET"

