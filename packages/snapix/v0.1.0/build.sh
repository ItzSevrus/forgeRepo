#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

log() {
    printf '[snapix] %s\n' "$*"
}

die() {
    printf '[snapix] ERROR: %s\n' "$*" >&2
    exit 1
}

# Validate source files.
[[ -f "$ROOT/CMakeLists.txt" ]] || die "CMakeLists.txt is missing."
[[ -f "$ROOT/main.cpp" ]] || die "main.cpp is missing."
[[ -f "$ROOT/hyprland/snapix.lua" ]] || die "hyprland/snapix.lua is missing."

# Verify supported package manager.
command -v pacman >/dev/null 2>&1 ||
    die "This installer requires Arch Linux and pacman."

# Install missing dependencies.
packages=(
    base-devel
    cmake
    qt6-base
    grim
    slurp
    wl-clipboard
)

missing=()

for package in "${packages[@]}"; do
    if ! pacman -Q "$package" >/dev/null 2>&1; then
        missing+=("$package")
    fi
done

if ((${#missing[@]})); then
    log "Installing dependencies: ${missing[*]}"
    sudo pacman -S --needed -- "${missing[@]}" ||
        die "Dependency installation failed."
fi

for command_name in cmake c++ grim slurp wl-copy; do
    command -v "$command_name" >/dev/null 2>&1 ||
        die "Required command not found: $command_name"
done

# Build in a temporary directory.
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/snapix-build.XXXXXX")"

cleanup() {
    rm -rf -- "$BUILD_DIR"
}
trap cleanup EXIT

log "Configuring CMake..."

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$FORGE_PREFIX"

log "Building Snapix..."
cmake --build "$BUILD_DIR" --parallel

# Install the executable and other CMake-managed files into Forge staging.
log "Staging application..."

DESTDIR="$FORGE_STAGE_DIR" \
    cmake --install "$BUILD_DIR"

# Stage the Lua module.
STAGED_MODULE="$FORGE_STAGE_DIR$FORGE_PREFIX/share/snapix/modules/snapix.lua"

install -Dm644 \
    "$ROOT/hyprland/snapix.lua" \
    "$STAGED_MODULE"

log "Application and Lua module staged successfully."

# Detect Hyprland.
if ! command -v hyprctl >/dev/null 2>&1; then
    log "Hyprland is not installed; skipping Hyprland configuration."
    exit 0
fi

# Use the standard config path, with support for custom locations.
CONFIG_DIR="${HYPRLAND_CONFIG_DIR:-$HOME/.config/hypr}"
MAIN_CONFIG="$CONFIG_DIR/hyprland.lua"
MODULE_DIR="$CONFIG_DIR/modules"
MODULE_FILE="$MODULE_DIR/snapix.lua"

mkdir -p "$CONFIG_DIR" "$MODULE_DIR"

# Back up the main config before editing it.
if [[ -f "$MAIN_CONFIG" ]]; then
    if [[ ! -f "$MAIN_CONFIG.bak" ]]; then
        cp -a -- "$MAIN_CONFIG" "$MAIN_CONFIG.bak"
        log "Backup created: $MAIN_CONFIG.bak"
    else
        log "Existing backup preserved: $MAIN_CONFIG.bak"
    fi
else
    touch "$MAIN_CONFIG"
    cp -a -- "$MAIN_CONFIG" "$MAIN_CONFIG.bak"
    log "Created new configuration and backup."
fi

# Install the module into the user's config directory.
install -m644 "$ROOT/hyprland/snapix.lua" "$MODULE_FILE"

# Add the import only once.
IMPORT='require("modules.snapix")'

if ! grep -Fqx -- "$IMPORT" "$MAIN_CONFIG"; then
    printf '\n-- Snapix screenshot bindings\n%s\n' "$IMPORT" \
        >> "$MAIN_CONFIG"

    log "Added Snapix import to $MAIN_CONFIG"
else
    log "Snapix import already exists."
fi

# Reload only when running inside an active Hyprland session.
if [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]]; then
    if hyprctl reload; then
        log "Hyprland configuration reloaded."
    else
        log "WARNING: Hyprland reload failed."
        log "Restore the backup with:"
        log "cp '$MAIN_CONFIG.bak' '$MAIN_CONFIG'"
    fi
else
    log "No active Hyprland session; changes will apply next session."
fi

log "Snapix build and staging completed."
