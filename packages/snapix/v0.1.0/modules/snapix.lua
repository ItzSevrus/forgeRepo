-- Snapix screenshot keybindings
-- Loaded from ~/.config/hypr/hyprland.lua

local snapix = "/home/sahil/.local/bin/snapix"

hl.bind("Print", hl.dsp.exec_cmd(snapix .. " --full"))

hl.bind("CTRL + Print", hl.dsp.exec_cmd(snapix .. " --full --copy"))

hl.bind("CTRL + SHIFT + Print", hl.dsp.exec_cmd(snapix .. " --select --copy"))

hl.bind("SHIFT + Print", hl.dsp.exec_cmd(snapix .. " --select"))
