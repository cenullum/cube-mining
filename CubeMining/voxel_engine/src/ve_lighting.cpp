#include "ve_world.h"
#include "chunk_manager.h"
#include <string.h>
#include <queue>
#include <set>
#include <algorithm>

struct LightNode { int x, y, z; uint8_t val; };
static std::queue<LightNode> add_source, rem_source;
static std::queue<LightNode> add_sun, rem_sun;
static const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

dmMutex::HMutex g_light_mutex = 0;

void MarkLightDirty(int wx, int wy, int wz) {
    int cx, cy, cz, lx, ly, lz;
    ChunkManager::WorldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    Chunk* c = ChunkManager::GetChunk(cx, cy, cz);
    if(c) {
        c->light_dirty = true;
        bool minx = (lx == 0), maxx = (lx == CHUNK_SIZE-1);
        bool miny = (ly == 0), maxy = (ly == CHUNK_SIZE-1);
        bool minz = (lz == 0), maxz = (lz == CHUNK_SIZE-1);
        for(int i = (minx ? -1 : 0); i <= (maxx ? 1 : 0); ++i) {
            for(int j = (miny ? -1 : 0); j <= (maxy ? 1 : 0); ++j) {
                for(int k = (minz ? -1 : 0); k <= (maxz ? 1 : 0); ++k) {
                    if (i == 0 && j == 0 && k == 0) continue;
                    Chunk* n = ChunkManager::GetChunk(cx + i, cy + j, cz + k);
                    if (n) n->light_dirty = true;
                }
            }
        }
    }
}

void ProcessLightQueues() {
    if (!g_light_mutex) return;
    dmMutex::Lock(g_light_mutex);
    while (!rem_source.empty()) {
        LightNode node = rem_source.front(); rem_source.pop();
        for (int d = 0; d < 6; d++) {
            int nx = node.x + dirs[d][0], ny = node.y + dirs[d][1], nz = node.z + dirs[d][2];
            uint8_t nl = ChunkManager::GetSourceLight(nx, ny, nz);
            if (nl != 0 && nl < node.val) {
                ChunkManager::SetSourceLight(nx, ny, nz, 0);
                MarkLightDirty(nx, ny, nz);
                rem_source.push({nx, ny, nz, nl});
            } else if (nl >= node.val) {
                add_source.push({nx, ny, nz, nl});
            }
        }
    }
    while (!add_source.empty()) {
        LightNode node = add_source.front(); add_source.pop();
        if (node.val <= 1) continue;
        for (int d = 0; d < 6; d++) {
            int nx = node.x + dirs[d][0], ny = node.y + dirs[d][1], nz = node.z + dirs[d][2];
            uint8_t neighbor_id = ChunkManager::GetBlock(nx, ny, nz);
            bool is_trans = neighbor_id == 0 || (!g_block_defs[neighbor_id].registered || g_block_defs[neighbor_id].render_type >= 1);
            if (is_trans) {
                uint8_t nl = ChunkManager::GetSourceLight(nx, ny, nz);
                if (nl < node.val - 1) {
                    ChunkManager::SetSourceLight(nx, ny, nz, node.val - 1);
                    MarkLightDirty(nx, ny, nz);
                    add_source.push({nx, ny, nz, (uint8_t)(node.val - 1)});
                }
            }
        }
    }
    while (!rem_sun.empty()) {
        LightNode node = rem_sun.front(); rem_sun.pop();
        for (int d = 0; d < 6; d++) {
            int nx = node.x + dirs[d][0], ny = node.y + dirs[d][1], nz = node.z + dirs[d][2];
            uint8_t nl = ChunkManager::GetSunlight(nx, ny, nz);
            uint8_t expected = (d==3 && node.val==15) ? 15 : (node.val > 0 ? node.val - 1 : 0);
            if (nl != 0 && nl <= expected) {
                ChunkManager::SetSunlight(nx, ny, nz, 0);
                MarkLightDirty(nx, ny, nz);
                rem_sun.push({nx, ny, nz, nl});
            } else if (nl > expected) {
                add_sun.push({nx, ny, nz, nl});
            }
        }
    }
    while (!add_sun.empty()) {
        LightNode node = add_sun.front(); add_sun.pop();
        for (int d = 0; d < 6; d++) {
            int nx = node.x + dirs[d][0], ny = node.y + dirs[d][1], nz = node.z + dirs[d][2];
            uint8_t neighbor_id = ChunkManager::GetBlock(nx, ny, nz);
            bool is_trans = neighbor_id == 0 || (!g_block_defs[neighbor_id].registered || g_block_defs[neighbor_id].render_type >= 1);
            if (is_trans) {
                uint8_t nl = ChunkManager::GetSunlight(nx, ny, nz);
                uint8_t next_l = (d==3 && node.val==15) ? 15 : (node.val > 0 ? node.val - 1 : 0);
                if (nl < next_l) {
                    ChunkManager::SetSunlight(nx, ny, nz, next_l);
                    MarkLightDirty(nx, ny, nz);
                    add_sun.push({nx, ny, nz, next_l});
                }
            }
        }
    }
    dmMutex::Unlock(g_light_mutex);
}

