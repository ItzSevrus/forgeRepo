# Forge Repository Template

A starter template for creating your own Forge package repository.

## Structure

- `repository.conf` — repository metadata and package layout.
- `packages/` — package names and their versioned directories.
- `packages/<name>/<version>/forge.pkg` — package manifest.
- `packages/<name>/<version>/build.sh` — package build script.
- `packages/<name>/<version>/` — package source files and other required assets.

## Creating a Package

1. Create a directory under `packages/<package-name>/<version>/`.
2. Add a `forge.pkg` manifest following the Forge package format.
3. Add the required source files and build script.
4. Configure the source archive URL and its matching SHA-256 checksum.
5. Validate the repository using Forge before publishing it.

## Important

This is a starter template. Update the repository metadata and package
details before publishing your own repository.

Review build scripts before executing them.
