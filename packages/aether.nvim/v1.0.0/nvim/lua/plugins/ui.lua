
return {
    -- Dark theme
    {
        "folke/tokyonight.nvim",
        priority = 1000,
        lazy = false,
        opts = {
            style = "night",
            transparent = false,
            styles = {
                sidebars = "dark",
                floats = "dark",
            },
        },
        config = function(_, opts)
            require("tokyonight").setup(opts)
            vim.cmd.colorscheme("tokyonight")
        end,
    },

    -- File icons
    {
        "nvim-tree/nvim-web-devicons",
        lazy = true,
    },

    -- Statusline
    {
        "nvim-lualine/lualine.nvim",
        event = "VeryLazy",
        dependencies = {
            "nvim-tree/nvim-web-devicons",
        },
        opts = {
            options = {
                theme = "tokyonight",
                globalstatus = true,
                section_separators = "",
                component_separators = "|",
            },
            sections = {
                lualine_a = { "mode" },
                lualine_b = { "branch", "diff" },
                lualine_c = { { "filename", path = 1 } },
                lualine_x = { "diagnostics", "filetype" },
                lualine_y = { "progress" },
                lualine_z = { "location" },
            },
        },
    },

    -- Startup dashboard
    {
        "goolord/alpha-nvim",
        event = "VimEnter",
        dependencies = {
            "nvim-tree/nvim-web-devicons",
        },
        config = function()
            local alpha = require("alpha")
            local dashboard = require("alpha.themes.dashboard")

            dashboard.section.header.val = {
                [[      _   __              _       ]],
                [[     / | / /__  _   __  (_)___  ]],
                [[    /  |/ / _ \| | / / / / __ \ ]],
                [[   / /|  /  __/| |/ / / / / / / ]],
                [[  /_/ |_/\___/ |___/ /_/_/ /_/  ]],
                [[                                 ]],
                [[       YOUR TERMINAL IDE         ]],
            }

            dashboard.section.buttons.val = {
                dashboard.button("f", "  Find file", ":Telescope find_files<CR>"),
                dashboard.button("r", "  Recent files", ":Telescope oldfiles<CR>"),
                dashboard.button("g", "  Find text", ":Telescope live_grep<CR>"),
                dashboard.button("q", "  Quit", ":qa<CR>"),
            }

            dashboard.section.footer.val = "Built for focused development"

            alpha.setup(dashboard.config)
        end,
    },
}

