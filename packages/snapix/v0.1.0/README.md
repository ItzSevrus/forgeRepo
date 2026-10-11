# Snapix 📸

**A lightweight, privacy-focused screenshot utility for Hyprland and Wayland.**

Snapix is a command-line screenshot tool written in C++ using Qt 6. It provides fast full-screen and region-based screenshots, with optional clipboard integration.

Designed for simplicity and easy integration, Snapix lets you manage screenshots directly from your terminal or through your Hyprland keybindings.

## ✨ Features

- **Full-screen capture** — Capture your entire screen.
- **Region selection** — Select a rectangular region interactively.
- **Clipboard integration** — Copy screenshots directly to your clipboard.
- **PNG validation** — Validate captured images before saving them.
- **Safe file handling** — Avoid overwriting existing screenshots and use atomic file writes.
- **Hyprland integration** — Configure screenshot shortcuts through a Lua module.
- **Lightweight CLI** — Simple commands without a bulky graphical interface.
- **Privacy-focused design** — Screenshot capture and clipboard operations are performed locally; Snapix does not intentionally upload screenshots to remote servers.

## 🖥️ Requirements

Snapix is designed for Linux systems using Wayland, particularly Hyprland.

### Dependencies

| Dependency | Purpose |
|---|---|
| C++ compiler | Compile the application |
| CMake | Configure and build the project |
| Qt 6 Base | Core application and image validation |
| `grim` | Capture screenshots |
| `slurp` | Interactive region selection |
| `wl-clipboard` | Clipboard integration |

On Arch Linux, install the dependencies with:

```bash
sudo pacman -S --needed base-devel cmake qt6-base grim slurp wl-clipboard
```

## 🔨 Build from Source

Clone or download the source code, then enter the project directory.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The compiled executable will be available at:

```text
build/snapix
```

### Install

To install under your local prefix:

```bash
cmake --install build --prefix "$HOME/.local"
```

Ensure that `~/.local/bin` is in your `PATH` if you want to run `snapix` directly.

> **Note:** The CMake install prefix and Forge package staging prefix may differ. Follow the installation instructions for the package manager you're using.

## 🚀 Usage

### Capture the full screen

```bash
snapix --full
```

### Capture the full screen and copy it

```bash
snapix --full --copy
```

### Select a region and copy it

```bash
snapix --select --copy
```

### Select a region and save it

```bash
snapix --select
```

### Specify an output path

```bash
snapix --full --output "$HOME/Pictures/desktop.png"
```

Use `--help` to see all available options:

```bash
snapix --help
```

## ⌨️ Hyprland Keybindings

Snapix can be integrated into a Lua-based Hyprland configuration using a dedicated module.

Example shortcuts:

| Shortcut | Action |
|---|---|
| `Print` | Capture the full screen |
| `Ctrl + Print` | Capture and copy the full screen |
| `Ctrl + Shift + Print` | Select a region and copy it |
| `Shift + Print` | Select a region and save it |

The exact Lua API depends on your installed Hyprland version and configuration setup.

## 📂 Screenshot Storage

By default, screenshots are saved to:

```text
~/Pictures/Screenshots/
```

Filenames use a timestamp to help organize screenshots, for example:

```text
2026-10-11_09-31-02.png
```

You can override the default destination with `--output`.

## 🔒 Privacy and Security

Snapix is designed to capture screenshots locally without intentionally transmitting them to remote servers.

- No intentional screenshot uploads.
- No intentional telemetry collection.
- Images are saved locally or copied to the local Wayland clipboard.
- Dependencies should be installed from trusted, official repositories.

Always inspect source code and verify package origins before installing software. Privacy and security also depend on the underlying compositor, clipboard manager, operating system, and dependencies.

## 📦 Forge Packaging

Snapix can be packaged for the Forge package manager.

The package build uses CMake and stages application files through the Forge-provided staging directory. Refer to the package's `forge.pkg` and `build.sh` for packaging details.

## 🤝 Contributing

Contributions, bug reports, ideas, and improvements are welcome.

When reporting an issue, please include:

- Your Linux distribution.
- Your Hyprland version.
- The Snapix command you ran.
- Relevant error messages or terminal output.

Please avoid attaching screenshots containing passwords, private messages, or other sensitive information.

## 📄 License

Snapix is distributed under the **MIT License**.

See the [`LICENSE`](LICENSE) file for the full license text.

By using, modifying, or redistributing Snapix, you agree to comply with the applicable license terms.

## ❤️ Built for the Community

Snapix aims to keep screenshot capture simple, fast, transparent, and accessible to the Linux community.

Use trusted software sources, respect user privacy, and help make open-source tools better for everyone.
