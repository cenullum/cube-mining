-- world.lua
-- Global module to store and access voxel data.
-- This allows physics and other systems to access the world state without message passing.

local M = {}

-- The 3D array of block IDs.
-- Accessed as M.data[x][y][z]
M.data = {}

-- Grid size of the world (assumed cubic for now)
M.grid_size = 16

--- Initialize the world data structure
---@param grid_size number
function M.init(grid_size)
    M.grid_size = grid_size
    M.data = {}
    for x = 0, grid_size - 1 do
        M.data[x] = {}
        for y = 0, grid_size - 1 do
            M.data[x][y] = {}
            for z = 0, grid_size - 1 do
                M.data[x][y][z] = 0 -- Default to Air
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

return M
