#!/bin/sh
set -eu

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

install -Dm755 hello \
    "$FORGE_STAGE_DIR$FORGE_PREFIX/bin/hello"
