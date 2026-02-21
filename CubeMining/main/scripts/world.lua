-- world.lua
-- Global module to store and access voxel data.
-- This allows physics and other systems to access the world state without message passing.

local block_data = require "main.scripts.block_data"

local M = {}

-- The 3D array of block IDs and light levels
-- Accessed as M.data[x][y][z]
M.data = {}
M.sun_light = {}
M.source_light = {} -- Block-based light sources (torches, glowstones, etc.)

-- Grid size of the world (assumed cubic for now)
M.grid_size = 16

--- Initialize the world data structure
---@param grid_size number
function M.init(grid_size)
    M.grid_size = grid_size
    M.data = {}
    M.sun_light = {}
    M.source_light = {}
    for x = 0, grid_size - 1 do
        M.data[x] = {}
        M.sun_light[x] = {}
        M.source_light[x] = {}
        for y = 0, grid_size - 1 do
            M.data[x][y] = {}
            M.sun_light[x][y] = {}
            M.source_light[x][y] = {}
            for z = 0, grid_size - 1 do
                M.data[x][y][z] = 0       -- Default to Air
                M.sun_light[x][y][z] = 15 -- Default sun is fully bright at top, will settle later
                M.source_light[x][y][z] = 0
            end
        end
    end
end

--- Get a block ID at the specified coordinates.
--- Returns 0 (Air) if out of bounds.
---@param x number
---@param y number
---@param z number
---@return number block_id
function M.get_block(x, y, z)
    if x < 0 or x >= M.grid_size or
        y < 0 or y >= M.grid_size or
        z < 0 or z >= M.grid_size then
        return 0
    end
    -- Check for nil to be safe, though init should prevent it
    if not M.data[x] or not M.data[x][y] then return 0 end
    return M.data[x][y][z] or 0
end

--- Set a block ID at the specified coordinates.
---@param x number
---@param y number
---@param z number
---@param id number
function M.set_block(x, y, z, id)
    if x >= 0 and x < M.grid_size and
        y >= 0 and y < M.grid_size and
        z >= 0 and z < M.grid_size then
        if not M.data[x] then M.data[x] = {} end
        if not M.data[x][y] then M.data[x][y] = {} end
        M.data[x][y][z] = id
    end
end

--- Get both light levels at once (optimized for NPC/Entity lighting).
---@return number sun_light, number source_light
function M.get_lights(x, y, z)
    if x < 0 or x >= M.grid_size or
        y < 0 or y >= M.grid_size or
        z < 0 or z >= M.grid_size then
        return 15, 0
    end
    local sun = M.sun_light[x][y][z] or 15
    local source = M.source_light[x][y][z] or 0
    return sun, source
end

--- Get a light level (0 = source, 1 = sun).
function M.get_light(x, y, z, channel)
    if x < 0 or x >= M.grid_size or
        y < 0 or y >= M.grid_size or
        z < 0 or z >= M.grid_size then
        return channel == 1 and 15 or 0
    end
    if channel == 1 then
        return M.sun_light[x][y][z] or 15
    else
        return M.source_light[x][y][z] or 0
    end
end

--- Set a light level (0 = source, 1 = sun).
function M.set_light(x, y, z, val, channel)
    if x >= 0 and x < M.grid_size and
        y >= 0 and y < M.grid_size and
        z >= 0 and z < M.grid_size then
        if channel == 1 then
            M.sun_light[x][y][z] = val
        else
            M.source_light[x][y][z] = val
        end
    end
end

--- Recalculate lighting for the entire grid
function M.recalculate_lighting()
    local s = M.grid_size
    local source_q = {}
    local sun_q = {}

    -- 1. Reset Lights and Seed Queues
    for x = 0, s - 1 do
        for y = 0, s - 1 do
            for z = 0, s - 1 do
                M.source_light[x][y][z] = 0
                M.sun_light[x][y][z] = 0

                local id = M.data[x][y][z]
                local def = block_data.get_block(id)
                if def and def.light_level > 0 then
                    M.source_light[x][y][z] = def.light_level
                    table.insert(source_q, { x = x, y = y, z = z })
                end
            end
        end
    end

    -- 2. Direct Sunlight (Top-Down Pass)
    -- We assume the sun shines straight down initially (y = s-1 to 0).
    for x = 0, s - 1 do
        for z = 0, s - 1 do
            local light = 15
            for y = s - 1, 0, -1 do
                local id = M.data[x][y][z]
                local def = block_data.get_block(id)
                if def and not def.transparent then
                    light = 0 -- Blocked by an opaque block
                end
                M.sun_light[x][y][z] = light
                if light > 0 then
                    table.insert(sun_q, { x = x, y = y, z = z })
                end
            end
        end
    end

    -- Directions for BFS
    local dirs = {
        { 1, 0, 0 }, { -1, 0, 0 },
        { 0, 1, 0 }, { 0, -1, 0 },
        { 0, 0, 1 }, { 0, 0, -1 }
    }

    -- 3. BFS Propagation for Block Light (Source Light)
    local idx = 1
    while idx <= #source_q do
        local node = source_q[idx]
        idx = idx + 1
        local light = M.source_light[node.x][node.y][node.z]
        if light > 1 then
            for _, d in ipairs(dirs) do
                local nx, ny, nz = node.x + d[1], node.y + d[2], node.z + d[3]
                if nx >= 0 and nx < s and ny >= 0 and ny < s and nz >= 0 and nz < s then
                    local nid = M.data[nx][ny][nz]
                    local ndef = block_data.get_block(nid)
                    -- If transparent and the new light is stronger
                    if (not ndef or ndef.transparent) and M.source_light[nx][ny][nz] < light - 1 then
                        M.source_light[nx][ny][nz] = light - 1
                        table.insert(source_q, { x = nx, y = ny, z = nz })
                    end
                end
            end
        end
    end

    -- 4. BFS Propagation for Sun Light (sideways and down under overhangs)
    idx = 1
    while idx <= #sun_q do
        local node = sun_q[idx]
        idx = idx + 1
        local light = M.sun_light[node.x][node.y][node.z]
        if light > 1 then
            for _, d in ipairs(dirs) do
                local nx, ny, nz = node.x + d[1], node.y + d[2], node.z + d[3]
                if nx >= 0 and nx < s and ny >= 0 and ny < s and nz >= 0 and nz < s then
                    local nid = M.data[nx][ny][nz]
                    local ndef = block_data.get_block(nid)
                    if not ndef or ndef.transparent then
                        -- Sun light only decreases by 1
                        local next_light = light - 1
                        if M.sun_light[nx][ny][nz] < next_light then
                            M.sun_light[nx][ny][nz] = next_light
                            table.insert(sun_q, { x = nx, y = ny, z = nz })
                        end
                    end
                end
            end
        end
    end
end

return M
