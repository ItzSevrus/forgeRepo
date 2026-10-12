return {
    {
        "akinsho/toggleterm.nvim",
        version = "*",
        config = function()
            require("toggleterm").setup({
                size = 12,
                direction = "horizontal",
                persist_size = false,
                persist_mode = true,
                start_in_insert = true,
                close_on_exit = false,
                auto_scroll = true,
                hide_numbers = true,
                shade_terminals = false,
                terminal_mappings = true,
                insert_mappings = true,
            })

            local Terminal = require("toggleterm.terminal").Terminal

            local terminal = Terminal:new({
                direction = "horizontal",
                hidden = true,
                close_on_exit = false,
                cmd = vim.o.shell,

                on_open = function(term)
                    -- Convenient navigation between editor windows.
                    local opts = {
                        buffer = term.bufnr,
                        silent = true,
                    }

                    vim.keymap.set("t", "<C-h>",
                        [[<Cmd>wincmd h<CR>]], opts)
                    vim.keymap.set("t", "<C-j>",
                        [[<Cmd>wincmd j<CR>]], opts)
                    vim.keymap.set("t", "<C-k>",
                        [[<Cmd>wincmd k<CR>]], opts)
                    vim.keymap.set("t", "<C-l>",
                        [[<Cmd>wincmd l<CR>]], opts)
            
                    -- Escape terminal insert mode and return to normal mode.
                    vim.keymap.set("t", "<Esc>", [[<C-\><C-n>]], {
                    desc = "Exit terminal input mode",
                    })
                end,
            })

            local NORMAL_HEIGHT = 12

            local function open_terminal()
                -- If the tree has focus, split the editor instead.
                if vim.bo.filetype == "NvimTree" then
                    for _, win in ipairs(vim.api.nvim_tabpage_list_wins(0)) do
                        local buf = vim.api.nvim_win_get_buf(win)

                        if vim.bo[buf].filetype ~= "NvimTree" then
                            vim.api.nvim_set_current_win(win)
                            break
                        end
                    end
                end

                terminal:open(NORMAL_HEIGHT, "horizontal")
            end

            local function toggle_terminal()
                if terminal:is_open() then
                    terminal:close()
                else
                    open_terminal()
                end
            end

local terminal_maximized = false

local function maximize_terminal()
    if not terminal:is_open() then
        open_terminal()
    end

    local win = terminal.window
    if not win or not vim.api.nvim_win_is_valid(win) then
        return
    end

    terminal_maximized = true

    -- Remove fixed sizing before expanding.
    vim.wo[win].winfixheight = false
    vim.api.nvim_set_current_win(win)
    vim.cmd("wincmd _")
end

local function minimize_terminal()
    if not terminal:is_open() then
        open_terminal()
        return
    end

    local win = terminal.window
    if not win or not vim.api.nvim_win_is_valid(win) then
        return
    end

    terminal_maximized = false

    vim.wo[win].winfixheight = false
    vim.api.nvim_set_current_win(win)

    -- Restore the normal bottom-panel height.
    vim.api.nvim_win_set_height(win, NORMAL_HEIGHT)
    vim.cmd("wincmd =")
end 

            -- Space + t, then t: toggle.
            vim.keymap.set("n", "<leader>tt", toggle_terminal, {
                desc = "Toggle bottom terminal",
            })

            -- Space + t, then Shift+M: maximize.
            vim.keymap.set("n", "<leader>tM", maximize_terminal, {
                desc = "Maximize terminal",
            })

            -- Space + t, then m: restore normal height.
            vim.keymap.set("n", "<leader>tm", minimize_terminal, {
                desc = "Minimize terminal",
            })
        end,
    },
}
