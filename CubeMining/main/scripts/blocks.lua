-- blocks.lua
-- Centralized block definitions.
-- format: [ID] = { name, faces = { top, bottom, side } } or { name, faces = { all } }

local vmath = vmath
local M = {}

M.definitions = {
    [0] = {
        name = "air",
        transparent = true
    },
    [1] = {
        name = "stone",
        faces = {
            top = "top",
            bottom = "top",
            side = "side"
        }
    },
    [2] = {
        name = "unbreakable",
        faces = {
            all = "unbreakable"
        }
    },
    [3] = {
        name = "golden_ore",
        faces = {
            top = "golden_top",
            bottom = "golden_top",
            side = "golden_side"
        }
    },
    [4] = {
        name = "torch",
        transparent = true,
        light_level = 15,
        light_color = vmath.vector3(1.0, 0.9, 0.6)
    }
}

return M
