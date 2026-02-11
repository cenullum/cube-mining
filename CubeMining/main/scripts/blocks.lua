-- blocks.lua
-- Centralized block definitions.
-- format: [ID] = { name, faces = { top, bottom, side } } or { name, faces = { all } }

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
    }
}

return M
