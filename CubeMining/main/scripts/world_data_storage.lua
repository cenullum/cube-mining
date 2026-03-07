-- world_data_storage.lua
-- Thin proxy to C++ terrain engine for world data access.
-- All storage and lighting now lives in the native extension.

local M = {}
M.grid_size = 16

function M.init(grid_size, seed)
    M.grid_size = grid_size
    terrain.init(grid_size, seed or os.time())
end

function M.get_block(x, y, z)
    return terrain.get_block(x, y, z)
end

function M.set_block(x, y, z, id)
    terrain.set_block(x, y, z, id)
end

function M.get_lights(x, y, z)
    return terrain.get_lights(x, y, z)
end

function M.get_light(x, y, z, channel)
    local sun, source = terrain.get_lights(x, y, z)
    if channel == 1 then return sun else return source end
end

function M.set_light(x, y, z, val, channel)
    -- Lights are managed by C++ now, no-op
end

function M.recalculate_lighting()
    -- Lighting runs as part of the mesh update on the worker thread.
    -- For synchronous recalc (used before mesh request), call sync version:
    terrain.recalc_lighting_sync()
end

return M
