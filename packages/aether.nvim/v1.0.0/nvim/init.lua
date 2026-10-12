
-- ==========================================
-- Neovim IDE Configuration
-- Arch Linux | Lua | C/C++ | CMake
-- ==========================================

-- Leader keys
vim.g.mapleader = " "
vim.g.maplocalleader = " "

-- Disable built-in netrw; use NvimTree instead
vim.g.loaded_netrw = 1
vim.g.loaded_netrwPlugin = 1

-- ------------------------------------------
-- Editor options
-- ------------------------------------------
local opt = vim.opt

opt.number = true
opt.relativenumber = false
opt.mouse = "a"
opt.termguicolors = true
opt.signcolumn = "yes"
opt.cursorline = true
opt.wrap = false
opt.scrolloff = 8
opt.sidescrolloff = 8
opt.updatetime = 250
opt.timeoutlen = 400
opt.splitright = true
opt.splitbelow = true
opt.ignorecase = true
opt.smartcase = true
opt.expandtab = true
opt.shiftwidth = 4
opt.tabstop = 4
opt.smartindent = true
opt.undofile = true
opt.completeopt = { "menu", "menuone", "noselect" }

-- Clipboard integration
opt.clipboard = "unnamedplus"

-- Keep backups and persistent undo
opt.undofile = true
opt.swapfile = true
opt.backup = false
opt.writebackup = true

-- ------------------------------------------
-- General editing
-- ------------------------------------------

-- Save
vim.keymap.set("n", "<leader>w", "<cmd>write<CR>",
    { desc = "Save file" })
vim.keymap.set("n", "<leader>W", "<cmd>wall<CR>",
    { desc = "Save all files" })

-- Close current window / quit
vim.keymap.set("n", "<leader>q", "<cmd>quit<CR>",
    { desc = "Close window safely" })
vim.keymap.set("n", "<leader>Q", "<cmd>quit!<CR>",
    { desc = "Force close window" })

-- Close current buffer (file)
vim.keymap.set("n", "<leader>bc", "<cmd>bdelete<CR>",
    { desc = "Close current file" })
vim.keymap.set("n", "<leader>bC", "<cmd>bdelete!<CR>",
    { desc = "Force close current file" })

-- Discard unsaved changes and reload the file
vim.keymap.set("n", "<leader>bd", function()
    if vim.bo.buftype ~= "" then
        vim.notify("Not a normal file buffer",
            vim.log.levels.WARN)
        return
    end

    if vim.api.nvim_buf_get_name(0) == "" then
        vim.notify("This buffer has no file to reload",
            vim.log.levels.WARN)
        return
    end

    vim.cmd("edit!")
end, { desc = "Discard changes and reload file" })

-- Quit all windows
vim.keymap.set("n", "<leader>qa", "<cmd>confirm qall<CR>",
    { desc = "Quit all safely" })
vim.keymap.set("n", "<leader>qA", "<cmd>qa!<CR>",
    { desc = "Force quit everything" })

-- ------------------------------------------
-- Clipboard and deletion
-- ------------------------------------------

