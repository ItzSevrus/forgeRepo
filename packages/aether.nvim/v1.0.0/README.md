# Aether.nvim

**A personalized Neovim IDE configuration for developers.**

Aether.nvim transforms Neovim into a more comfortable development environment with a file explorer, integrated terminal, syntax highlighting, code intelligence, and support for C/C++ and Python.

## Features

- **File Explorer** — Navigate projects and files with NvimTree.
- **Integrated Terminal** — Open and manage terminal sessions inside Neovim using ToggleTerm.
- **Syntax Highlighting** — Improved code readability with Treesitter.
- **Code Intelligence** — LSP support for diagnostics, definitions, references, and code actions.
- **C/C++ Development** — Integration with clangd.
- **Python Development** — Integration with Pyright and Python virtual environment detection.
- **Fuzzy Finding** — Search files and project content using Telescope.
- **Status Line** — Useful editor information with Lualine.
- **Buffer Management** — Switch between open files with Bufferline.
- **Custom Keybindings** — Convenient shortcuts for common development tasks.
- **Plugin Management** — Plugins managed through lazy.nvim.

## Requirements

- Neovim
- Git
- ripgrep
- fd
- A Nerd Font or compatible terminal font is recommended for icons.
- A Wayland clipboard utility such as `wl-clipboard` when using Wayland.

### Optional development tools

Install the tools appropriate for your projects:

- **C/C++:** clangd, Clang or GCC, CMake, and Ninja or Make.
- **Python:** Python 3, Pyright, and virtual environment support.

Some tools are only needed for particular features. Language servers must be installed separately if they are not already available on your system.

## Installation

Install Aether.nvim through Forge using the package name:

```bash
forge install aether.nvim@latest
```

The package stages the configuration under `.config/nvim` according to the configured Forge installation prefix.

After installation, start Neovim:

```bash
nvim
```

lazy.nvim may download configured plugins on the first launch. Internet access may be required for initial setup.

## Configuration

The configuration is organized into Lua modules:

```text
nvim/
├── init.lua
└── lua/
    ├── config/
    │   └── python_venv.lua
    └── plugins/
        ├── c-cpp.lua
        ├── editor.lua
        ├── terminal.lua
        └── ui.lua
```

You can customize the configuration by editing these files.

**Tip:** Back up your existing `~/.config/nvim` directory before installing if you want to preserve your current configuration.

## Philosophy

Aether.nvim aims to make Neovim a practical, customizable development environment while keeping the configuration transparent and user-controlled.

## License

See the package's license declaration and accompanying license text for the applicable terms.
