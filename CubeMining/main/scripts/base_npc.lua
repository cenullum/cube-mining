-- base_npc.lua
local M = {}
local go = go
local vmath = vmath
local msg = msg
local math = math
local hash = hash
local sm = require "main.scripts.sound_manager"


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
    self.ambient_light = terrain.get_ambient_light(go.get_position(self.id))

    -- Register with C++ engine directly
    if terrain and terrain.register_npc then
        terrain.register_npc(self.id, self.id, go.get_position(), self.size, false, {
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

    sm.play(sm.swing, 1.0, go.get_position())
end

function M.final(self)
    terrain.unregister_npc(self.id)
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
        local new_ambient = terrain.get_ambient_light(pos)
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
end

return M
