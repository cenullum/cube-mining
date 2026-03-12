#include "ve_world.h"
#include <math.h>
#include <set>
#include <vector>
#include <algorithm>



// ─────────────────────────────────────────────────────────────────────────────
// CheckCollision: tests if the AABB overlaps any solid voxel.
// Epsilons are symmetric: a face exactly on a voxel boundary does NOT collide.
// ─────────────────────────────────────────────────────────────────────────────
bool CheckCollision(float min_x, float min_y, float min_z,
                    float max_x, float max_y, float max_z) {
    // Inset by a tiny epsilon so a face *exactly* on a voxel edge doesn't count.
    // This prevents the player from sticking when sliding along a flat wall.
    const float E = 0.001f;

    int x0 = (int)floorf(min_x + E + 0.5f),  x1 = (int)floorf(max_x - E + 0.5f);
    int y0 = (int)floorf(min_y + E + 0.5f),  y1 = (int)floorf(max_y - E + 0.5f);
    int z0 = (int)floorf(min_z + E + 0.5f),  z1 = (int)floorf(max_z - E + 0.5f);

    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++)
            for (int z = z0; z <= z1; z++)
                if (IsSolid(x, y, z)) return true;

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// SnapFloor: after a downward Y collision, snap the AABB bottom to the exact
// top surface of the voxel it landed on, eliminating float drift.
// ─────────────────────────────────────────────────────────────────────────────
static float SnapFloor(float feet_y) {
    // Top surface of block y is y + 0.5.
    // Index of the block below feet_y:
    int iy = (int)floorf(feet_y - 0.05f + 0.5f);
    return (float)iy + 0.5f;
}

// ─────────────────────────────────────────────────────────────────────────────
// SnapCeiling: after an upward Y collision, snap the AABB top to the ceiling.
// ─────────────────────────────────────────────────────────────────────────────
static float SnapCeiling(float feet_y, float height) {
    float head_grid_y = feet_y + height;
    // Bottom surface of block y is y - 0.5.
    // Index of the block above head:
    int iy = (int)floorf(head_grid_y + 0.05f + 0.5f);
    float snapped_head = (float)iy - 0.5f;
    return snapped_head - height;
}