-- Delete without replacing the clipboard.
-- Works with motions: Space+d then w, $, i{, etc.
vim.keymap.set({ "n", "x" }, "<leader>d", '"_d',
    { desc = "Delete without yanking" })

-- Small x deletions do not overwrite clipboard either
vim.keymap.set({ "n", "x" }, "x", '"_x',
    { desc = "Delete character without yanking" })

-- Explicit system clipboard operations
vim.keymap.set({ "n", "x" }, "<leader>y", '"+y',
    { desc = "Yank to system clipboard" })
vim.keymap.set("n", "<leader>Y", '"+Y',
    { desc = "Copy line to system clipboard" })
vim.keymap.set({ "n", "x" }, "<leader>p", '"+p',
    { desc = "Paste from system clipboard" })
vim.keymap.set({ "n", "x" }, "<leader>P", '"+P',
    { desc = "Paste before from system clipboard" })

-- Keep visual selection when indenting
vim.keymap.set("x", "<", "<gv")
vim.keymap.set("x", ">", ">gv")

-- Escape clears search highlighting
vim.keymap.set("n", "<Esc>", "<cmd>nohlsearch<CR>")

-- Undo / redo
vim.keymap.set("n", "U", "<C-r>",
    { desc = "Redo" })

-- ------------------------------------------
-- Explorer: NvimTree only
-- ------------------------------------------

vim.keymap.set("n", "<leader>ee",
    "<cmd>NvimTreeToggle<CR>",
    { desc = "Toggle explorer" })

vim.keymap.set("n", "<leader>el",
    "<cmd>NvimTreeFocus<CR>",
    { desc = "Focus explorer" })

vim.keymap.set("n", "<leader>ec", function()
    local ok, api = pcall(require, "nvim-tree.api")

    if ok and api.tree.is_visible() then
        api.tree.close()
    else
        -- Also close a legacy netrw window if one exists
        if vim.bo.filetype == "netrw" then
            vim.cmd("close")
        else
            vim.notify("Explorer is already closed",
                vim.log.levels.INFO)
        end
    end
end, { desc = "Close explorer" })

vim.keymap.set("n", "<leader>er", function()
    local api = require("nvim-tree.api")
    local root = vim.fn.getcwd()
    api.tree.change_root(root)
    api.tree.open()
end, { desc = "Reset explorer root" })

vim.keymap.set("n", "<leader>ef",
    "<cmd>NvimTreeFindFile<CR>",
    { desc = "Reveal current file" })

-- ------------------------------------------
-- Window navigation
-- ------------------------------------------

-- Space + arrow keys
vim.keymap.set("n", "<leader><Left>", "<C-w>h",
    { desc = "Focus left window" })
vim.keymap.set("n", "<leader><Right>", "<C-w>l",
    { desc = "Focus right window" })
vim.keymap.set("n", "<leader><Up>", "<C-w>k",
    { desc = "Focus upper window" })
vim.keymap.set("n", "<leader><Down>", "<C-w>j",
    { desc = "Focus lower window" })

-- Ctrl + h/j/k/l navigation
vim.keymap.set("n", "<C-h>", "<C-w>h")
vim.keymap.set("n", "<C-j>", "<C-w>j")
vim.keymap.set("n", "<C-k>", "<C-w>k")
vim.keymap.set("n", "<C-l>", "<C-w>l")

-- Split windows
vim.keymap.set("n", "<leader>sv", "<cmd>vsplit<CR>",
    { desc = "Vertical split" })
vim.keymap.set("n", "<leader>sh", "<cmd>split<CR>",
    { desc = "Horizontal split" })
vim.keymap.set("n", "<leader>sx", "<cmd>close<CR>",
    { desc = "Close split" })

-- ------------------------------------------
-- Buffer navigation
-- ------------------------------------------

vim.keymap.set("n", "<Tab>", "<cmd>bnext<CR>",
    { desc = "Next file tab" })
vim.keymap.set("n", "<S-Tab>", "<cmd>bprevious<CR>",
    { desc = "Previous file tab" })


-- C/C++ language-server shortcuts

vim.keymap.set("n", "gd", vim.lsp.buf.definition,
    { desc = "Go to definition" })

vim.keymap.set("n", "gD", vim.lsp.buf.declaration,
    { desc = "Go to declaration" })

vim.keymap.set("n", "gr", vim.lsp.buf.references,
    { desc = "Find references" })

vim.keymap.set("n", "K", vim.lsp.buf.hover,
    { desc = "Show documentation" })

vim.keymap.set("n", "<leader>cr", vim.lsp.buf.rename,
    { desc = "Rename symbol" })

vim.keymap.set("n", "<leader>ca", vim.lsp.buf.code_action,
    { desc = "Code actions" })

vim.keymap.set("n", "[d", vim.diagnostic.goto_prev,
    { desc = "Previous diagnostic" })

vim.keymap.set("n", "]d", vim.diagnostic.goto_next,
    { desc = "Next diagnostic" })

vim.keymap.set("n", "<leader>cd",
    vim.diagnostic.open_float,
    { desc = "Show error details" })

vim.keymap.set("n", "<leader>cl",
    vim.diagnostic.setloclist,
    { desc = "List diagnostics" })

-- ------------------------------------------
-- Bootstrap lazy.nvim
-- ------------------------------------------

local lazypath = vim.fn.stdpath("data")
    .. "/lazy/lazy.nvim"

if not vim.uv.fs_stat(lazypath) then
    vim.fn.system({
        "git", "clone", "--filter=blob:none",
        "https://github.com/folke/lazy.nvim.git",
        lazypath,
    })
end

vim.opt.rtp:prepend(lazypath)

require("lazy").setup({
    spec = {
        { import = "plugins" },
    },
    checker = {
        enabled = true,
        notify = false,
    },
})

-- ------------------------------------------
-- Project root on startup
-- ------------------------------------------

vim.api.nvim_create_autocmd("VimEnter", {
    callback = function()
        local arg = vim.fn.argv(0)

        if arg ~= "" and vim.fn.isdirectory(arg) == 1 then
            local root = vim.fn.fnamemodify(arg, ":p")
            vim.cmd.cd(vim.fn.fnameescape(root))

            local ok, api = pcall(require, "nvim-tree.api")
            if ok then
                api.tree.change_root(root)
            end
        end
    end,
})

-- Detect and optionally select a project Python environment
vim.api.nvim_create_autocmd("VimEnter", {
    group = vim.api.nvim_create_augroup(
        "PythonVirtualEnvironment",
        { clear = true }
    ),
    callback = function()
        require("config.python_venv").setup()
    end,
})
