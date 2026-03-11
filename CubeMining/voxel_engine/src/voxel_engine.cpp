#define EXTENSION_NAME voxel_engine
#define LIB_NAME "voxel_engine"

#include "ve_world.h"
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/time.h>
#include <algorithm>
#include <set>

// Worker Thread State
static dmThread::Thread g_worker_thread = 0;
static dmMutex::HMutex g_mutex = 0;
static volatile bool g_worker_running = false;
static volatile bool g_worker_has_work = false;
static volatile bool g_worker_result_ready = false;

static void WorkerThreadLoop(void* arg) {
    while (g_worker_running) {
        bool has_work = false;
        dmMutex::Lock(g_mutex);
        if (g_worker_has_work) {
            has_work = true;
            g_worker_has_work = false;
        }
        dmMutex::Unlock(g_mutex);

        if (has_work) {
            perform_lighting_pass(g_blocks, g_sun_light, g_source_light, g_grid_size);
            execute_mesh_generation_pipeline(g_blocks, g_sun_light, g_source_light, g_grid_size, g_ao_enabled, g_light_mode);

            dmMutex::Lock(g_mutex);
            g_worker_result_ready = true;
            dmMutex::Unlock(g_mutex);
        }
        dmTime::Sleep(1000); // 1ms
    }
}

// Lua Bridge
static int Lua_InitializeVoxelEngine(lua_State* L) {
    int side_length = luaL_checkinteger(L, 1);
    if (side_length > MAX_GRID_SIZE) side_length = MAX_GRID_SIZE;
    g_grid_size = side_length;
    g_seed = luaL_checkinteger(L, 2);
    
    // Explicitly initialize Air block def
    g_block_defs[0].registered = true;
    g_block_defs[0].transparent = true;
    g_block_defs[0].solid = false;
    g_block_defs[0].light_level = 0;

    initialize_world_terrain_data();
    alloc_mesh_buffers(side_length);

    if (!g_mutex) g_mutex = dmMutex::New();
    if (!g_worker_running) {
        g_worker_running = true;
        g_worker_has_work = false;
        g_worker_result_ready = false;
        g_worker_thread = dmThread::New(WorkerThreadLoop, 0x80000, 0, "VoxelWorker");
    }
    return 0;
}

static int Lua_ShutdownVoxelEngine(lua_State* L) {
    g_worker_running = false;
    if (g_worker_thread) { 
        dmThread::Join(g_worker_thread); 
        g_worker_thread = 0; 
    }
    if (g_mutex) { 
        dmMutex::Delete(g_mutex); 
        g_mutex = 0; 
    }
    
    free(g_vertex_positions);   g_vertex_positions = 0;
    free(g_vertex_uvs_base);    g_vertex_uvs_base = 0;
    free(g_vertex_uvs_local);   g_vertex_uvs_local = 0;
    free(g_vertex_face_ids);    g_vertex_face_ids = 0;
    
    return 0;
}

