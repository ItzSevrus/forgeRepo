
local M = {}

local function find_venv(start_dir)
    local dir = vim.fs.normalize(start_dir)
    local home = vim.fs.normalize(vim.fn.expand("~"))

    while true do
        for _, name in ipairs({
            ".venv",
            "venv",
            "env",
            ".env",
        }) do
            local candidate = dir .. "/" .. name
            local interpreter = candidate .. "/bin/python"

            if vim.fn.executable(interpreter) == 1 then
                return candidate, interpreter
            end

            interpreter = candidate .. "/bin/python3"

            if vim.fn.executable(interpreter) == 1 then
                return candidate, interpreter
            end
        end

        if dir == home or dir == "/" then
            break
        end

        local parent = vim.fs.dirname(dir)
        if not parent or parent == dir then
            break
        end

        dir = parent
    end

    return nil, nil
end

function M.setup()
    local root = vim.fn.getcwd()
    local venv, interpreter = find_venv(root)

    if not venv then
        return
    end

    vim.ui.select(
        { "Yes, use this environment", "No, keep current environment" },
        {
            prompt = "Python virtual environment found: "
                .. vim.fn.fnamemodify(venv, ":~")
                .. ". Use it?",
        },
        function(choice)
            if choice ~= "Yes, use this environment" then
                return
            end

            -- Set the environment for Neovim and its child processes.
            vim.env.VIRTUAL_ENV = venv
            vim.env.PATH = venv .. "/bin:" .. vim.env.PATH

            -- Tell Pyright which interpreter to use.
            vim.lsp.config("pyright", {
                settings = {
                    python = {
                        pythonPath = interpreter,
                        analysis = {
                            typeCheckingMode = "basic",
                            autoSearchPaths = true,
                            useLibraryCodeForTypes = true,
                            diagnosticMode = "workspace",
                        },
                    },
                },
            })

            -- Update an already-running Pyright client.
            for _, client in ipairs(
                vim.lsp.get_clients({ name = "pyright" })
            ) do
                client.settings = client.settings or {}
                client.settings.python = client.settings.python or {}
                client.settings.python.pythonPath = interpreter

                client:notify(
                    "workspace/didChangeConfiguration",
                    { settings = client.settings }
                )
            end

            vim.notify(
                "Python environment selected: "
                    .. vim.fn.fnamemodify(venv, ":~"),
                vim.log.levels.INFO
            )
        end
    )
end

return M

