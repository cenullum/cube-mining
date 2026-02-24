-- npc.lua
local physics = require "main.scripts.physics"
local world = require "main.scripts.world"

local vmath = vmath
local msg = msg
local go = go
local math = math
local os = os
local hash = hash
local timer = timer
local gui = gui

local H_DRAW_LINE = hash("draw_line")
local H_TINT = hash("tint")

local STATE_IDLE = 1
local STATE_WALK = 2
local STATE_DEAD = 3

local M = {}

-- Global registry for optimized shooting lookups
M.registry = {}

function M.init(self)
    self.id = go.get_id()
    self.model_url = msg.url(nil, self.id, "model")

    self.velocity = vmath.vector3()
    self.size = vmath.vector3(0.6, 1.5, 0.6) -- Default size

    self.state = STATE_IDLE
    self.timer = 0
    self.state_duration = math.random(4, 6)
    self.move_dir = vmath.vector3()

    -- Health system
    self.health = 100
    self.max_health = 100
    self.is_dead = false

    -- Visual feedback state
    self.tint_timer = 0
    self.knockback_velocity = vmath.vector3()

    -- Register this NPC for shooting system
    M.registry[go.get_id()] = self

    -- Random seed based on time and object ID
    local id_str = tostring(go.get_id())
    local result = 0
    for i = 1, #id_str do
        result = result + string.byte(id_str, i)
    end
    math.randomseed(os.time() + result)
    math.random() -- Warm up

    self.cliff_check_timer = 0
end

function M.final(self)
    -- Remove from registry when deleted
    M.registry[go.get_id()] = nil
end

function M.take_damage(self, damage, knockback_dir)
    if self.is_dead then return end

    self.health = self.health - damage

    -- Apply knockback: current velocity + knockback
    -- Upward force doubled as requested (10 instead of 5)
    local knockback_force = knockback_dir + vmath.vector3(0, 10, 0)
    self.velocity = self.velocity + knockback_force

    -- Trigger red tint for 2 seconds
    self.tint_timer = 2.0

    if self.health <= 0 then
        self.health = 0
        self.is_dead = true
        self.state = STATE_DEAD

        -- Remove from registry immediately so shooting logic skips it
        M.registry[self.id] = nil

        -- Death animation: Tip over 90 degrees
        -- Use explicit ID to avoid animating the caller (camera)
        local rot = go.get_rotation(self.id)
        local target_rot = rot * vmath.quat_rotation_x(math.rad(-90))
        go.animate(self.id, "rotation", go.PLAYBACK_ONCE_FORWARD, target_rot, go.EASING_OUTBOUNCE, 0.5)

        -- Delete NPC after 2 seconds
        -- ID is already captured in self.id
        local deletion_id = self.id
        timer.delay(2, false, function()
            if pcall(function() go.get_position(deletion_id) end) then
                go.delete(deletion_id)
            end
        end)
    end
end

local function pick_random_direction()
    local angle = math.random() * math.pi * 2
    return vmath.vector3(math.cos(angle), 0, math.sin(angle))
end