void LightingUpdateBlockChanged(int wx, int wy, int wz, uint8_t old_id, uint8_t new_id) {
    int new_light = g_block_defs[new_id].registered ? g_block_defs[new_id].light_level : 0;
    bool old_trans = (!g_block_defs[old_id].registered || g_block_defs[old_id].render_type >= 1);
    bool new_trans = (!g_block_defs[new_id].registered || g_block_defs[new_id].render_type >= 1);

    dmMutex::Lock(g_light_mutex);
    uint8_t cur_src = ChunkManager::GetSourceLight(wx, wy, wz);
    if (new_light > cur_src) {
        ChunkManager::SetSourceLight(wx, wy, wz, new_light);
        MarkLightDirty(wx, wy, wz);
        add_source.push({wx, wy, wz, (uint8_t)new_light});
    } else if (new_light < cur_src || !new_trans) {
        uint8_t diff = cur_src;
        ChunkManager::SetSourceLight(wx, wy, wz, 0);
        MarkLightDirty(wx, wy, wz);
        rem_source.push({wx, wy, wz, diff});
    }

    if (!old_trans && new_trans && new_light == 0) {
        // We just created an empty space. Pull in source light from neighbors!
        uint8_t max_given = 0;
        for(int d=0; d<6; d++){
            uint8_t nl = ChunkManager::GetSourceLight(wx+dirs[d][0], wy+dirs[d][1], wz+dirs[d][2]);
            uint8_t gl = (nl > 0) ? nl - 1 : 0;
            max_given = std::max(max_given, gl);
        }
        if (max_given > 0) {
            ChunkManager::SetSourceLight(wx, wy, wz, max_given);
            MarkLightDirty(wx, wy, wz);
            add_source.push({wx, wy, wz, max_given});
        }
    }

    uint8_t cur_sun = ChunkManager::GetSunlight(wx, wy, wz);
    if (old_trans && !new_trans) {
        if (cur_sun > 0) {
            ChunkManager::SetSunlight(wx, wy, wz, 0);
            MarkLightDirty(wx, wy, wz);
            rem_sun.push({wx, wy, wz, cur_sun});
            if (cur_sun == 15) {
                int ty = wy - 1;
                while (ty > -128) {
                    uint8_t b = ChunkManager::GetBlock(wx, ty, wz);
                    if (b != 0 && g_block_defs[b].render_type == 0) break;
                    if (ChunkManager::GetSunlight(wx, ty, wz) == 15) {
                        ChunkManager::SetSunlight(wx, ty, wz, 0);
                        MarkLightDirty(wx, ty, wz);
                        rem_sun.push({wx, ty, wz, 15});
                    } else break;
                    ty--;
                }
            }
        }
    } else if (!old_trans && new_trans) {
        uint8_t above_sun = ChunkManager::GetSunlight(wx, wy + 1, wz);
        if (above_sun == 15) {
            int ty = wy;
            while (ty > -128) {
                uint8_t b = ChunkManager::GetBlock(wx, ty, wz);
                if (b != 0 && g_block_defs[b].render_type == 0) break;
                ChunkManager::SetSunlight(wx, ty, wz, 15);
                MarkLightDirty(wx, ty, wz);
                add_sun.push({wx, ty, wz, 15});
                ty--;
            }
        } else {
            uint8_t max_given = 0;
            for(int d=0; d<6; d++){
                uint8_t nl = ChunkManager::GetSunlight(wx+dirs[d][0], wy+dirs[d][1], wz+dirs[d][2]);
                uint8_t gl = (nl > 0) ? nl - 1 : 0;
                if (d == 2 && nl == 15) gl = 15; // From above, 15 stays 15
                max_given = std::max(max_given, gl);
            }
            if (max_given > 0) {
                ChunkManager::SetSunlight(wx, wy, wz, max_given);
                MarkLightDirty(wx, wy, wz);
                add_sun.push({wx, wy, wz, max_given});
            }
        }
    }
    dmMutex::Unlock(g_light_mutex);
}

