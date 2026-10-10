#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

APP="$ROOT/forge-dev"

if [[ ! -f "$APP" ]]; then
    echo "Error: forge-dev executable not found at $APP" >&2
    exit 1
fi

if [[ ! -x "$APP" ]]; then
    echo "Error: forge-dev is not executable. Run chmod +x forge-dev." >&2
    exit 1
fi

# Install the CLI.
install -Dm755 \
    "$APP" \
    "$FORGE_STAGE_DIR$FORGE_PREFIX/bin/forge-dev"

echo "Installed forge-dev executable."

# Install the repository template if this package includes one.
if [[ -d "$ROOT/template" ]]; then
    install -d \
        "$FORGE_STAGE_DIR$FORGE_PREFIX/share/forge-dev/template"

    cp -a "$ROOT/template/." \
        "$FORGE_STAGE_DIR$FORGE_PREFIX/share/forge-dev/template/"

    echo "Installed Forge repository template."
fi

echo "forge-dev staging completed successfully."