// ─────────────────────────────────────────────────────────────────────────────
// TryStepUp: when walking into a wall, try to climb steps up to max_step_height.
// Returns true if the step was successful, and updates pos.
// This is the key technique to avoid snagging on voxel edges while walking.
// ─────────────────────────────────────────────────────────────────────────────
static bool TryStepUp(dmVMath::Vector3& pos, const dmVMath::Vector3& vel,
                      float half_x, float half_z, float height,
                      float dt_step, float max_step_height) {
    // Only attempt step-up for horizontal movement on the ground
    float dx = vel.getX() * dt_step;
    float dz = vel.getZ() * dt_step;
    if (fabsf(dx) < 1e-6f && fabsf(dz) < 1e-6f) return false;

    // Do not step up if we are visibly falling. Floor snapping handles landing.
    if (vel.getY() < -0.5f) return false;

    // Try stepping up in small increments
    const int STEP_SUBSTEPS = 4;
    for (int i = 1; i <= STEP_SUBSTEPS; i++) {
        float step_y = pos.getY() + max_step_height * ((float)i / STEP_SUBSTEPS);

        // Check if lifted position is clear (room for the player above the step)
        bool lifted_clear = !CheckCollision(
            pos.getX() - half_x, step_y, pos.getZ() - half_z,
            pos.getX() + half_x, step_y + height, pos.getZ() + half_z
        );
        if (!lifted_clear) continue;

        // Check if moved-forward position at lifted height is also clear
        bool forward_clear = !CheckCollision(
            pos.getX() + dx - half_x, step_y, pos.getZ() + dz - half_z,
            pos.getX() + dx + half_x, step_y + height, pos.getZ() + dz + half_z
        );
        if (!forward_clear) continue;

        // Check there is ground to stand on after the step
        bool has_ground = CheckCollision(
            pos.getX() + dx - half_x, step_y - 0.05f, pos.getZ() + dz - half_z,
            pos.getX() + dx + half_x, step_y,          pos.getZ() + dz + half_z
        );
        if (!has_ground) continue;

        // Step succeeded: lift and move forward
        pos.setY(step_y);
        pos.setX(pos.getX() + dx);
        pos.setZ(pos.getZ() + dz);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Depenetrate: if the player is already inside a voxel (e.g. from world edits
// or a previous frame bug), push them out along Y first, then X/Z.
// ─────────────────────────────────────────────────────────────────────────────
static void Depenetrate(dmVMath::Vector3& pos, const dmVMath::Vector3& size) {
    float hx = size.getX() * 0.5f;
    float hz = size.getZ() * 0.5f;
    float h  = size.getY();

    if (!CheckCollision(pos.getX()-hx, pos.getY(), pos.getZ()-hz,
                        pos.getX()+hx, pos.getY()+h, pos.getZ()+hz))
        return; // not stuck, nothing to do

    // Try pushing up in small increments (most common case: slightly in a floor)
    for (int i = 1; i <= 8; i++) {
        float try_y = pos.getY() + i * 0.125f;
        if (!CheckCollision(pos.getX()-hx, try_y, pos.getZ()-hz,
                            pos.getX()+hx, try_y+h, pos.getZ()+hz)) {
            pos.setY(try_y);
            return;
        }
    }

    // Fallback: try pushing in each cardinal direction
    const float offsets[4][2] = {{0.25f,0},{-0.25f,0},{0,0.25f},{0,-0.25f}};
    for (auto& o : offsets) {
        float tx = pos.getX() + o[0];
        float tz = pos.getZ() + o[1];
        if (!CheckCollision(tx-hx, pos.getY(), tz-hz,
                            tx+hx, pos.getY()+h, tz+hz)) {
            pos.setX(tx);
            pos.setZ(tz);
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveAndSlide: axis-separated AABB sweep with step-up, snapping & depenetration.
//
// Key differences from the old version:
//  - NO shrink buffers on any axis (they caused clipping into voxels)
//  - Position snapped to voxel boundaries on collision (stops float drift)
//  - Step-up for smooth traversal over single-voxel height changes
//  - Depenetration at the start of each frame
// ─────────────────────────────────────────────────────────────────────────────
void MoveAndSlide(dmVMath::Vector3& pos, dmVMath::Vector3& vel,
                  const dmVMath::Vector3& size, float dt, bool& is_grounded) {
    is_grounded = false;

    float hx = size.getX() * 0.5f;
    float hz = size.getZ() * 0.5f;
    float h  = size.getY();

    // Push the player out if they somehow ended up inside a solid voxel
    Depenetrate(pos, size);

    // Sub-stepping: prevents tunneling through thin walls at high speed
    float speed = dmVMath::Length(vel);
    int steps = (int)ceilf(speed * dt / 0.4f);
    if (steps < 1) steps = 1;
    if (steps > 20) steps = 20; // safety cap
    float dt_step = dt / (float)steps;

    for (int s = 0; s < steps; s++) {

        // ── X axis ──────────────────────────────────────────────────────────
        float dx = vel.getX() * dt_step;
        if (fabsf(dx) > 1e-7f) {
            float nx = pos.getX() + dx;
            if (CheckCollision(nx-hx, pos.getY(), pos.getZ()-hz,
                               nx+hx, pos.getY()+h, pos.getZ()+hz)) {
                // Try to step over a 1-voxel obstacle before giving up
                if (!TryStepUp(pos, vel, hx, hz, h, dt_step, 0.3f)) {
                    vel.setX(0);
                }
            } else {
                pos.setX(nx);
            }
        }

        // ── Z axis ──────────────────────────────────────────────────────────
        float dz = vel.getZ() * dt_step;
        if (fabsf(dz) > 1e-7f) {
            float nz = pos.getZ() + dz;
            if (CheckCollision(pos.getX()-hx, pos.getY(), nz-hz,
                               pos.getX()+hx, pos.getY()+h, nz+hz)) {
                if (!TryStepUp(pos, vel, hx, hz, h, dt_step, 0.3f)) {
                    vel.setZ(0);
                }
            } else {
                pos.setZ(nz);
            }
        }

        // ── Y axis ──────────────────────────────────────────────────────────
        float dy = vel.getY() * dt_step;
        if (fabsf(dy) > 1e-7f) {
            float ny = pos.getY() + dy;
            if (CheckCollision(pos.getX()-hx, ny, pos.getZ()-hz,
                               pos.getX()+hx, ny+h, pos.getZ()+hz)) {
                if (vel.getY() < 0) {
                    is_grounded = true;
                    // Snap feet to the exact top of the voxel below us.
                    // This eliminates the float drift that slowly pushes us into the floor.
                    pos.setY(SnapFloor(pos.getY()));
                } else {
                    // Hit ceiling: snap head down
                    pos.setY(SnapCeiling(pos.getY(), h));
                }
                vel.setY(0);
            } else {
                pos.setY(ny);
            }
        }
    }

    // ── Ground proximity check (no Y movement this frame but standing still) ──
    if (!is_grounded && vel.getY() <= 0) {
        const float GROUND_PROBE = 0.05f;
        if (CheckCollision(pos.getX()-hx, pos.getY()-GROUND_PROBE, pos.getZ()-hz,
                           pos.getX()+hx, pos.getY(),              pos.getZ()+hz)) {
            is_grounded = true;
            vel.setY(0);
        }
    }
}

bool RayAABBIntersection(const dmVMath::Vector3& ray_origin, const dmVMath::Vector3& ray_dir, 
                               const dmVMath::Vector3& box_min, const dmVMath::Vector3& box_max, float& t_out) {
    float t1 = (box_min.getX() - ray_origin.getX()) / (ray_dir.getX() + 1e-6f);
    float t2 = (box_max.getX() - ray_origin.getX()) / (ray_dir.getX() + 1e-6f);
    float t3 = (box_min.getY() - ray_origin.getY()) / (ray_dir.getY() + 1e-6f);
    float t4 = (box_max.getY() - ray_origin.getY()) / (ray_dir.getY() + 1e-6f);
    float t5 = (box_min.getZ() - ray_origin.getZ()) / (ray_dir.getZ() + 1e-6f);
    float t6 = (box_max.getZ() - ray_origin.getZ()) / (ray_dir.getZ() + 1e-6f);

    float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
    float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));

    if (tmax < 0 || tmin > tmax) return false;
    t_out = (tmin < 0) ? tmax : tmin;
    return true;
}

int Lua_Explosion(lua_State* L) {
    dmVMath::Vector3 center = *dmScript::ToVector3(L, 1);
    float radius = (float)luaL_checknumber(L, 2);
    float base_damage = (float)luaL_checknumber(L, 3);

    int min_x = (int)floorf(center.getX() - radius + 0.5f);
    int max_x = (int)floorf(center.getX() + radius + 0.5f);
    int min_y = (int)floorf(center.getY() - radius + 0.5f);
    int max_y = (int)floorf(center.getY() + radius + 0.5f);
    int min_z = (int)floorf(center.getZ() - radius + 0.5f);
    int max_z = (int)floorf(center.getZ() + radius + 0.5f);

    std::set<uint64_t> touched_chunks;
    float r_sq = radius * radius;
    bool world_modified = false;

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                float dx = (float)x - center.getX();
                float dy = (float)y - center.getY();
                float dz = (float)z - center.getZ();
                if (dx*dx + dy*dy + dz*dz <= r_sq) {
                    if (GetBlock(x, y, z) != 0) {
                        SetBlock(x, y, z, 0); world_modified = true;
                        uint64_t key = ((uint64_t)(x >> 4 & 0xFFFF) << 32) | ((uint64_t)(y >> 4 & 0xFFFF) << 16) | (uint64_t)(z >> 4 & 0xFFFF);
                        touched_chunks.insert(key);
                    }
                }
            }
        }
    }

    lua_newtable(L);
    int nidx = 1;
    for (auto& npc : g_npcs) {
        if (npc.is_dead) continue;
        dmVMath::Vector3 to_npc = npc.pos - center;
        if (dmVMath::Length(to_npc) <= radius + 1.0f) {
            float dist_f = fminf(dmVMath::Length(to_npc) / radius, 1.0f);
            float damage = base_damage * (1.0f - dist_f * 0.75f);
            dmVMath::Vector3 kb = dmVMath::Normalize(dmVMath::Vector3(to_npc.getX(), 0, to_npc.getZ())) * (damage * 0.3f) + dmVMath::Vector3(0, 10.0f, 0);
            npc.health -= damage; npc.vel = npc.vel + kb;
            dmMessage::URL receiver; dmMessage::ResetURL(&receiver); receiver.m_Socket = npc.socket; receiver.m_Path = dmGameObject::GetIdentifier(npc.instance);
            dmMessage::Post(0, &receiver, dmHashString64("damaged"), 0, 0, 0, 0, 0, 0);
            lua_newtable(L); dmScript::PushHash(L, npc.id); lua_setfield(L, -2, "id"); lua_pushnumber(L, damage); lua_setfield(L, -2, "damage"); dmScript::PushVector3(L, kb); lua_setfield(L, -2, "kb_dir"); lua_rawseti(L, -2, nidx++);
        }
    }

    lua_newtable(L);
    int cidx = 1;
    for (uint64_t key : touched_chunks) {
        lua_newtable(L); lua_pushinteger(L, (int)(key >> 32 & 0xFFFF)); lua_setfield(L, -2, "x"); lua_pushinteger(L, (int)(key >> 16 & 0xFFFF)); lua_setfield(L, -2, "y"); lua_pushinteger(L, (int)(key & 0xFFFF)); lua_setfield(L, -2, "z"); lua_rawseti(L, -2, cidx++);
    }

    if (world_modified) {
        TriggerAsyncMeshUpdate();
    }
    return 2;
}

int Lua_ShootRay(lua_State* L) {
    dmVMath::Vector3 origin = *dmScript::ToVector3(L, 1);
    dmVMath::Vector3 dir = *dmScript::ToVector3(L, 2);
    float max_dist = (float)luaL_checknumber(L, 3);
    int penetration = (int)luaL_checkinteger(L, 4);
    float damage = (float)luaL_optnumber(L, 5, 0);
    float kb_power = (float)luaL_optnumber(L, 6, 0);

    struct Hit { float dist; uint64_t npc_id; bool is_block; dmVMath::Vector3 pos; dmVMath::Vector3 normal; };
    std::vector<Hit> hits;

    dmVMath::Vector3 ray_grid = origin;
    dmVMath::Vector3 step(dir.getX() > 0 ? 1 : -1, dir.getY() > 0 ? 1 : -1, dir.getZ() > 0 ? 1 : -1);
    dmVMath::Vector3 delta(fabsf(1.0f / (dir.getX() + 1e-9f)), fabsf(1.0f / (dir.getY() + 1e-9f)), fabsf(1.0f / (dir.getZ() + 1e-9f)));
    int ix = (int)floorf(ray_grid.getX() + 0.5f), iy = (int)floorf(ray_grid.getY() + 0.5f), iz = (int)floorf(ray_grid.getZ() + 0.5f);
    dmVMath::Vector3 next_t(dir.getX() > 0 ? (ix + 0.5f - ray_grid.getX()) * delta.getX() : (ray_grid.getX() - (ix - 0.5f)) * delta.getX(),
                           dir.getY() > 0 ? (iy + 0.5f - ray_grid.getY()) * delta.getY() : (ray_grid.getY() - (iy - 0.5f)) * delta.getY(),
                           dir.getZ() > 0 ? (iz + 0.5f - ray_grid.getZ()) * delta.getZ() : (ray_grid.getZ() - (iz - 0.5f)) * delta.getZ());

    float dist = 0;
    dmVMath::Vector3 norm(0, 0, 0);
    while (dist < max_dist) {
        if (GetBlock(ix, iy, iz) != 0) {
            hits.push_back({dist, 0, true, dmVMath::Vector3((float)ix, (float)iy, (float)iz), norm});
            break;
        }
        if (next_t.getX() < next_t.getY() && next_t.getX() < next_t.getZ()) {
            dist = next_t.getX();
            next_t.setX(next_t.getX() + delta.getX());
            ix += (int)step.getX();
            norm = dmVMath::Vector3(-step.getX(), 0, 0);
        } else if (next_t.getY() < next_t.getZ()) {
            dist = next_t.getY();
            next_t.setY(next_t.getY() + delta.getY());
            iy += (int)step.getY();
            norm = dmVMath::Vector3(0, -step.getY(), 0);
        } else {
            dist = next_t.getZ();
            next_t.setZ(next_t.getZ() + delta.getZ());
            iz += (int)step.getZ();
            norm = dmVMath::Vector3(0, 0, -step.getZ());
        }
    }

    for (const auto& npc : g_npcs) {
        if (npc.is_dead) continue;
        float t_hit;
        dmVMath::Vector3 half = npc.size * 0.5f;
        if (RayAABBIntersection(origin, dir, npc.pos + dmVMath::Vector3(-half.getX(), 0, -half.getZ()), npc.pos + dmVMath::Vector3(half.getX(), npc.size.getY(), half.getZ()), t_hit)) {
            if (t_hit < max_dist) {
                bool obscured = false;
                for (auto& h : hits) if (h.is_block && h.dist < t_hit) obscured = true;
                if (!obscured) {
                    hits.push_back({t_hit, npc.id, false, npc.pos, dmVMath::Vector3(0,0,0)});
                    if (damage > 0) {
                        for (auto& n : g_npcs) if (n.id == npc.id) {
                            n.health -= damage; if (kb_power > 0) n.vel = n.vel + dir * kb_power + dmVMath::Vector3(0, 1.5f, 0);
                            dmMessage::URL rec; dmMessage::ResetURL(&rec); rec.m_Socket = n.socket; rec.m_Path = dmGameObject::GetIdentifier(n.instance); dmMessage::Post(0, &rec, dmHashString64("damaged"), 0,0,0,0,0,0);
                            break;
                        }
                    }
                }
            }
        }
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.dist < b.dist; });
    lua_newtable(L);
    int hcount = 0;
    for (auto& h : hits) {
        lua_newtable(L);
        lua_pushboolean(L, h.is_block); lua_setfield(L, -2, "is_block");
        lua_pushnumber(L, h.dist); lua_setfield(L, -2, "dist");
        dmScript::PushVector3(L, h.pos); lua_setfield(L, -2, "pos");
        dmScript::PushVector3(L, h.normal); lua_setfield(L, -2, "normal");
        if (!h.is_block) { dmScript::PushHash(L, h.npc_id); lua_setfield(L, -2, "id"); }
        lua_rawseti(L, -2, ++hcount);
        if (h.is_block || hcount > penetration) break;
    }
    return 1;
}

int Lua_MoveAndSlide(lua_State* L) {
    dmVMath::Vector3 pos = *dmScript::ToVector3(L, 1), vel = *dmScript::ToVector3(L, 2), size = *dmScript::ToVector3(L, 3);
    float dt = (float)luaL_checknumber(L, 4);
    bool grounded = false;
    MoveAndSlide(pos, vel, size, dt, grounded);
    dmScript::PushVector3(L, pos); dmScript::PushVector3(L, vel); lua_pushboolean(L, grounded);
    return 3;
}

int Lua_CheckCollision(lua_State* L) {
    float x1=luaL_checknumber(L, 1), y1=luaL_checknumber(L, 2), z1=luaL_checknumber(L, 3), x2=luaL_checknumber(L, 4), y2=luaL_checknumber(L, 5), z2=luaL_checknumber(L, 6);
    lua_pushboolean(L, CheckCollision(x1,y1,z1,x2,y2,z2));
    return 1;
}
