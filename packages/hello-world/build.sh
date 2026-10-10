
#!/bin/sh
set -eu

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

# Compile the source code
cc -O2 -Wall -Wextra -o hello hello.c

# Install into Forge's staging directory
install -Dm755 hello \
    "$FORGE_STAGE_DIR$FORGE_PREFIX/bin/hello"
