-- block_data.lua
-- Utility module to manage and validate block data.
-- Pre-calculates UVs for optimized runtime access.

local blocks = require "main.scripts.blocks"

local hash = hash
local resource = resource

local M = {}

-- Cache for processed block data indexing by ID
local block_cache = {}

-- Directions mapping for mesh generation
local D_Z_POS = 1
local D_Z_NEG = 2
local D_Y_POS = 3
local D_Y_NEG = 4
local D_X_POS = 5
local D_X_NEG = 6

---@param atlas table Defold atlas resource data
---@param texture_info table Texture dimensions
local function get_uv_data(atlas, texture_info, animation_name)
    local animation_id = hash(animation_name)
    local geometry_id = nil

    for i, anim in ipairs(atlas.animations) do
        local anim_id = type(anim.id) == "string" and hash(anim.id) or anim.id
        if anim_id == animation_id then
            geometry_id = i
            break
        end
    end

    if not geometry_id then
        print("WARNING: Texture '" .. animation_name .. "' not found in atlas!")
        return nil
    end

    local geom = atlas.geometries[geometry_id]
    local min_u, max_u, min_v, max_v = math.huge, -math.huge, math.huge, -math.huge
    for i = 1, #geom.uvs, 2 do
        local u, v = geom.uvs[i], geom.uvs[i + 1]
        if u < min_u then min_u = u end
        if u > max_u then max_u = u end
        if v < min_v then min_v = v end
        if v > max_v then max_v = v end
    end

    return {
        u = min_u / texture_info.width,
        v = (texture_info.height - max_v) / texture_info.height,
        w = (max_u - min_u) / texture_info.width,
        h = (max_v - min_v) / texture_info.height
    }
end

--- Initializes the block library, validates textures, and caches UVs.
---@param atlas_res_id hash Atlas resource hash
function M.init(atlas_res_id)
    local atlas = resource.get_atlas(atlas_res_id)
    local texture_info = resource.get_texture_info(atlas.texture)

    block_cache = {}

    for id, def in pairs(blocks.definitions) do
        if def.transparent then
            block_cache[id] = { transparent = true, name = def.name }
        else
            local faces = def.faces
            local uv_map = {}

            local function assign_face(dir, tex_name)
                local uv = get_uv_data(atlas, texture_info, tex_name)
                if not uv then
                    print("ERROR: Block '" .. def.name .. "' (ID " .. id .. ") missing texture: " .. tex_name)
                    uv = { u = 0, v = 0, w = 0, h = 0 }
                end
                uv_map[dir] = uv
            end

            if faces.all then
                for d = 1, 6 do assign_face(d, faces.all) end
            else
                -- Check for missing face definitions
                local required = { "top", "bottom", "side" }
                for _, r in ipairs(required) do
                    if not faces[r] then
                        print("WARNING: Block '" .. def.name .. "' (ID " .. id .. ") missing face definition for: " .. r)
                    end
                end

                assign_face(D_Y_POS, faces.top or "top")
                assign_face(D_Y_NEG, faces.bottom or "top")

                -- Sides
                local side_tex = faces.side or "side"
                assign_face(D_Z_POS, side_tex)
                assign_face(D_Z_NEG, side_tex)
                assign_face(D_X_POS, side_tex)
                assign_face(D_X_NEG, side_tex)
            end

            block_cache[id] = {
                name = def.name,
                uvs = uv_map,
                transparent = false
            }
        end
    end
end

--- Get pre-calculated block data by ID.
---@param id number
---@return table|nil
function M.get_block(id)
    return block_cache[id]
end

return M
