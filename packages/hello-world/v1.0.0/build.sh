#!/bin/sh
set -eu

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

# Find main.c, whether it's at the archive root or in a subdirectory.
source_file=$(find . -type f -name main.c -print -quit)

if [ -z "$source_file" ]; then
    echo "Error: main.c not found in extracted source archive." >&2
    exit 1
fi

# Compile the source into an executable named hello.
cd "$(dirname "$source_file")"
cc -O2 -Wall -Wextra -o hello main.c

# Stage it for Forge to install.
install -Dm755 hello \
    "$FORGE_STAGE_DIR$FORGE_PREFIX/bin/hello"
