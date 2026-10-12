
return {
    -- Modern syntax highlighting
    {
        "nvim-treesitter/nvim-treesitter",
        branch = "master",
        lazy = false,
        build = ":TSUpdate",
        config = function()
            require("nvim-treesitter.configs").setup({
                ensure_installed = {
                    "c",
                    "cpp",
                    "cmake",
                    "make",
                    "bash",
                    "lua",
                    "python"
                },

                highlight = {
                    enable = true,
                    additional_vim_regex_highlighting = false,
                },

                indent = {
                    enable = true,
                },
            })
        end,
    },

    -- C/C++ language server integration
    {
        "neovim/nvim-lspconfig",
        lazy = false,
        config = function()
            vim.lsp.config("clangd", {
                cmd = {
                    "clangd",
                    "--background-index",
                    "--clang-tidy",
                    "--completion-style=detailed",
                    "--header-insertion=iwyu",
                },

                filetypes = { "c", "cpp", "objc", "objcpp" },

                root_markers = {
                    "compile_commands.json",
                    "compile_flags.txt",
                    ".clangd",
                    ".git",
                    "CMakeLists.txt",
                },
            })
   
            -- Python language server
            vim.lsp.config("pyright", {
    
                cmd = { "pyright-langserver", "--stdio" },
                filetypes = { "python" },

                root_dir = function(bufnr, on_dir)
                local filename = vim.api.nvim_buf_get_name(bufnr)
                local root = vim.fs.root(filename, {
                    "pyrightconfig.json",
                    "pyproject.toml",
                    "setup.py",
                    "setup.cfg",
                    "requirements.txt",
                    "Pipfile",
                    ".git",
                })
                -- Fall back to the directory containing the file.
                on_dir(root or vim.fs.dirname(filename))
                end,
                settings = {
                    python = {
                    analysis = {
                        typeCheckingMode = "basic",
                        autoSearchPaths = true,
                        useLibraryCodeForTypes = true,

                        -- Check open Python files even without a project root.
                        diagnosticMode = "openFilesOnly",
                        },
                    },
                },
            })
            vim.lsp.enable("pyright")
            vim.lsp.enable("clangd")
        end,
    },
}

