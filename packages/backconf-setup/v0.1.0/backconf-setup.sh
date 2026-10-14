#!/usr/bin/env bash
set -Eeuo pipefail

REPO_URL="https://github.com/itzsevrus/backconf.git"
REPO_DIR_NAME="backconf"

log() {
    printf '[backconf-setup] %s\n' "$*"
}

die() {
    printf '[backconf-setup] ERROR: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Backconf Setup

Usage:
  backconf-setup install
  backconf-setup repair
  backconf-setup uninstall
  backconf-setup --help
EOF
}

ACTION="${1:-}"

case "$ACTION" in
    install|repair|uninstall)
        ;;
    --help|-h|help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

[[ $# -eq 1 ]] || die "Provide exactly one command."

# Resolve the actual user, even when invoked through sudo.
if [[ -n "${SUDO_USER:-}" && "$SUDO_USER" != root ]]; then
    TARGET_USER="$SUDO_USER"
elif [[ $EUID -ne 0 ]]; then
    TARGET_USER="$(id -un)"
else
    die "Run this command as your normal user, not directly as root."
fi

TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
[[ -n "$TARGET_HOME" && -d "$TARGET_HOME" ]] ||
    die "Could not determine the home directory for $TARGET_USER."

SOURCE_DIR="$TARGET_HOME/.local/share/$REPO_DIR_NAME"

run_as_user() {
    if [[ $EUID -eq 0 ]]; then
        sudo -u "$TARGET_USER" env HOME="$TARGET_HOME" "$@"
    else
        "$@"
    fi
}

run_as_root() {
    if [[ $EUID -eq 0 ]]; then
        "$@"
    else
        sudo -- "$@"
    fi
}

# Uninstall should work even if the source checkout has been removed.
if [[ "$ACTION" == "uninstall" ]]; then
    INSTALLED_INSTALLER="/usr/local/lib/backconf/install.sh"

    [[ -x "$INSTALLED_INSTALLER" ]] ||
        die "Backconf is not installed, or its installed installer is missing."

    log "Starting uninstall..."
    run_as_root "$INSTALLED_INSTALLER" uninstall
    exit $?
fi

command -v git >/dev/null 2>&1 ||
    die "Git is required. Install it using your system package manager."

run_as_user mkdir -p "$TARGET_HOME/.local/share"

if [[ ! -d "$SOURCE_DIR/.git" ]]; then
    if [[ -e "$SOURCE_DIR" ]]; then
        die "$SOURCE_DIR exists but is not a Git checkout. Move it aside before continuing."
    fi

    log "Cloning Backconf into $SOURCE_DIR..."
    run_as_user git clone -- "$REPO_URL" "$SOURCE_DIR" ||
        die "Could not clone the Backconf repository."
else
    # Ensure the existing checkout belongs to the expected repository.
    ACTUAL_URL="$(run_as_user git -C "$SOURCE_DIR" remote get-url origin)" ||
        die "Could not read the repository remote."

    [[ "$ACTUAL_URL" == "$REPO_URL" ||
       "$ACTUAL_URL" == "https://github.com/itzsevrus/backconf" ]] ||
        die "The existing checkout has an unexpected Git remote: $ACTUAL_URL"

    # Never silently discard local modifications.
    if [[ -n "$(run_as_user git -C "$SOURCE_DIR" status --porcelain)" ]]; then
        die "The checkout has local changes. Commit or back them up before continuing."
    fi

    log "Updating the Backconf source checkout..."
    run_as_user git -C "$SOURCE_DIR" pull --ff-only ||
        die "Could not safely update the repository."
fi

[[ -f "$SOURCE_DIR/install.sh" ]] ||
    die "The repository does not contain install.sh."

log "Running Backconf $ACTION..."
run_as_root "$SOURCE_DIR/install.sh" "$ACTION"
