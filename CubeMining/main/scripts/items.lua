local hash = hash
local vmath = vmath

local M = {}

M.definitions = {
    -- Blocks (kept IDs 0-6 for voxel engine compatibility)
    -- id: Unique numeric identifier for the item/block
    -- name: Display name of the item
    -- type: "block", "tool", "gun" (logical type)
    -- visual: "block" (voxel mesh), "voxel" (2D to 3D item), or "model" (3D prefab)
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
        visual = "block",
        type = "block",
        transparent = true,
        is_placeable = false
    },
    [1] = {
        name = "stone",
        visual = "block",
        type = "block",
        is_placeable = true,
        faces = {
            top = "stone_top",
            bottom = "stone_top",
            side = "stone_side"
        },
        health = 10,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [2] = {
        name = "unbreakable",
        visual = "block",
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
        visual = "block",
        type = "block",
        is_placeable = true,
        faces = {
            top = "golden_top",
            bottom = "golden_top",
            side = "golden_side"
        },
        health = 15,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [5] = {
        name = "grass",
        visual = "block",
        type = "block",
        is_placeable = true,
        faces = {
            top = "grass_top",
            bottom = "dirt",
            side = "grass_side"
        },
        health = 5,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [6] = {
        name = "dirt",
        visual = "block",
        type = "block",
        is_placeable = true,
        faces = {
            all = "dirt"
        },
        health = 5,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },

    -- Items (remapped to 10+ to avoid collision)
    -- mining_power: Damage dealt to blocks per hit
    -- damage_power: Damage dealt to entities (future-proofing)
    -- id: Hashed name for internal system lookup (animations/prefabs)

    [12] = {
        name = "torch",
        visual = "model",
        type = "tool",
        mining_power = 1,
        damage_power = 1,
        is_placeable = true,
        transparent = true,
        light_level = 15,
        light_color = vmath and vmath.vector3(1.0, 0.9, 0.6) or nil,
        health = 1,
        hit_sound = "hit",
        break_sound = "stone_debris"
    },
    [13] = {
        name = "bomb",
        visual = "model",
        type = "tool",
        mining_power = 1,
        damage_power = 1,
        is_placeable = false
    },
    [14] = {
        name = "pistol",
        visual = "voxel",
        type = "gun",
        mining_power = 1,
        damage_power = 10,
        recoil_power = 1,
        is_placeable = false,
        ammo_max = 7,
        fire_rate = 0.2,
        reload_duration = 1.0,
        penetration = 2,
        shot_sound = "pistol_shot",
        reload_sound = "pistol_reload",
        holster_sound = "gun_holster"
    },
    [15] = {
        name = "iron_pickaxe",
        visual = "voxel",
        type = "tool",
        mining_power = 2,
        damage_power = 2,
        is_placeable = false
    },
    [16] = {
        name = "golden_pickaxe",
        visual = "voxel",
        type = "tool",
        mining_power = 3,
        damage_power = 2,
        is_placeable = false
    },
    [17] = {
        name = "diamond_pickaxe",
        visual = "voxel",
        type = "tool",
        mining_power = 5,
        damage_power = 3,
        is_placeable = false
    },
    [18] = {
        name = "iron_sword",
        visual = "voxel",
        type = "tool",
        mining_power = 1,
        damage_power = 4,
        is_placeable = false
    },
    [19] = {
        name = "golden_sword",
        visual = "voxel",
        type = "tool",
        mining_power = 1,
        damage_power = 5,
        is_placeable = false
    },
    [20] = {
        name = "diamond_sword",
        visual = "voxel",
        type = "tool",
        mining_power = 2,
        damage_power = 7,
        is_placeable = false
    },
    [21] = {
        name = "submachine_gun",
        visual = "voxel",
        type = "gun",
        mining_power = 1,
        damage_power = 20,
        recoil_power = 2,
        is_placeable = false,
        ammo_max = 30,
        fire_rate = 0.1,
        is_automatic = true,
        reload_duration = 2.0,
        penetration = 0,
        shot_sound = "submachine_gun_shot",
        reload_sound = "submachine_gun_reload",
        holster_sound = "gun_holster"
    }
}

-- Post-process definitions to ensure 'id' (hash) and 'name' are properly handled
-- Users can use either ID or hashed name for lookups
for id, def in pairs(M.definitions) do
    if not def.id then
        def.id = hash(def.name)
    end
end

return M
