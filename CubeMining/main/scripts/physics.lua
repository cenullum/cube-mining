-- physics.lua
-- Shared physics logic for AABB collision detection and resolution against a voxel world.

local world = require "main.scripts.world"
local block_data = require "main.scripts.block_data"

local M = {}

-- Ensure vmath is available (it is global in Defold)
local vmath = vmath

-- Helper to check if a block at (x,y,z) is solid
local function is_solid(x, y, z)
    local id = world.get_block(x, y, z)
    if id == 0 then return false end
    local def = block_data.get_block(id)
    return def and def.solid
end

--- Axis Aligned Bounding Box check
-- Returns true if the box defined by min/max overlaps with any solid block
local function check_collision(min_x, min_y, min_z, max_x, max_y, max_z)
    -- World Transform Adjustments matching generate_voxel.script
    local offset   = -world.grid_size / 2 + 0.5
    local origin_x = offset
    local origin_y = offset
    local origin_z = 490 -- Hardcoded in generate_voxel.script

    -- Convert world AABB to voxel grid indices
    -- Voxel Index k corresponds to world position: origin + k
    -- Block bounds: [origin + k - 0.5, origin + k + 0.5]
    -- To find range of K:
    -- start_k = floor((min_world - origin) + 0.5)
    -- end_k   = floor((max_world - origin) + 0.5)

    local start_x  = math.floor(min_x - origin_x + 0.5)
    local end_x    = math.floor(max_x - origin_x + 0.5)

    local start_y  = math.floor(min_y - origin_y + 0.5)
    local end_y    = math.floor(max_y - origin_y + 0.5)

    local start_z  = math.floor(min_z - origin_z + 0.5)
    local end_z    = math.floor(max_z - origin_z + 0.5)

    for x = start_x, end_x do
        for y = start_y, end_y do
            for z = start_z, end_z do
                if is_solid(x, y, z) then
                    return true
                end
            end
        end
    end
    return false
end

M.check_collision = check_collision

--- Resolve movement using sub-stepped AABB checks (Swept approximation)
---@param position vector3 Current position (bottom-center)
---@param velocity vector3 Current velocity
---@param size vector3 Size of the AABB
---@param dt number Delta time
---@return vector3 new_position
---@return vector3 new_velocity
---@return boolean is_grounded
function M.move_and_slide(position, velocity, size, dt)
    local pos = vmath.vector3(position)
    local vel = vmath.vector3(velocity)
    local is_grounded = false

    -- Sub-stepping to prevent tunneling
    -- Max step size should be less than block size (1.0).
    -- 0.4 ensures we don't skip over thin obstacles or corners too easily.
    local step_size = 0.4
    local total_dist = vmath.length(vel * dt)
    local steps = math.ceil(total_dist / step_size)
    if steps < 1 then steps = 1 end

    local dt_step = dt / steps

    for s = 1, steps do
        local collided_any = false

        -- X Axis
        local dx = vel.x * dt_step
        if dx ~= 0 then
            local next_pos_x = pos.x + dx
            if check_collision(next_pos_x - size.x * 0.5, pos.y, pos.z - size.z * 0.5,
                    next_pos_x + size.x * 0.5, pos.y + size.y, pos.z + size.z * 0.5) then
                vel.x = 0
            else
                pos.x = next_pos_x
            end
        end

        -- Z Axis
        local dz = vel.z * dt_step
        if dz ~= 0 then
            local next_pos_z = pos.z + dz
            if check_collision(pos.x - size.x * 0.5, pos.y, next_pos_z - size.z * 0.5,
                    pos.x + size.x * 0.5, pos.y + size.y, next_pos_z + size.z * 0.5) then
                vel.z = 0
            else
                pos.z = next_pos_z
            end
        end

        -- Y Axis
        local dy = vel.y * dt_step
        if dy ~= 0 then
            local next_pos_y = pos.y + dy
            if check_collision(pos.x - size.x * 0.5, next_pos_y, pos.z - size.z * 0.5,
                    pos.x + size.x * 0.5, next_pos_y + size.y, pos.z + size.z * 0.5) then
                if vel.y < 0 then is_grounded = true end
                vel.y = 0
            else
                pos.y = next_pos_y
            end
        end
    end

    -- Final ground check for stationary or small movements
    if not is_grounded and vel.y <= 0 then
        local check_dist = 0.05
        if check_collision(pos.x - size.x * 0.5 + 0.05, pos.y - check_dist, pos.z - size.z * 0.5 + 0.05,
                pos.x + size.x * 0.5 - 0.05, pos.y, pos.z + size.z * 0.5 - 0.05) then
            is_grounded = true
            -- Ensure velocity is zero if we are grounded (fixes micro-sliding)
            vel.y = 0
        end
    end

    return pos, vel, is_grounded
end

return M
