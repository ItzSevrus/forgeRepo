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

# The Lua module is stored in the source's modules directory.
LUA_MODULE="$ROOT/modules/snapix.lua"

if [[ -f "$LUA_MODULE" ]]; then
    log "Hyprland Lua module found: modules/snapix.lua"
else
    log "Hyprland Lua module not found; shortcut integration will be skipped."
fi

# Verify supported package manager.
command -v pacman >/dev/null 2>&1 ||
    die "This build script requires Arch Linux and pacman."

# Check dependencies.
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

# Ask permission before installing dependencies.
if ((${#missing[@]})); then
    printf '\n'
    printf '%s\n' "Snapix requires the following additional Arch Linux packages:"
    printf '  - %s\n' "${missing[@]}"
    printf '\n'
    printf '%s\n' \
        "Pacman will install these packages and any required dependencies."
    printf '%s\n' \
        "This requires sudo privileges and may download packages."
    printf '\n'

    read -r -p "Proceed with sudo pacman installation? [y/N] " answer

    case "$answer" in
        y|Y|yes|YES|Yes)
            log "Installing approved dependencies..."
            sudo pacman -S --needed -- "${missing[@]}" ||
                die "Dependency installation failed."
            ;;
        *)
            die "Dependency installation declined. Nothing was installed."
            ;;
    esac
else
    log "All declared system dependencies are already installed."
fi

# Verify required build and runtime commands.
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

# Stage application files for Forge.
log "Staging application..."

DESTDIR="$FORGE_STAGE_DIR" cmake --install "$BUILD_DIR"

# Stage the Lua module as part of the package.
if [[ -f "$LUA_MODULE" ]]; then
    STAGED_MODULE="$FORGE_STAGE_DIR$FORGE_PREFIX/share/snapix/modules/snapix.lua"
    install -Dm644 "$LUA_MODULE" "$STAGED_MODULE"
    log "Lua module staged."
fi

log "Application staging completed."

# No Lua module means no Hyprland shortcut configuration.
if [[ ! -f "$LUA_MODULE" ]]; then
    log "Skipping Hyprland configuration because the module is absent."
    log "Snapix build completed successfully."
    exit 0
fi

# Detect whether Hyprland's control utility is installed.
if ! command -v hyprctl >/dev/null 2>&1; then
    log "Hyprland was not detected; skipping configuration."
    exit 0
fi

# Configure the current user's Hyprland setup.
CONFIG_DIR="${HYPRLAND_CONFIG_DIR:-$HOME/.config/hypr}"
MAIN_CONFIG="$CONFIG_DIR/hyprland.lua"
MODULE_DIR="$CONFIG_DIR/modules"
MODULE_FILE="$MODULE_DIR/snapix.lua"

# Create the config and modules directories if necessary.
mkdir -p "$MODULE_DIR"

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
    log "Created Hyprland config and backup."
fi

# Install the module into ~/.config/hypr/modules/.
install -m644 "$LUA_MODULE" "$MODULE_FILE"
log "Installed Lua module: $MODULE_FILE"

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
        log "Backup available at: $MAIN_CONFIG.bak"
    fi
else
    log "No active Hyprland session; changes apply next session."
fi

log "Snapix build and staging completed."
