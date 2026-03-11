#include "ve_world.h"
#include <math.h>
#include <set>
#include <vector>
#include <algorithm>

bool CheckCollision(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z) {
    float offset = (float)g_grid_size / -2.0f + 0.5f;
    float origin_x = offset;
    float origin_y = offset;
    float origin_z = 490.0f;

    // Add small epsilon to avoid being stuck on boundaries
    int start_x = (int)floorf(min_x - origin_x + 0.501f);
    int end_x   = (int)floorf(max_x - origin_x + 0.499f);
    int start_y = (int)floorf(min_y - origin_y + 0.501f);
    int end_y   = (int)floorf(max_y - origin_y + 0.499f);
    int start_z = (int)floorf(min_z - origin_z + 0.501f);
    int end_z   = (int)floorf(max_z - origin_z + 0.499f);

    for (int x = start_x; x <= end_x; x++) {
        for (int y = start_y; y <= end_y; y++) {
            for (int z = start_z; z <= end_z; z++) {
                if (IsSolid(x, y, z)) return true;
            }
        }
    }
    return false;
}

void MoveAndSlide(dmVMath::Vector3& pos, dmVMath::Vector3& vel, const dmVMath::Vector3& size, float dt, bool& is_grounded) {
    is_grounded = false;
    float step_size = 0.4f;
    float total_dist = dmVMath::Length(vel * dt);
    int steps = (int)ceilf(total_dist / step_size);
    if (steps < 1) steps = 1;

    float dt_step = dt / (float)steps;
    float half_x = size.getX() * 0.5f;
    float half_z = size.getZ() * 0.5f;

    for (int s = 0; s < steps; s++) {
        float dx = vel.getX() * dt_step;
        if (dx != 0) {
            float next_x = pos.getX() + dx;
            // Check collision with a small vertical offset (0.1f) to avoid hitting the ground the NPC is standing on
            if (CheckCollision(next_x - half_x, pos.getY() + 0.1f, pos.getZ() - half_z,
                               next_x + half_x, pos.getY() + size.getY() - 0.1f, pos.getZ() + half_z)) {
                vel.setX(0);
            } else {
                pos.setX(next_x);
            }
        }

        float dz = vel.getZ() * dt_step;
        if (dz != 0) {
            float next_z = pos.getZ() + dz;
            if (CheckCollision(pos.getX() - half_x, pos.getY() + 0.1f, next_z - half_z,
                               pos.getX() + half_x, pos.getY() + size.getY() - 0.1f, next_z + half_z)) {
                vel.setZ(0);
            } else {
                pos.setZ(next_z);
            }
        }

        float dy = vel.getY() * dt_step;
        if (dy != 0) {
            float next_y = pos.getY() + dy;
            float check_min_y = next_y;
            float check_max_y = next_y + size.getY();
            if (dy > 0) check_min_y += 0.01f; // Skip floor slightly when jumping
            else if (dy < 0) check_max_y -= 0.01f; // Skip ceiling slightly when falling

            // Check collision with a small horizontal buffer (0.1f) to avoid snagging on walls while jumping/falling
            if (CheckCollision(pos.getX() - half_x + 0.1f, check_min_y, pos.getZ() - half_z + 0.1f,
                               pos.getX() + half_x - 0.1f, check_max_y, pos.getZ() + half_z - 0.1f)) {
                if (vel.getY() < 0) is_grounded = true;
                vel.setY(0);
            } else {
                pos.setY(next_y);
            }
        }
    }

    if (!is_grounded && vel.getY() <= 0) {
        float check_dist = 0.05f;
        if (CheckCollision(pos.getX() - half_x + 0.1f, pos.getY() - check_dist, pos.getZ() - half_z + 0.1f,
                          pos.getX() + half_x - 0.1f, pos.getY(), pos.getZ() + half_z - 0.1f)) {
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
    float offset = (float)g_grid_size / -2.0f + 0.5f;

    int min_x = (int)floorf(center.getX() - offset - radius + 0.5f);
    int max_x = (int)ceilf(center.getX() - offset + radius + 0.5f);
    int min_y = (int)floorf(center.getY() - offset - radius + 0.5f);
    int max_y = (int)ceilf(center.getY() - offset + radius + 0.5f);
    int min_z = (int)floorf(center.getZ() - 490.0f - radius + 0.5f);
    int max_z = (int)ceilf(center.getZ() - 490.0f + radius + 0.5f);

    std::set<uint64_t> touched_chunks;
    float r_sq = radius * radius;
    bool world_modified = false;

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                float dx = (float)x + offset - center.getX();
                float dy = (float)y + offset - center.getY();
                float dz = (float)z + 490.0f - center.getZ();
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
    float offset = (float)g_grid_size / -2.0f + 0.5f;

    dmVMath::Vector3 ray_grid = dmVMath::Vector3(origin.getX() - offset, origin.getY() - offset, origin.getZ() - 490.0f);
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