static int Lua_RegisterBlockType(lua_State* L) {
    int id = luaL_checkinteger(L, 1);
    if (id < 0 || id > 255) return 0;
    
    BlockDef& bd = g_block_defs[id];
    bd.registered = true;
    bd.transparent = lua_toboolean(L, 2);
    bd.solid = lua_toboolean(L, 3);
    bd.light_level = luaL_checkinteger(L, 4);

    if (lua_isstring(L, 5)) bd.name_hash = dmHashString64(lua_tostring(L, 5));
    else bd.name_hash = 0;

    if (lua_isstring(L, 6)) bd.hit_sound_hash = dmHashString64(lua_tostring(L, 6));
    else bd.hit_sound_hash = 0;

    if (lua_isstring(L, 7)) bd.break_sound_hash = dmHashString64(lua_tostring(L, 7));
    else bd.break_sound_hash = 0;

    if (lua_istable(L, 8)) {
        for (int face = 1; face <= 6; face++) {
            lua_rawgeti(L, 8, face);
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1); bd.uvs[face].u = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); bd.uvs[face].v = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 3); bd.uvs[face].w = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 4); bd.uvs[face].h = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int Lua_GetBlockInfo(lua_State* L) {
    int id = luaL_checkinteger(L, 1);
    if (id < 0 || id > 255 || !g_block_defs[id].registered) {
        lua_pushnil(L);
        return 1;
    }
    const BlockDef& bd = g_block_defs[id];
    lua_newtable(L);
    lua_pushboolean(L, bd.transparent); lua_setfield(L, -2, "transparent");
    lua_pushboolean(L, bd.solid); lua_setfield(L, -2, "solid");
    lua_pushinteger(L, bd.light_level); lua_setfield(L, -2, "light_level");
    if (bd.name_hash != 0) { dmScript::PushHash(L, bd.name_hash); lua_setfield(L, -2, "name"); }
    if (bd.hit_sound_hash != 0) { dmScript::PushHash(L, bd.hit_sound_hash); lua_setfield(L, -2, "hit_sound"); }
    if (bd.break_sound_hash != 0) { dmScript::PushHash(L, bd.break_sound_hash); lua_setfield(L, -2, "break_sound"); }
    lua_newtable(L);
    for (int face = 1; face <= 6; face++) {
        lua_newtable(L);
        lua_pushnumber(L, bd.uvs[face].u); lua_setfield(L, -2, "u");
        lua_pushnumber(L, bd.uvs[face].v); lua_setfield(L, -2, "v");
        lua_pushnumber(L, bd.uvs[face].w); lua_setfield(L, -2, "w");
        lua_pushnumber(L, bd.uvs[face].h); lua_setfield(L, -2, "h");
        lua_rawseti(L, -2, face);
    }
    lua_setfield(L, -2, "uvs");
    return 1;
}

static int Lua_SetBlockInWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    int id = luaL_checkinteger(L, 4);
    SetBlock(x, y, z, (uint8_t)id);
    return 0;
}

static int Lua_GetBlockFromWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    lua_pushinteger(L, GetBlock(x, y, z));
    return 1;
}

static int Lua_GetAmbientLight(lua_State* L) {
    dmVMath::Vector3 pos = *dmScript::ToVector3(L, 1);
    float offset = (float)g_grid_size / -2.0f + 0.5f;
    int x = (int)floorf(pos.getX() - offset + 0.5f);
    int y = (int)floorf(pos.getY() - offset + 0.5f);
    int z = (int)floorf(pos.getZ() - 490.0f + 0.5f);

    float sun_f = 1.0f, source_f = 0.0f;
    if (x >= 0 && x < g_grid_size && y >= 0 && y < g_grid_size && z >= 0 && z < g_grid_size) {
        int idx = calculate_block_index(x, y, z, g_grid_size);
        sun_f = (float)g_sun_light[idx] / 15.0f;
        source_f = ((float)g_source_light[idx] / 15.0f) * 1.5f;
    }
    float r = fmaxf(0.02f, sun_f + source_f * 1.0f);
    float g = fmaxf(0.02f, sun_f + source_f * 0.9f);
    float b = fmaxf(0.02f, sun_f + source_f * 0.6f);
    dmScript::PushVector4(L, dmVMath::Vector4(r, g, b, 1.0f));
    return 1;
}

static int Lua_GetMaxVertices(lua_State* L) {
    uint32_t max_verts = (uint32_t)g_grid_size * g_grid_size * g_grid_size * 3 * 6;
    lua_pushinteger(L, max_verts);
    return 1;
}

static int Lua_RequestAsyncMeshUpdate(lua_State* L) {
    if (!g_worker_thread || !g_mutex) return 0;
    g_ao_enabled = lua_toboolean(L, 1);
    g_light_mode = luaL_checkinteger(L, 2);
    g_debug_enabled = lua_toboolean(L, 3);
    dmMutex::Lock(g_mutex);
    g_worker_has_work = true;
    g_worker_result_ready = false;
    dmMutex::Unlock(g_mutex);
    return 0;
}

