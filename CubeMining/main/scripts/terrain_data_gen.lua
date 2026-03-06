local M          = {}

-- Localize globals for performance
local math_floor = math.floor
local math_abs   = math.abs

-- Permutation table (256 entries, doubled to avoid index wrapping)
local p          = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10,
    23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56,
    87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122,
    60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76,
    132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217,
    226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28,
    42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253,
    19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12,
    191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176,
    115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
}
for i = 1, 256 do p[256 + i] = p[i] end

-- Fully inlined Perlin noise — no sub-function call overhead
-- Returns a value roughly in [-1, 1]
function M.perlin(x, y, z)
    local xf  = math_floor(x)
    local yf  = math_floor(y)
    local zf  = math_floor(z)

    -- Cell coords (1-based, mod 256)
    local X   = xf % 256 + 1
    local Y   = yf % 256 + 1
    local Z   = zf % 256 + 1

    -- Fractional part
    x         = x - xf
    y         = y - yf
    z         = z - zf

    -- Fade curves (inline)
    local u   = x * x * x * (x * (x * 6 - 15) + 10)
    local v   = y * y * y * (y * (y * 6 - 15) + 10)
    local w   = z * z * z * (z * (z * 6 - 15) + 10)

    -- Hash coords — cache table lookups
    local pX  = p[X]
    local pX1 = p[X + 1]
    local A   = pX + Y
    local B   = pX1 + Y
    local AA  = p[A] + Z
    local AB  = p[A + 1] + Z
    local BA  = p[B] + Z
    local BB  = p[B + 1] + Z

    -- Inline grad(hash, x, y, z)
    local function g(hash, gx, gy, gz)
        local h = hash % 16
        local gu = h < 8 and gx or gy
        local gv = h < 4 and gy or (h == 12 or h == 14) and gx or gz
        return ((h % 2 == 0) and gu or -gu) + ((math_floor(h * 0.5) % 2 == 0) and gv or -gv)
    end

    -- Trilinear interpolation (inline lerp)
    local x1   = x - 1
    local y1   = y - 1
    local z1   = z - 1

    local pAA  = p[AA]; local pBA = p[BA]
    local pAB  = p[AB]; local pBB = p[BB]
    local pAA1 = p[AA + 1]; local pBA1 = p[BA + 1]
    local pAB1 = p[AB + 1]; local pBB1 = p[BB + 1]

    local leg1 = g(pAA, x, y, z)
    local leg2 = g(pBA, x1, y, z)
    local leg3 = g(pAB, x, y1, z)
    local leg4 = g(pBB, x1, y1, z)
    local leg5 = g(pAA1, x, y, z1)
    local leg6 = g(pBA1, x1, y, z1)
    local leg7 = g(pAB1, x, y1, z1)
    local leg8 = g(pBB1, x1, y1, z1)

    -- lerp(w, lerp(v, lerp(u,…),…), lerp(v, lerp(u,…),…))
    local lx1  = leg1 + u * (leg2 - leg1)
    local lx2  = leg3 + u * (leg4 - leg3)
    local lx3  = leg5 + u * (leg6 - leg5)
    local lx4  = leg7 + u * (leg8 - leg7)

    local ly1  = lx1 + v * (lx2 - lx1)
    local ly2  = lx3 + v * (lx4 - lx3)

    return ly1 + w * (ly2 - ly1)
end

-- Configuration
local SEED         = 12345
local WORLD_HEIGHT = 64
local SCALE        = 0.05
local PERSISTENCE  = 0.5
local OCTAVES      = 3

-- Pre-computed seed offsets (recomputed on set_seed)
local SEED_X_OFFSET, SEED_Z_OFFSET, SEED_ORE_Y, SEED_Y_FREQ

local function recompute_seed_offsets()
    SEED_X_OFFSET = SEED * 0.132
    SEED_Z_OFFSET = SEED * 0.941
    SEED_ORE_Y    = SEED * 0.42
    SEED_Y_FREQ   = SEED * 0.42 -- same constant, kept for clarity
end
recompute_seed_offsets()

function M.set_seed(seed)
    SEED = seed
    recompute_seed_offsets()
end

function M.get_seed()
    return SEED
end

-- Localize M.perlin for hot paths
local perlin = M.perlin

--- Get ground height for a column (x, z).
-- Call once per column, then reuse the result for every block in that column.
function M.get_ground_height(x, z)
    local nx         = (x + SEED_X_OFFSET) * SCALE
    local nz         = (z + SEED_Z_OFFSET) * SCALE

    local height_sum = 0
    local freq       = 1
    local ampl       = 1
    local max_value  = 0

    for _ = 1, OCTAVES do
        height_sum = height_sum + perlin(nx * freq, SEED_ORE_Y, nz * freq) * ampl
        max_value  = max_value + ampl
        ampl       = ampl * PERSISTENCE
        freq       = freq * 2
    end

    -- Normalise [-1,1] → [0, WORLD_HEIGHT]
    local norm = (height_sum / max_value + 1) * 0.5
    return math_floor(norm * WORLD_HEIGHT)
end

--- Return block ID for world position (x, y, z).
-- Pass a pre-computed ground_height to avoid redundant column sampling.
--
-- Block IDs:
--   0  Air
--   1  Stone
--   2  Bedrock
--   3  Golden Ore
--   5  Grass
--   6  Dirt
function M.get_block_for_pos(x, y, z, ground_height)
    if y < 0 then return 0 end

    ground_height = ground_height or M.get_ground_height(x, z)

    if y > ground_height then
        return 0 -- Air above surface
    end

    if y == ground_height then
        return (y < 10) and 1 or 5 -- Stone beach / Grass
    elseif y > ground_height - 4 then
        return 6                   -- Dirt sub-surface
    elseif y == 0 then
        return 2                   -- Bedrock
    else
        -- Deep stone — only sample ore noise when actually needed
        local nx    = (x + SEED_X_OFFSET) * SCALE
        local nz    = (z + SEED_Z_OFFSET) * SCALE
        local ore_n = perlin(nx * 2.0, y * 0.5, nz * 2.0)
        return (ore_n > 0.6) and 3 or 1 -- Golden Ore or Stone
    end
end

--- Populate a chunk with terrain data
function M.generate_chunk(cx, cy, cz)
    local world = require "main.scripts.world_data_storage"
    local chunk = world.get_chunk(cx, cy, cz, true)

    local start_x, start_y, start_z = cx * 16, cy * 16, cz * 16
    local chunk_size = 16

    -- Optimize: only compute ground height once per x,z column per chunk
    for lx = 0, chunk_size - 1 do
        local gx = start_x + lx
        for lz = 0, chunk_size - 1 do
            local gz = start_z + lz
            local ground_h = M.get_ground_height(gx, gz)
            for ly = 0, chunk_size - 1 do
                local gy = start_y + ly
                local block_id = M.get_block_for_pos(gx, gy, gz, ground_h)
                local idx = lx + ly * chunk_size + lz * (chunk_size * chunk_size) + 1
                chunk.data[idx] = block_id
            end
        end
    end

    chunk.modified = true
end

return M
