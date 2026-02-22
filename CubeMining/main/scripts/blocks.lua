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
            top = "stone_top",
            bottom = "stone_top",
            side = "stone_side"
        },
        health = 1,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [2] = {
        name = "unbreakable",
        faces = {
            all = "unbreakable"
        },
        health = 9999,
        hit_sound = "hit"
    },
    [3] = {
        name = "golden_ore",
        faces = {
            top = "golden_top",
            bottom = "golden_top",
            side = "golden_side"
        },
        health = 2,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [4] = {
        name = "torch",
        transparent = true,
        light_level = 15,
        light_color = vmath.vector3(1.0, 0.9, 0.6),
        health = 1,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [5] = {
        name = "grass",
        faces = {
            top = "grass_top",
            bottom = "dirt",
            side = "grass_side"
        },
        health = 1,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [6] = {
        name = "dirt",
        faces = {
            all = "dirt"
        },
        health = 1,
        hit_sound = "hit",
        break_sound = "stone_debris"
    }
}

return M
