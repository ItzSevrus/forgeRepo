# Forge 0.3.1 development source

Forge is a user-local package manager written in C. This source snapshot adds optional package messages, installed repository reporting, upgrade checks, and cache cleaning.

## Build

Requirements: a C11 compiler, `pkg-config`, libcurl development files, and OpenSSL development files.

```sh
make
./build/forge --help
make install
```

## New commands

- `forge list` — show installed package, version, and source repository.
- `forge upgrade` — check installed packages against the currently synced catalogs.
- `forge upgrade <package>` — check and offer to upgrade one installed package.
- `forge clean` — interactively remove Forge's cache/build/download directory. Installed files and synced repository catalogs are preserved.

Run `forge repo sync` before checking for updates so the local catalog is current.

## Optional package message

A package version directory may contain `MESSAGE.md` next to `forge.pkg`. Forge displays it before the install confirmation. It can include maintainer notes, license terms, warnings, or other information. If the file is absent or unavailable, installation continues with the normal confirmation prompt.

## Upgrade caveat

This implementation removes the installed version's recorded files before starting the new install; it is not yet a transactional upgrade. If the new build fails, Forge reports the old version and how to reinstall it. Review the package and keep backups for important software.
