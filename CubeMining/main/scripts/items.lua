local hash = hash
local vmath = vmath

local M = {}

M.definitions = {
    -- Blocks (kept IDs 0-6 for voxel engine compatibility)
    -- id: Unique numeric identifier for the item/block
    -- name: Display name of the item
    -- type: "block" (voxel mesh), "voxel" (2D to 3D item), or "model" (3D prefab)
    -- is_placeable: Boolean, if true, the item can be placed into the world grid as a voxel
    -- faces: UV mapping for voxel faces (top, bottom, side) or {all}
    -- transparent: Boolean, if true, the block is considered non-opaque
    -- light_level: Emission level (0-15)
    -- light_color: Emission color (vmath.vector3)
    -- health: Durability of the block (mining hits required)
    -- hit_sound: Sound played when hitting the block
    -- break_sound: Sound played when the block is destroyed

    [0] = {
        name = "air",
        type = "block",
        transparent = true,
        is_placeable = false
    },
    [1] = {
        name = "stone",
        type = "block",
        is_placeable = true,
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
        type = "block",
        is_placeable = true,
        faces = {
            all = "unbreakable"
        },
        health = 9999,
        hit_sound = "hit"
    },
    [3] = {
        name = "golden_ore",
        type = "block",
        is_placeable = true,
        faces = {
            top = "golden_top",
            bottom = "golden_top",
            side = "golden_side"
        },
        health = 2,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [5] = {
        name = "grass",
        type = "block",
        is_placeable = true,
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
        type = "block",
        is_placeable = true,
        faces = {
            all = "dirt"
        },
        health = 1,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },

    -- Items (remapped to 10+ to avoid collision)
    -- power: Mining damage dealt per hit
    -- id: Hashed name for internal system lookup (animations/prefabs)

    [10] = {
        name = "pickaxe",
        type = "voxel",
        power = 1,
        id = hash("pickaxe"),
        is_placeable = false
    },
    [11] = {
        name = "torch",
        type = "model",
        power = 1,
        id = hash("torch"),
        is_placeable = true,
        transparent = true,
        light_level = 15,
        light_color = vmath and vmath.vector3(1.0, 0.9, 0.6) or nil,
        health = 1,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [12] = {
        name = "bomb",
        type = "model",
        power = 1,
        id = hash("bomb"),
        is_placeable = false
    },
    [13] = {
        name = "gun",
        type = "voxel",
        power = 1,
        id = hash("gun"),
        is_placeable = false
    },
    [14] = {
        name = "diamond_sword",
        type = "voxel",
        power = 2,
        id = hash("diamond_sword"),
        is_placeable = false
    }
}

return M