static int Lua_PollMeshBuffer(lua_State* L) {
    if (!g_worker_thread || !g_vertex_positions) { lua_pushnil(L); return 1; }
    dmMutex::Lock(g_mutex);
    bool is_ready = g_worker_result_ready;
    uint32_t v_count = g_result_vertex_count;
    uint32_t f_count = g_result_face_count;
    uint32_t q_count = g_result_quad_count;
    double build_time = g_result_build_time;
    dmMutex::Unlock(g_mutex);

    if (!is_ready) { lua_pushnil(L); return 1; }

    if (v_count == 0) {
        dmBuffer::StreamDeclaration empty_decl[] = { { dmHashString64("position"),  dmBuffer::VALUE_TYPE_FLOAT32, 3 } };
        dmBuffer::HBuffer buffer_handle = 0;
        dmBuffer::Create(0, empty_decl, 1, &buffer_handle);
        dmScript::LuaHBuffer luabuf(buffer_handle, dmScript::OWNER_LUA);
        dmScript::PushBuffer(L, luabuf);
        lua_pushinteger(L, 0); lua_pushinteger(L, 0); lua_pushinteger(L, 0);
        lua_pushnumber(L, build_time);
        dmMutex::Lock(g_mutex); g_worker_result_ready = false; dmMutex::Unlock(g_mutex);
        return 5;
    }

    dmBuffer::StreamDeclaration streams_decl[] = {
        { dmHashString64("position"),  dmBuffer::VALUE_TYPE_FLOAT32, 3 },
        { dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 4 },
        { dmHashString64("texcoord1"), dmBuffer::VALUE_TYPE_FLOAT32, 4 },
        { dmHashString64("texcoord2"), dmBuffer::VALUE_TYPE_FLOAT32, 1 }
    };

    dmBuffer::HBuffer buffer_handle = 0;
    dmBuffer::Result res = dmBuffer::Create(v_count, streams_decl, 4, &buffer_handle);
    if (res != dmBuffer::RESULT_OK) { lua_pushnil(L); return 1; }

    copy_array_to_buffer_stream(buffer_handle, dmHashString64("position"),  g_vertex_positions,  v_count, 3);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord0"), g_vertex_uvs_base,   v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord1"), g_vertex_uvs_local,  v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord2"), g_vertex_face_ids,   v_count, 1);

    dmBuffer::UpdateContentVersion(buffer_handle);
    dmScript::LuaHBuffer luabuf(buffer_handle, dmScript::OWNER_LUA);
    dmScript::PushBuffer(L, luabuf);
    lua_pushinteger(L, v_count); lua_pushinteger(L, f_count); lua_pushinteger(L, q_count); lua_pushnumber(L, build_time);
    dmMutex::Lock(g_mutex); g_worker_result_ready = false; dmMutex::Unlock(g_mutex);
    return 5;
}

