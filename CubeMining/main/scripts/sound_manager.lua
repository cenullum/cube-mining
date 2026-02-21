-- sound_manager.lua
-- Centralized sound controller for Cube Mining.
-- Optimized for lookup performance, spatial awareness, and resource management.

-- Cache engine modules and functions for high-frequency access (Upvalues)
local sound = sound
local hash = hash
local msg = msg
local vmath = vmath
local math = math
local vmath = vmath
local math = math
local os = os
local socket = socket -- Keep for compatibility if needed, but we'll use os.clock
local type = type
local tostring = tostring
local pairs = pairs

local vmath_length = vmath.length
local math_min = math.min
local os_clock = os.clock

local M = {}

-- Cache for sound URLs to avoid repeated msg.url() calls
local url_cache = {}

-- Map of sound IDs to their component names on the sounds object
local SOUND_ROOT = "/sounds"

-- Pre-hashed sound names for faster comparison
local HASHES = {
    bomb = hash("bomb"),
    swing = hash("swing"),
    hit = hash("hit"),
    stone_debris = hash("stone_debris")
}

-- Throttling: Prevent the same sound from playing too frequently in a single frame.
-- This prevents "loudness spikes" and engine overload during mass events (like explosions).
local sound_cooldowns = {}
local MIN_COOLDOWN = 0.05 -- 50ms between same sound triggers

--- Plays a sound by its ID with throttling and spatial support.
---@param sound_id string|hash The name of the sound component (e.g., "bomb", "swing")
---@param gain number|nil Optional volume gain (0.0 to 1.0)
---@param position vector3|nil Optional world position for 3D spatial effect
function M.play(sound_id, gain, position)
    local s_hash = type(sound_id) == "string" and hash(sound_id) or sound_id

    -- 1. Throttling Check
    local now = os_clock()
    local last_play = sound_cooldowns[s_hash] or 0
    if now - last_play < MIN_COOLDOWN then
        return -- Skip redundant sound trigger
    end

    -- 2. Spatial Calculation (Early Out)
    local final_gain = gain or 1.0
    if position and _G.cam_pos then
        local dist = vmath_length(position - _G.cam_pos)
        local max_dist = 25.0

        -- Linear falloff with squared curve for natural feel
        local falloff = 1.0 - math_min(1.0, dist / max_dist)
        falloff = falloff * falloff

        final_gain = final_gain * falloff

        -- If too quiet to hear, discard early before expensive URL lookup
        if final_gain < 0.02 then return end
    end

    -- 3. URL Retrieval (Cached)
    local url = url_cache[s_hash]
    if not url then
        local name = nil
        for key, h in pairs(HASHES) do
            if h == s_hash then
                name = key
                break
            end
        end

        name = name or (type(sound_id) == "string" and sound_id or ("sound_" .. tostring(s_hash)))
        url = msg.url(nil, SOUND_ROOT, name)
        url_cache[s_hash] = url
    end

    -- 4. Execute
    sound.play(url, { gain = final_gain })
    sound_cooldowns[s_hash] = now
end

-- Expose hashes for direct use
M.HASHES = HASHES

return M
