
-- Snapix: screenshot keybindings for Hyprland
-- Requires Hyprland 0.55+ Lua configuration.

local snapix = "snapix"

-- Print: full-screen screenshot
hl.bind("PRINT", function()
    hl.exec_cmd(snapix .. " --full")
end)

-- Ctrl + Print: full-screen screenshot and copy
hl.bind("CTRL + PRINT", function()
    hl.exec_cmd(snapix .. " --full --copy")
end)

-- Ctrl + Shift + Print: select region and copy
hl.bind("CTRL + SHIFT + PRINT", function()
    hl.exec_cmd(snapix .. " --select --copy")
end)

-- Shift + Print: select region and save
hl.bind("SHIFT + PRINT", function()
    hl.exec_cmd(snapix .. " --select")
end)
