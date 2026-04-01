-- base_npc.lua
local M = {}
local go = go
local vmath = vmath
local msg = msg
local math = math
local hash = hash
local sm = require "main.scripts.sound_manager"

local H_DRAW_LINE = hash("draw_line")
local COLOR_DEBUG = vmath.vector4(1, 1, 0, 1)

local function draw_debug_aabb(self)
    if _G.performance_mode ~= 2 then return end

    local pos = go.get_position(self.id)
    local sx, sy, sz = self.size.x, self.size.y, self.size.z
    local hx, hz = sx * 0.5, sz * 0.5

    local x0, x1 = pos.x - hx, pos.x + hx
    local y0, y1 = pos.y, pos.y + sy
    local z0, z1 = pos.z - hz, pos.z + hz

    -- 8 corners
    local p1 = vmath.vector3(x0, y0, z0)
    local p2 = vmath.vector3(x1, y0, z0)
    local p3 = vmath.vector3(x1, y0, z1)
    local p4 = vmath.vector3(x0, y0, z1)
    local p5 = vmath.vector3(x0, y1, z0)
    local p6 = vmath.vector3(x1, y1, z0)
    local p7 = vmath.vector3(x1, y1, z1)
    local p8 = vmath.vector3(x0, y1, z1)

    -- Bottom
    msg.post("@render:", H_DRAW_LINE, { start_point = p1, end_point = p2, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p2, end_point = p3, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p3, end_point = p4, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p4, end_point = p1, color = COLOR_DEBUG })
    -- Top
    msg.post("@render:", H_DRAW_LINE, { start_point = p5, end_point = p6, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p6, end_point = p7, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p7, end_point = p8, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p8, end_point = p5, color = COLOR_DEBUG })
    -- Verticals
    msg.post("@render:", H_DRAW_LINE, { start_point = p1, end_point = p5, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p2, end_point = p6, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p3, end_point = p7, color = COLOR_DEBUG })
    msg.post("@render:", H_DRAW_LINE, { start_point = p4, end_point = p8, color = COLOR_DEBUG })
end


function M.init(self)
    self.id = go.get_id()
    self.model_url = msg.url(nil, self.id, "model")

    self.velocity = vmath.vector3()
    -- Allow scripts to override size before calling init, otherwise default
    self.size = self.size or vmath.vector3(0.6, 1.5, 0.6)

    self.is_dead = false

    -- Optimized Lighting & State Tint Update
    self.light_update_frame = math.random(1, 10)
    self.frame_counter = 0
    self.damage_flash_timer = 0
    self.ambient_light = voxel_engine.get_ambient_light(go.get_position(self.id))

    -- Register with C++ engine directly
    if voxel_engine and voxel_engine.register_npc then
        voxel_engine.register_npc(self.id, self.id, go.get_position(), self.size, false, {
            state = 1,
            timer = 0,
            state_duration = math.random(4, 6),
            speed = self.speed or 3.0,
            gravity = self.gravity or -25.0,
            jump_force = self.jump_force or 8.0,
            rotation_offset_y = self.rotation_offset_y or 0.0,
            health = 100, -- Default health
            socket = msg.url().socket
        })
    end

    pcall(function() go.set(self.model_url, "tint", self.ambient_light) end)
    local position = go.get_position(self.id)
    pcall(function() go.set(self.model_url, "position", position) end)
    sm.play(sm.swing, 1.0, go.get_position())
end

function M.final(self)
    voxel_engine.unregister_npc(self.id)
end

function M.die(self)
    if self.is_dead then return end
    self.is_dead = true
    self.death_timer = 0

    pcall(function() go.set(self.model_url, "tint", vmath.vector4(0.35, 0.35, 0.35, 1.0)) end)
    local rot = go.get_rotation()
    local target_rot = rot * vmath.quat_rotation_x(math.rad(-90))
    go.animate(".", "rotation", go.PLAYBACK_ONCE_FORWARD, target_rot, go.EASING_OUTBOUNCE, 0.5)
end

function M.on_damage(self)
    if self.is_dead then return end
    self.damage_flash_timer = 0.5 -- 0.5s flash
end

function M.on_message(self, message_id, message, sender)
    if message_id == hash("died") then
        M.die(self)
    elseif message_id == hash("damaged") then
        M.on_damage(self)
    end
end

function M.update(self, dt)
    if self.is_dead then
        self.death_timer = self.death_timer + dt
        if self.death_timer >= 2.0 then go.delete() end
        return -- Death tint fixed in M.die
    end

    local needs_tint_update = false

    -- Handle Damage Flash Timer
    if self.damage_flash_timer > 0 then
        self.damage_flash_timer = self.damage_flash_timer - dt
        needs_tint_update = true
        if self.damage_flash_timer <= 0 then
            self.damage_flash_timer = 0
        end
    end

    -- Handle Environmental Light (periodic staggered update)
    self.frame_counter = self.frame_counter + 1
    if self.frame_counter >= 10 then
        self.frame_counter = 0
        local pos = go.get_position(self.id)
        local new_ambient = voxel_engine.get_ambient_light(pos)
        if new_ambient ~= self.ambient_light then
            self.ambient_light = new_ambient
            needs_tint_update = true
        end
    end

    -- Apply Tint Priority: Damage Flash > Ambient Light
    if needs_tint_update then
        local final_tint = self.ambient_light
        if self.damage_flash_timer > 0 then
            local t = self.damage_flash_timer / 0.5
            -- Flash red (override ambient)
            final_tint = vmath.vector4(2.0 * t + final_tint.x * (1 - t),
                0.4 * t + final_tint.y * (1 - t),
                0.4 * t + final_tint.z * (1 - t), 1.0)
        end
        pcall(function() go.set(self.model_url, "tint", final_tint) end)
    end

    draw_debug_aabb(self)
end

return M
