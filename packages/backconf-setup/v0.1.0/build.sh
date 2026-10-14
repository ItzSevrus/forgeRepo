#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

: "${FORGE_STAGE_DIR:?Forge staging directory is not set}"
: "${FORGE_PREFIX:?Forge installation prefix is not set}"

APP="backconf-setup"
SOURCE="$ROOT/backconf-setup.sh"

log() {
    printf '[%s] %s\n' "$APP" "$*"
}

die() {
    printf '[%s] ERROR: %s\n' "$APP" "$*" >&2
    exit 1
}

# Validate source.
[[ -f "$SOURCE" ]] ||
    die "Missing backconf-setup.sh in package source."

IFS= read -r FIRST_LINE < "$SOURCE" || true
[[ "$FIRST_LINE" == '#!/usr/bin/env bash' ]] ||
    die "backconf-setup.sh must start with #!/usr/bin/env bash."

# Forge's prefix is user-local, e.g. ~/.local.
PREFIX="${FORGE_PREFIX/#\~/$HOME}"
PREFIX="${PREFIX%/}"

# Stage under the Forge prefix without modifying the live system.
DEST="$FORGE_STAGE_DIR$PREFIX/bin/$APP"

install -D -m 0755 -- "$SOURCE" "$DEST"

log "Staged executable: $DEST"
log "Install prefix: $PREFIX"
log "Build staging completed successfully."