void perform_lighting_pass(Chunk* chunk) {
    if (!chunk) return;
    int cx = chunk->cx * CHUNK_SIZE, cy = chunk->cy * CHUNK_SIZE, cz = chunk->cz * CHUNK_SIZE;
    
    // Simple batch seeding for new chunks
    dmMutex::Lock(g_light_mutex);
    for (int y = 0; y < CHUNK_SIZE; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                int idx = x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE;
                uint8_t block_id = chunk->blocks[idx];
                bool is_transparent = !g_block_defs[block_id].registered || (g_block_defs[block_id].render_type >= 1);
                
                if (g_block_defs[block_id].registered && g_block_defs[block_id].light_level > 0) {
                    chunk->source_light[idx] = g_block_defs[block_id].light_level;
                    add_source.push({cx + x, cy + y, cz + z, g_block_defs[block_id].light_level});
                }
                
                if (is_transparent) {
                    bool sky_exposed = true;
                    for (int ty = y + 1; ty < CHUNK_SIZE; ty++) {
                        uint8_t trace_id = chunk->blocks[x + z * CHUNK_SIZE + ty * CHUNK_SIZE * CHUNK_SIZE];
                        if (trace_id != 0 && g_block_defs[trace_id].registered && g_block_defs[trace_id].render_type == 0) {
                            sky_exposed = false; break;
                        }
                    }
                    if (sky_exposed && cy < 4) {
                       uint8_t trace_above = ChunkManager::GetBlock(cx + x, cy * CHUNK_SIZE + CHUNK_SIZE, cz + z);
                       if(trace_above != 0) { sky_exposed = false; }
                    }
                    if (sky_exposed) {
                        chunk->sun_light[idx] = 15;
                        add_sun.push({cx + x, cy + y, cz + z, 15});
                    }
                }
            }
        }
    }
    chunk->light_dirty = true;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            for (int k = -1; k <= 1; k++) {
                if (i == 0 && j == 0 && k == 0) continue;
                Chunk* n = ChunkManager::GetChunk(chunk->cx + i, chunk->cy + j, chunk->cz + k);
                if (n) n->light_dirty = true;
            }
        }
    }
    dmMutex::Unlock(g_light_mutex);
}