function M.update(self, dt)
    if self.is_dead then return end

    self.timer = self.timer + dt
    if self.cliff_check_timer > 0 then
        self.cliff_check_timer = self.cliff_check_timer - dt
    end

    -- AI Decision Logic
    if self.timer >= self.state_duration then
        self.timer = 0

        if self.state == STATE_IDLE then
            -- Switch to Walk
            self.state = STATE_WALK
            self.state_duration = math.random(1, 3)
            self.move_dir = pick_random_direction()
        else
            -- Switch to Idle
            self.state = STATE_IDLE
            self.state_duration = math.random(4, 6)
            self.move_dir = vmath.vector3(0, 0, 0)
        end
    end

    -- Apply Movement
    local target_vel_x = 0
    local target_vel_z = 0

    if self.state == STATE_WALK then
        target_vel_x = self.move_dir.x * self.speed
        target_vel_z = self.move_dir.z * self.speed

        -- Rotate towards movement direction
        if vmath.length_sqr(self.move_dir) > 0.01 then
            local angle = math.atan2(self.move_dir.x, self.move_dir.z)
            -- Apply rotation offset (some models are rotated differently)
            -- 90 degrees offset as requested for mouse
            local rot_offset_y = self.rotation_offset_y or 0
            local rot = vmath.quat_rotation_y(angle + math.rad(rot_offset_y))
            go.set_rotation(rot, self.id)
        end
    end

    -- Smooth acceleration
    self.velocity.x = vmath.lerp(0.1, self.velocity.x, target_vel_x)
    self.velocity.z = vmath.lerp(0.1, self.velocity.z, target_vel_z)

    -- Gravity
    self.velocity.y = self.velocity.y + self.gravity * dt

    local pos = go.get_position()

    -- Physics Step
    local new_pos, new_vel, grounded = physics.move_and_slide(pos, self.velocity, self.size, dt)

    -- Cliff Avoidance & Smart Jump Logic
    if self.state == STATE_WALK and grounded then
        local dir = vmath.normalize(vmath.vector3(self.move_dir.x, 0, self.move_dir.z))
        local check_dist = 0.6

        -- 1. CLIFF CHECK: Check if there's ground in front of us
        -- We check a small area ahead and 2 blocks below to define a "cliff".
        -- A 1-block drop is fine, 2 or more is a cliff.
        local ground_check_min = pos + dir * check_dist + vmath.vector3(-0.1, -1.6, -0.1)
        local ground_check_max = pos + dir * (check_dist + 0.2) + vmath.vector3(0.1, -0.1, 0.1)

        local has_ground = physics.check_collision(ground_check_min.x, ground_check_min.y, ground_check_min.z,
            ground_check_max.x, ground_check_max.y, ground_check_max.z)

        if not has_ground then
            -- Cliff ahead (2+ block drop)! Turn 180 degrees back
            self.move_dir = -self.move_dir
            self.timer = 0
            new_vel.x = 0
            new_vel.z = 0
        else
            -- 2. JUMP CHECK: Only if we are grounded and there's ground ahead
            local intended_speed = self.speed
            local current_speed = vmath.length(vmath.vector3(new_vel.x, 0, new_vel.z))

            if current_speed < intended_speed * 0.2 then
                -- Wall detected or stuck

                -- Check for a 1-block high obstacle specifically
                -- A: Block at feet level + slightly ahead (the obstacle)
                local obs_check_min = pos + dir * 0.5 + vmath.vector3(-0.1, 0.1, -0.1)
                local obs_check_max = pos + dir * 0.7 + vmath.vector3(0.1, 0.9, 0.1)

                -- B: Block above the obstacle (the clearance check)
                local clear_check_min = pos + dir * 0.5 + vmath.vector3(-0.1, 1.1, -0.1)
                local clear_check_max = pos + dir * 0.7 + vmath.vector3(0.1, 1.9, 0.1)

                local is_blocked = physics.check_collision(obs_check_min.x, obs_check_min.y, obs_check_min.z,
                    obs_check_max.x, obs_check_max.y, obs_check_max.z)
                local is_clear = not physics.check_collision(clear_check_min.x, clear_check_min.y, clear_check_min.z,
                    clear_check_max.x, clear_check_max.y, clear_check_max.z)

                if is_blocked and is_clear then
                    -- 1-block obstacle detected. Can we jump over?
                    local jump_height_blocks = math.ceil(self.size.y)
                    local head_min = pos + vmath.vector3(-self.size.x / 2 + 0.1, self.size.y, -self.size.z / 2 + 0.1)
                    local head_max = pos +
                        vmath.vector3(self.size.x / 2 - 0.1, self.size.y + jump_height_blocks, self.size.z / 2 - 0.1)

                    if not physics.check_collision(head_min.x, head_min.y, head_min.z, head_max.x, head_max.y, head_max.z) then
                        self.velocity.y = self.jump_force
                        new_vel.y = self.jump_force
                    else
                        self.move_dir = pick_random_direction()
                        self.timer = 0
                    end
                else
                    -- Not a jumpable obstacle or too high. Change direction.
                    self.move_dir = pick_random_direction()
                    self.timer = 0
                end
            end
        end
    end

    self.velocity = new_vel
    go.set_position(new_pos, self.id)

    -- Per-NPC block light sampling
    local grid_size = _G.grid_size or 16
    local grid_offset = -grid_size / 2 + 0.5
    local npc_gx = math.floor(new_pos.x - grid_offset + 0.5)
    local npc_gy = math.floor(new_pos.y - grid_offset + 0.5)
    local npc_gz = math.floor(new_pos.z - 490 + 0.5)

    -- 1. Check if we even need to recalculate base light (block changed or mode changed)
    local light_changed = npc_gx ~= self.last_npc_gx or npc_gy ~= self.last_npc_gy or npc_gz ~= self.last_npc_gz or
        _G.light_mode ~= self.last_light_mode
    if light_changed then
        self.last_npc_gx, self.last_npc_gy, self.last_npc_gz = npc_gx, npc_gy, npc_gz
        self.last_light_mode = _G.light_mode

        if _G.light_mode == 2 then
            self.base_npc_tint = vmath.vector4(1, 1, 1, 1)
        else
            local sun_l, source_l = world.get_lights(npc_gx, npc_gy, npc_gz)
            local sun_f = sun_l / 15.0
            local source_f = (source_l / 15.0) * 1.5
            self.base_npc_tint = vmath.vector4(
                math.max(0.02, sun_f + source_f * 1.0),
                math.max(0.02, sun_f + source_f * 0.9),
                math.max(0.02, sun_f + source_f * 0.6),
                1.0
            )
        end
    end

    -- 2. Handle tint fade (this MUST be outside the if light_changed block)
    local current_base = self.base_npc_tint or vmath.vector4(1, 1, 1, 1)
    local final_tint = vmath.vector4(current_base)

    if self.tint_timer > 0 then
        local t = self.tint_timer / 2.0 -- 2s max duration
        self.tint_timer = self.tint_timer - dt
        if self.tint_timer < 0 then self.tint_timer = 0 end

        -- Fade logic: Lerp between red-tint and the base light tint
        local red_tint = vmath.vector4(1.0, current_base.y * 0.3, current_base.z * 0.3, 1.0)
        final_tint = vmath.lerp(t, current_base, red_tint)
    end

    if final_tint ~= self.last_tint then
        self.last_tint = final_tint
        pcall(function() go.set(self.model_url, "tint", final_tint) end)
    end

    -- 3. Fog color caching
    if _G.fog_color and _G.fog_color ~= self.last_fog_color then
        self.last_fog_color = _G.fog_color
        pcall(function() go.set(self.model_url, "fog_color", _G.fog_color) end)
    end

    -- Debug Drawing
    if _G.performance_mode == 2 then
        local pos = go.get_position()
        local half_size = self.size * 0.5
        local min = pos + vmath.vector3(-half_size.x, 0, -half_size.z)
        local max = pos + vmath.vector3(half_size.x, self.size.y, half_size.z)
        local color = vmath.vector4(1, 1, 0, 1) -- Yellow for NPC

        -- Draw box edges
        local function draw_line(p1, p2)
            msg.post("@render:", H_DRAW_LINE, { start_point = p1, end_point = p2, color = color })
        end

        local p1 = vmath.vector3(min.x, min.y, min.z)
        local p2 = vmath.vector3(max.x, min.y, min.z)
        local p3 = vmath.vector3(max.x, min.y, max.z)
        local p4 = vmath.vector3(min.x, min.y, max.z)
        local p5 = vmath.vector3(min.x, max.y, min.z)
        local p6 = vmath.vector3(max.x, max.y, min.z)
        local p7 = vmath.vector3(max.x, max.y, max.z)
        local p8 = vmath.vector3(min.x, max.y, max.z)

        draw_line(p1, p2); draw_line(p2, p3); draw_line(p3, p4); draw_line(p4, p1)
        draw_line(p5, p6); draw_line(p6, p7); draw_line(p7, p8); draw_line(p8, p5)
        draw_line(p1, p5); draw_line(p2, p6); draw_line(p3, p7); draw_line(p4, p8)
    end
end

return M
