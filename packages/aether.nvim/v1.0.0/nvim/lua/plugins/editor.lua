
return {
    -- Persistent left-side project explorer
    {
        "nvim-tree/nvim-tree.lua",
        version = "*",
        lazy = false,
        dependencies = {
            "nvim-tree/nvim-web-devicons",
        },
        config = function()
            local api = require("nvim-tree.api")

            require("nvim-tree").setup({
                view = {
                    side = "left",
                    width = 28,
                    preserve_window_proportions = true,
                },

                hijack_directories = {
                    enable = false,
                },

                -- Follow the current project directory
                sync_root_with_cwd = true,
                respect_buf_cwd = false,

                update_focused_file = {
                    enable = true,
                    update_root = true,
                    ignore_list = {},
                },

                actions = {
                    open_file = {
                        quit_on_open = false,
                        resize_window = false,
                    },
                },

                renderer = {
                    root_folder_label = ":~:s?",
                    group_empty = true,
                    highlight_git = true,
                    indent_markers = {
                        enable = true,
                    },
                    icons = {
                        show = {
                            file = true,
                            folder = true,
                            folder_arrow = true,
                            git = true,
                        },
                    },
                },

                filters = {
                    dotfiles = false,
                },

                git = {
                    enable = true,
                },

                on_attach = function(bufnr)
                    local function opts(desc)
                        return {
                            desc = "NvimTree: " .. desc,
                            buffer = bufnr,
                            noremap = true,
                            silent = true,
                            nowait = true,
                        }
                    end

                    api.config.mappings.default_on_attach(bufnr)

                    vim.keymap.set(
                        "n", "<CR>", api.node.open.edit, opts("Open")
                    )
                    vim.keymap.set(
                        "n", "o", api.node.open.edit, opts("Open")
                    )
                    vim.keymap.set(
                        "n", "h", api.node.navigate.parent_close,
                        opts("Close parent")
                    )
                    vim.keymap.set(
                        "n", "l", api.node.open.edit, opts("Open")
                    )
                end,
            })
        end,
    },

    -- Search files and project contents
    {
        "nvim-telescope/telescope.nvim",
        branch = "master",
        dependencies = {
            "nvim-lua/plenary.nvim",
        },
        keys = {
            {
                "<leader>ff",
                "<cmd>Telescope find_files<CR>",
                desc = "Find files",
            },
            {
                "<leader>fg",
                "<cmd>Telescope live_grep<CR>",
                desc = "Search project",
            },
            {
                "<leader>fb",
                "<cmd>Telescope buffers<CR>",
                desc = "Open buffers",
            },
            {
                "<leader>fr",
                "<cmd>Telescope oldfiles<CR>",
                desc = "Recent files",
            },
        },
        opts = {},
    },

    -- VS Code-style file tabs
    {
        "akinsho/bufferline.nvim",
        version = "*",
        dependencies = {
            "nvim-tree/nvim-web-devicons",
        },
        event = "VeryLazy",
        opts = {
            options = {
                mode = "buffers",
                diagnostics = "nvim_lsp",
                separator_style = "slant",
                always_show_bufferline = true,
                show_buffer_close_icons = true,
                show_close_icon = false,

                offsets = {
                    {
                        filetype = "NvimTree",
                        text = "EXPLORER",
                        highlight = "Directory",
                        separator = true,
                    },
                },
            },
        },
    },
}


