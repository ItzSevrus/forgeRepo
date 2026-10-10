#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

for tool in make cc pkg-config; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Error: required tool '$tool' is not installed." >&2
        exit 1
    fi
done

if [[ ! -f "$ROOT/Makefile" ]]; then
    echo "Error: Makefile not found in $ROOT" >&2
    exit 1
fi

if ! pkg-config --exists libcurl openssl; then
    echo "Error: libcurl and OpenSSL development files are required." >&2
    exit 1
fi

echo "==> Building Forge..."

# Clean existing objects to avoid stale build artifacts.
make -C "$ROOT" clean

# Compile the application.
make -C "$ROOT" -j"$(nproc)" all

BINARY="$ROOT/build/forge"

if [[ ! -x "$BINARY" ]]; then
    echo "Error: Forge executable was not produced." >&2
    exit 1
fi

# Stage the executable at the requested installation prefix.
TARGET="$FORGE_STAGE_DIR$FORGE_PREFIX/bin/forge"

echo "==> Staging Forge..."
install -Dm755 "$BINARY" "$TARGET"

echo "==> Build completed successfully."
echo "    Staged executable: $TARGET"