int Lua_GetAmbientLight(lua_State* L) {
    dmVMath::Vector3 pos = *dmScript::ToVector3(L, 1);
    int x = (int)floorf(pos.getX() + 0.5f);
    int y = (int)floorf(pos.getY() + 0.5f);
    int z = (int)floorf(pos.getZ() + 0.5f);

    float sun_f = ChunkManager::GetSunlight(x, y, z) / 15.0f;
    float source_f = ChunkManager::GetSourceLight(x, y, z) / 15.0f;

    float r = fminf(1.0f, fmaxf(0.02f, sun_f + source_f * 1.0f));
    float g = fminf(1.0f, fmaxf(0.02f, sun_f + source_f * 0.9f));
    float b = fminf(1.0f, fmaxf(0.02f, sun_f + source_f * 0.6f));
    dmScript::PushVector4(L, dmVMath::Vector4(r, g, b, 1.0f));
    return 1;
}

int Lua_UpdateLightBuffer(lua_State* L) {
    dmScript::LuaHBuffer* luabuf = dmScript::CheckBuffer(L, 1);
    dmBuffer::HBuffer buffer_handle = luabuf->m_Buffer;
    uint8_t* stream_ptr = 0;
    uint32_t stream_count = 0, stream_components = 0, stream_stride = 0;
    dmBuffer::GetStream(buffer_handle, dmHashString64("rgba"), (void**)&stream_ptr, &stream_count, &stream_components, &stream_stride);
    
    dmMutex::Lock(ChunkManager::mutex);
    for (auto& pair : ChunkManager::active_chunks) {
        Chunk* c = pair.second;
        if (c->is_empty) continue; 
        if (c->light_tex_u == -1 || c->light_tex_v == -1) continue; 
        if (!c->light_dirty && c->is_generated) continue; 
        
        int start_x = c->light_tex_u;  
        int start_y = c->light_tex_v;
        int tex_width = 2048;
        
        for (int ly = -1; ly <= 16; ++ly) {
            for (int lz = -1; lz <= 16; ++lz) {
                for (int lx = -1; lx <= 16; ++lx) {
                    int wx = c->cx * CHUNK_SIZE + lx;
                    int wy = c->cy * CHUNK_SIZE + ly;
                    int wz = c->cz * CHUNK_SIZE + lz;
                    
                    uint8_t block_id = ChunkManager::GetBlock(wx, wy, wz);
                    float sun_f = ChunkManager::GetSunlight(wx, wy, wz) / 15.0f;
                    float torch_f = ChunkManager::GetSourceLight(wx, wy, wz) / 15.0f;
                    
                    float out_r = fminf(1.0f, sun_f + torch_f);
                    float out_g = fminf(1.0f, sun_f + torch_f * 0.9f);
                    float out_b = fminf(1.0f, sun_f + torch_f * 0.6f);
                    uint8_t a = (block_id != 0 && g_block_defs[block_id].render_type == 0) ? 0 : 255;
                    
                    int tex_x = start_x + (lx + 1);
                    int tex_y = start_y + (ly + 1) * 18 + (lz + 1);
                    
                    int tex_idx = tex_x + tex_y * tex_width;
                    if(tex_idx >= 0 && tex_idx < (int)stream_count) {
                       stream_ptr[tex_idx * stream_stride + 0] = (uint8_t)(out_r * 255.0f);
                       stream_ptr[tex_idx * stream_stride + 1] = (uint8_t)(out_g * 255.0f);
                       stream_ptr[tex_idx * stream_stride + 2] = (uint8_t)(out_b * 255.0f);
                       stream_ptr[tex_idx * stream_stride + 3] = a;
                    }
                }
            }
        }
        c->light_dirty = false;
    }
    dmMutex::Unlock(ChunkManager::mutex);
    
    dmBuffer::UpdateContentVersion(buffer_handle);
    return 0;
}

int Lua_PerformLightingPassSync(lua_State* L) {
    dmMutex::Lock(ChunkManager::mutex);
    for (auto& pair : ChunkManager::active_chunks) {
        pair.second->light_dirty = true;
    }
    dmMutex::Unlock(ChunkManager::mutex);
    return 0;
}
