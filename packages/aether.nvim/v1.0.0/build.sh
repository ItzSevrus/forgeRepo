#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"
: "${HOME:?HOME is not set}"

CONFIG_SOURCE="$ROOT/nvim"

if [[ ! -d "$CONFIG_SOURCE" ]]; then
    echo "Error: Neovim configuration directory not found: $CONFIG_SOURCE" >&2
    exit 1
fi

# Forge installs staged files relative to FORGE_PREFIX.
# Stage the config under .config/nvim relative to the user's home.
TARGET="$FORGE_STAGE_DIR$FORGE_PREFIX/../.config/nvim"

echo "==> Staging Aether.nvim configuration..."

mkdir -p "$TARGET"
cp -a "$CONFIG_SOURCE/." "$TARGET/"

if [[ ! -f "$TARGET/init.lua" ]]; then
    echo "Error: init.lua was not staged correctly." >&2
    exit 1
fi

echo "==> Build completed successfully."
echo "    Staged configuration: $TARGET"