static int Lua_GetMeshDebugQuads(lua_State* L) {
    lua_newtable(L);
    for (size_t i = 0; i < g_debug_quads.size(); ++i) {
        const DebugQuad& dq = g_debug_quads[i];
        lua_newtable(L);
        lua_pushnumber(L, dq.x1); lua_rawseti(L, -2, 1);
        lua_pushnumber(L, dq.y1); lua_rawseti(L, -2, 2);
        lua_pushnumber(L, dq.z1); lua_rawseti(L, -2, 3);
        lua_pushnumber(L, dq.x2); lua_rawseti(L, -2, 4);
        lua_pushnumber(L, dq.y2); lua_rawseti(L, -2, 5);
        lua_pushnumber(L, dq.z2); lua_rawseti(L, -2, 6);
        lua_pushnumber(L, dq.x3); lua_rawseti(L, -2, 7);
        lua_pushnumber(L, dq.y3); lua_rawseti(L, -2, 8);
        lua_pushnumber(L, dq.z3); lua_rawseti(L, -2, 9);
        lua_pushnumber(L, dq.x4); lua_rawseti(L, -2, 10);
        lua_pushnumber(L, dq.y4); lua_rawseti(L, -2, 11);
        lua_pushnumber(L, dq.z4); lua_rawseti(L, -2, 12);
        lua_pushinteger(L, dq.dir); lua_rawseti(L, -2, 13);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int Lua_UpdateLightBuffer(lua_State* L) {
    dmScript::LuaHBuffer* luabuf = dmScript::CheckBuffer(L, 1);
    dmBuffer::HBuffer buffer_handle = luabuf->m_Buffer;
    uint8_t* stream_ptr = 0;
    uint32_t stream_count = 0, stream_components = 0, stream_stride = 0;
    dmBuffer::GetStream(buffer_handle, dmHashString64("rgba"), (void**)&stream_ptr, &stream_count, &stream_components, &stream_stride);
    
    for (int y = 0; y < g_grid_size; ++y) {
        for (int z = 0; z < g_grid_size; ++z) {
            for (int x = 0; x < g_grid_size; ++x) {
                int block_idx = calculate_block_index(x, y, z, g_grid_size);
                float sun_f = g_sun_light[block_idx] / 15.0f;
                float torch_f = g_source_light[block_idx] / 15.0f;
                float out_r = fminf(1.0f, sun_f + torch_f);
                float out_g = fminf(1.0f, sun_f + torch_f * 0.9f);
                float out_b = fminf(1.0f, sun_f + torch_f * 0.6f);
                uint8_t a = IsSolid(x, y, z) ? 0 : 255;
                int tex_idx = x + z * g_grid_size + y * g_grid_size * g_grid_size;
                stream_ptr[tex_idx * stream_stride + 0] = (uint8_t)(out_r * 255.0f);
                stream_ptr[tex_idx * stream_stride + 1] = (uint8_t)(out_g * 255.0f);
                stream_ptr[tex_idx * stream_stride + 2] = (uint8_t)(out_b * 255.0f);
                stream_ptr[tex_idx * stream_stride + 3] = a;
            }
        }
    }
    dmBuffer::UpdateContentVersion(buffer_handle);
    return 0;
}

static int Lua_PerformLightingPassSync(lua_State* L) {
    perform_lighting_pass(g_blocks, g_sun_light, g_source_light, g_grid_size);
    return 0;
}

static int Lua_RegisterNPC(lua_State* L) {
    dmhash_t id = dmScript::CheckHash(L, 1);
    dmGameObject::HInstance instance = dmScript::CheckGOInstance(L, 2);
    dmVMath::Vector3 pos = *dmScript::ToVector3(L, 3);
    dmVMath::Vector3 size = *dmScript::ToVector3(L, 4);
    bool is_dead = lua_toboolean(L, 5);

    int state = 1;
    float timer = 0, state_duration = 5.0f, speed = 3.0f, gravity = -25.0f, jump_force = 8.0f, rotation_offset_y = 0, health = 100.0f;
    dmhash_t socket = 0;
    if (lua_istable(L, 6)) {
        lua_getfield(L, 6, "state"); state = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 1; lua_pop(L, 1);
        lua_getfield(L, 6, "timer"); timer = (float)luaL_optnumber(L, -1, 0); lua_pop(L, 1);
        lua_getfield(L, 6, "state_duration"); state_duration = (float)luaL_optnumber(L, -1, 5); lua_pop(L, 1);
        lua_getfield(L, 6, "speed"); speed = (float)luaL_optnumber(L, -1, 3); lua_pop(L, 1);
        lua_getfield(L, 6, "gravity"); gravity = (float)luaL_optnumber(L, -1, -25); lua_pop(L, 1);
        lua_getfield(L, 6, "jump_force"); jump_force = (float)luaL_optnumber(L, -1, 8); lua_pop(L, 1);
        lua_getfield(L, 6, "rotation_offset_y"); rotation_offset_y = (float)luaL_optnumber(L, -1, 0); lua_pop(L, 1);
        lua_getfield(L, 6, "health"); health = (float)luaL_optnumber(L, -1, 100); lua_pop(L, 1);
        lua_getfield(L, 6, "socket"); if (lua_isuserdata(L, -1)) socket = dmScript::CheckHash(L, -1); lua_pop(L, 1);
    }
    for (auto& npc : g_npcs) {
        if (npc.id == id) {
            npc.instance = instance; npc.pos = pos; npc.size = size; npc.is_dead = is_dead;
            npc.state = state; npc.timer = timer; npc.state_duration = state_duration;
            npc.speed = speed; npc.gravity = gravity; npc.jump_force = jump_force;
            npc.rotation_offset_y = rotation_offset_y; npc.health = health; npc.socket = socket;
            return 0;
        }
    }
    g_npcs.push_back({id, instance, pos, dmVMath::Vector3(0,0,0), dmVMath::Vector3(0,0,0), size, is_dead, state, timer, state_duration, speed, gravity, jump_force, rotation_offset_y, health, socket});
    return 0;
}

static int Lua_UnregisterNPC(lua_State* L) {
    dmhash_t id = dmScript::CheckHash(L, 1);
    for (size_t i = 0; i < g_npcs.size(); ++i) { if (g_npcs[i].id == id) { g_npcs.erase(g_npcs.begin() + i); break; } }
    return 0;
}

static int Lua_Explosion(lua_State* L) {
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

    if (world_modified && g_worker_thread && g_mutex) { dmMutex::Lock(g_mutex); g_worker_has_work = true; dmMutex::Unlock(g_mutex); }
    return 2;
}

static int Lua_ShootRay(lua_State* L) {
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

static int Lua_MoveAndSlide(lua_State* L) {
    dmVMath::Vector3 pos = *dmScript::ToVector3(L, 1), vel = *dmScript::ToVector3(L, 2), size = *dmScript::ToVector3(L, 3);
    float dt = (float)luaL_checknumber(L, 4);
    bool grounded = false;
    MoveAndSlide(pos, vel, size, dt, grounded);
    dmScript::PushVector3(L, pos); dmScript::PushVector3(L, vel); lua_pushboolean(L, grounded);
    return 3;
}

static int Lua_CheckCollision(lua_State* L) {
    float x1=luaL_checknumber(L, 1), y1=luaL_checknumber(L, 2), z1=luaL_checknumber(L, 3), x2=luaL_checknumber(L, 4), y2=luaL_checknumber(L, 5), z2=luaL_checknumber(L, 6);
    lua_pushboolean(L, CheckCollision(x1,y1,z1,x2,y2,z2));
    return 1;
}

static const luaL_reg Module_methods[] = {
    {"init", Lua_InitializeVoxelEngine}, {"shutdown", Lua_ShutdownVoxelEngine}, {"register_block", Lua_RegisterBlockType}, {"get_block_info", Lua_GetBlockInfo}, {"set_block", Lua_SetBlockInWorld}, {"get_block", Lua_GetBlockFromWorld}, {"get_ambient_light", Lua_GetAmbientLight}, {"get_max_vertices", Lua_GetMaxVertices}, {"request_mesh_update", Lua_RequestAsyncMeshUpdate}, {"poll_mesh_buffer", Lua_PollMeshBuffer}, {"get_debug_quads", Lua_GetMeshDebugQuads}, {"update_light_buffer", Lua_UpdateLightBuffer}, {"recalc_lighting_sync", Lua_PerformLightingPassSync}, {"move_and_slide", Lua_MoveAndSlide}, {"check_collision", Lua_CheckCollision}, {"register_npc", Lua_RegisterNPC}, {"unregister_npc", Lua_UnregisterNPC}, {"explode", Lua_Explosion}, {"shoot", Lua_ShootRay}, {0, 0}
};

static dmExtension::Result AppInit(dmExtension::AppParams* params) { return dmExtension::RESULT_OK; }
static dmExtension::Result Init(dmExtension::Params* params) { luaL_register(params->m_L, LIB_NAME, Module_methods); lua_pop(params->m_L, 1); return dmExtension::RESULT_OK; }
static uint64_t g_last_time = 0;
static dmExtension::Result UpdateExtension(dmExtension::Params* params) {
    if (g_grid_size == 0) return dmExtension::RESULT_OK;
    uint64_t now = dmTime::GetTime();
    if (g_last_time == 0 || (now - g_last_time) > 1000000) { // Reset if first frame or pause > 1s
        g_last_time = now;
        return dmExtension::RESULT_OK;
    }
    float dt = (float)(now - g_last_time) / 1000000.0f;
    g_last_time = now;
    
    UpdateAllNPCs(dt);
    return dmExtension::RESULT_OK;
}
static dmExtension::Result Finalize(dmExtension::Params* params) { return dmExtension::RESULT_OK; }
static dmExtension::Result AppFinalFunc(dmExtension::AppParams* params) {
    if (g_worker_running) { g_worker_running = false; if (g_worker_thread) { dmThread::Join(g_worker_thread); g_worker_thread = 0; } if (g_mutex) { dmMutex::Delete(g_mutex); g_mutex = 0; } free(g_vertex_positions); free(g_vertex_uvs_base); free(g_vertex_uvs_local); free(g_vertex_face_ids); }
    return dmExtension::RESULT_OK;
}
DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInit, AppFinalFunc, Init, UpdateExtension, 0, Finalize)
