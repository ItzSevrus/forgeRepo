# Backconf Setup

A lightweight setup launcher for [Backconf](https://github.com/itzsevrus/backconf), a backup utility for managing local backup snapshots.

`backconf-setup` is designed for installation through the Forge package manager. It provides a convenient command to install, repair, or uninstall Backconf.

## Features

- **Simple setup:** Install Backconf using a single command.
- **Repair support:** Rerun the Backconf installer to repair an existing installation.
- **Uninstallation:** Start the Backconf uninstall process.
- **User-local integration:** The launcher is installed within Forge's configured installation prefix, typically under `~/.local`.
- **Source checkout:** The setup launcher downloads the Backconf repository into `~/.local/share/backconf`.

## Requirements

- Linux
- [Forge](https://github.com/itzsevrus/forge)
- Bash
- Git
- `sudo` for operations requiring administrator privileges

The launcher itself does not require a compiler or build system.

## Installation

Install the package using Forge:

```bash
forge install backconf-setup
```

The exact command may vary depending on your configured Forge repository and package version.

## Usage

Run the following commands after installing the package:

```bash
backconf-setup --help
```

Install Backconf:

```bash
backconf-setup install
```

Repair an existing installation:

```bash
backconf-setup repair
```

Uninstall Backconf:

```bash
backconf-setup uninstall
```

## How It Works

1. Forge installs the `backconf-setup` launcher under its configured prefix.
2. The launcher clones the Backconf repository into `~/.local/share/backconf`, or updates an existing checkout when appropriate.
3. The launcher invokes Backconf's own installation script.
4. Operations that modify system-managed files or services may require administrator privileges.

The launcher and the Backconf installer have different responsibilities. Installing or removing the launcher through Forge is separate from installing or removing Backconf itself.

## Security Notice

**Review the scripts before executing them.**

The setup launcher downloads and executes installation code from the Backconf repository. Before running it:

- Review `backconf-setup.sh`.
- Inspect the Backconf repository and its `install.sh` script.
- Verify the repository URL and the source code you intend to execute.
- Understand which files, directories, permissions, and system services the installer modifies.
- Read any license and confirmation prompts before proceeding.

Do not run scripts you do not trust. Administrator privileges can allow installation scripts to modify critical parts of your system.

## License

This package is distributed under the MIT License. See [LICENSE](LICENSE) for details.

Backconf's own license and terms should also be reviewed in its repository.
