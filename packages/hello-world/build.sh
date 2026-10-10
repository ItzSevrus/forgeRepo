#!/bin/sh
set -eu

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

# Locate hello.c anywhere in the extracted source tree.
source_file=$(find . -type f -name hello.c -print -quit)

if [ -z "$source_file" ]; then
    echo "Error: hello.c was not found in the extracted source archive." >&2
    exit 1
fi

# Compile from the directory containing hello.c.
cd "$(dirname "$source_file")"
cc -O2 -Wall -Wextra -o hello hello.c

# Stage the executable for Forge to install.
install -Dm755 hello \
    "$FORGE_STAGE_DIR$FORGE_PREFIX/bin/hello"
