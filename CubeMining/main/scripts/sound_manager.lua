-- sound_manager.lua
-- Centralized sound controller for Cube Mining.
-- Optimized for lookup performance, spatial awareness, and resource management.

-- Cache engine modules and functions for high-frequency access (Upvalues)
local sound = sound
local hash = hash
local msg = msg
local vmath = vmath
local math = math
local os = os
local type = type
local tostring = tostring
local factory = factory

local vmath_length = vmath.length
local math_min = math.min
local math_random = math.random
local os_clock = os.clock

local M = {}

-- Cache for sound URLs to avoid repeated msg.url() calls
local url_cache = {}

-- Map of sound IDs to their component names on the sounds object
local SOUND_ROOT = "/sounds"

-- Expose hashes directly on the module for optimization (avoid table lookups)
M.bomb = hash("bomb")
M.swing = hash("swing")
M.hit = hash("hit")
M.stone_debris = hash("stone_debris")
M.gun_holster = hash("gun_holster")
M.pistol_shot = hash("pistol_shot")
M.pistol_reload = hash("pistol_reload")
M.submachine_gun_shot = hash("submachine_gun_shot")
M.submachine_gun_reload = hash("submachine_gun_reload")
M.place = hash("place")
M.jumping = hash("jumping")
M.landing = hash("landing")

-- Internal reverse lookup for URL construction
local REVERSE_NAMES = {
    [M.bomb] = "bomb",
    [M.swing] = "swing",
    [M.hit] = "hit",
    [M.stone_debris] = "stone_debris",
    [M.gun_holster] = "gun_holster",
    [M.pistol_shot] = "pistol_shot",
    [M.pistol_reload] = "pistol_reload",
    [M.submachine_gun_shot] = "submachine_gun_shot",
    [M.submachine_gun_reload] = "submachine_gun_reload",
    [M.place] = "place",
    [M.jumping] = "jumping",
    [M.landing] = "landing"
}

-- Throttling: Prevent the same sound from playing too frequently.
local sound_cooldowns = {}
local MIN_COOLDOWN = 0.04 -- 40ms between same sound triggers

-- Dynamic footstep sounds separated by material
local footstep_sounds = {
    stone = {
        "stone_footstep-01", "stone_footstep-02", "stone_footstep-03", "stone_footstep-04", "stone_footstep-05",
        "stone_footstep-06", "stone_footstep-07", "stone_footstep-08", "stone_footstep-09", "stone_footstep-10"
    },
    grass = {
        "grass_footstep-01", "grass_footstep-02", "grass_footstep-03", "grass_footstep-04", "grass_footstep-05",
        "grass_footstep-06", "grass_footstep-07", "grass_footstep-08", "grass_footstep-09", "grass_footstep-10"
    },
    dirt = {
        "dirt_footstep-01", "dirt_footstep-02", "dirt_footstep-03", "dirt_footstep-04", "dirt_footstep-08",
        "dirt_footstep_-01", "dirt_footstep_-04", "dirt_footstep_-06", "dirt_footstep_-08", "dirt_footstep_-09"
    }
}

-- Keep track of the last played footstep for each material to avoid immediate repetition
local last_footstep = {}

--- Registers new footstep sounds for a specific material dynamically
---@param material string
---@param sound_list table Array of sound component names
function M.register_footsteps(material, sound_list)
    footstep_sounds[material] = sound_list
end

--- Plays a dynamic footstep sound based on material
---@param material string|nil The block material (defaults to "stone")
---@param position vector3 world position of the feet for 3D sound
---@param gain number|nil Optional volume
function M.play_footstep(material, position, gain)
    material = material or "stone"
    local sounds = footstep_sounds[material] or footstep_sounds["stone"]
    
    if not sounds or #sounds == 0 then return end
    
    local sound_name
    if #sounds > 1 then
        local index = math_random(1, #sounds)
        sound_name = sounds[index]
        if sound_name == last_footstep[material] then
            index = (index % #sounds) + 1
            sound_name = sounds[index]
        end
    else
        sound_name = sounds[1]
    end
    
    last_footstep[material] = sound_name
    
    -- Speed variation gives slightly varying pitch to each footstep
    M.play(sound_name, gain or 0.8, position, nil, 0.15)
end


--- Plays a sound by its ID with throttling, spatial support, and pitch variation.
---@param sound_id string|hash The name of the sound component (e.g., "bomb", "swing")
---@param gain number|nil Optional volume gain (0.0 to 1.0), default 1.0
---@param position vector3|nil Optional world position for 3D spatial effect
---@param complete_callback function|nil Optional callback when sound finishes
---@param speed_variation number|nil Optional random pitch range (e.g. 0.1). Default 0.1
function M.play(sound_id, gain, position, complete_callback, speed_variation)
    local s_hash = type(sound_id) == "string" and hash(sound_id) or sound_id

    -- 1. Throttling Check
    local now = os_clock()
    local last_play = sound_cooldowns[s_hash] or 0
    if now - last_play < MIN_COOLDOWN then return end

    -- 2. Spatial Calculation
    local final_gain = gain or 1.0
    if position and _G.cam_pos then
        local dist = vmath_length(position - _G.cam_pos)
        local max_dist = 25.0
        local falloff = 1.0 - math_min(1.0, dist / max_dist)
        falloff = falloff * falloff
        final_gain = final_gain * falloff
        if final_gain < 0.02 then return end
    end

    -- 3. Speed/Pitch variation
    local var = speed_variation
    if var == nil then var = 0.1 end
    local final_speed = 1.0
    if var > 0 then
        final_speed = 1.0 + (math_random() * 2 - 1) * var
    end

    -- 4. URL Retrieval (Cached)
    local url = url_cache[s_hash]
    if not url then
        local name = REVERSE_NAMES[s_hash] or (type(sound_id) == "string" and sound_id or ("sound_" .. tostring(s_hash)))
        url = msg.url(nil, SOUND_ROOT, name)
        url_cache[s_hash] = url
    end

    -- 5. Execute
    sound.play(url, { gain = final_gain, speed = final_speed }, complete_callback)
    sound_cooldowns[s_hash] = now
end

--- Dynamic Sound Spawning (Factory Pool)
--- Spawns a dedicated emitter GO that plays once and deletes itself.
--- This allows for infinite overlapping sounds.
---@param sound_id hash The base sound ID (for throttling)
---@param factory_url url The factory component to spawn from
function M.play_pooled(sound_id, factory_url)
    if not factory_url or factory_url.path == hash("") then
        M.play(sound_id)
        return
    end

    -- Throttling at 20ms to prevent extreme spam
    local now = os_clock()
    if now - (sound_cooldowns[sound_id] or 0) < 0.02 then return end

    -- Create at player position
    factory.create(factory_url, _G.cam_pos or vmath.vector3())
    sound_cooldowns[sound_id] = now
end

return M
