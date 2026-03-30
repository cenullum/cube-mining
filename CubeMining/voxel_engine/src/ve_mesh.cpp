#include "ve_world.h"
#include "chunk_manager.h"
#include <stdlib.h>
#include <string.h>

static void append_quad_to_mesh_buffers(Chunk* chunk, int quad_idx, float p1x, float p1y, float p1z, float p2x, float p2y, float p2z,
    float p3x, float p3y, float p3z, float p4x, float p4y, float p4z,
    const UVData* uv_data, int quad_width, int quad_height, int face_direction, uint16_t block_id, bool is_transparent) {

    float* pos_ptr   = is_transparent ? chunk->trans_pos : chunk->opaque_pos;
    float* uvb_ptr   = is_transparent ? chunk->trans_uvb : chunk->opaque_uvb;
    float* uvl_ptr   = is_transparent ? chunk->trans_uvl : chunk->opaque_uvl;
    float* face_ptr  = is_transparent ? chunk->trans_face : chunk->opaque_face;

    int base_v3 = quad_idx * 18;
    int base_v4 = quad_idx * 24;
    int base_v1 = quad_idx * 6;
    
    // Offset local coords by chunk pos, so the spawned GameObject represents world pos directly? 
    // Or keep them relative to chunk, and the GO will be positioned at chunk world coordinates.
    // Defold GameObjects will be positioned at (cx*16, cy*16, cz*16).
    // So vertex positions should be relative to chunk base (0..16).
    float px[4] = {p1x, p2x, p3x, p4x};
    float py[4] = {p1y, p2y, p3y, p4y};
    float pz[4] = {p1z, p2z, p3z, p4z};
    int indices[6] = {0, 1, 2, 0, 2, 3};

    float lux[4] = {0, 1, 1, 0};
    float luy[4] = {0, 0, 1, 1};

    for(int i = 0; i < 6; i++) {
        int idx = indices[i];
        int v_off = i;
        
        pos_ptr[base_v3 + v_off * 3 + 0] = px[idx];
        pos_ptr[base_v3 + v_off * 3 + 1] = py[idx];
        pos_ptr[base_v3 + v_off * 3 + 2] = pz[idx];

        uvb_ptr[base_v4 + v_off * 4 + 0] = uv_data->u;
        uvb_ptr[base_v4 + v_off * 4 + 1] = uv_data->v;
        uvb_ptr[base_v4 + v_off * 4 + 2] = uv_data->w;
        uvb_ptr[base_v4 + v_off * 4 + 3] = uv_data->h;

        uvl_ptr[base_v4 + v_off * 4 + 0] = lux[idx];
        uvl_ptr[base_v4 + v_off * 4 + 1] = luy[idx];
        uvl_ptr[base_v4 + v_off * 4 + 2] = (float)quad_width;
        uvl_ptr[base_v4 + v_off * 4 + 3] = (float)quad_height;

        face_ptr[base_v1 + v_off] = (float)face_direction;
    }

    if (g_debug_enabled) {
        float cx = chunk->cx * CHUNK_SIZE;
        float cy = chunk->cy * CHUNK_SIZE;
        float cz = chunk->cz * CHUNK_SIZE;
        DebugQuad dq;
        dq.x1 = cx + p1x; dq.y1 = cy + p1y; dq.z1 = cz + p1z;
        dq.x2 = cx + p2x; dq.y2 = cy + p2y; dq.z2 = cz + p2z;
        dq.x3 = cx + p3x; dq.y3 = cy + p3y; dq.z3 = cz + p3z;
        dq.x4 = cx + p4x; dq.y4 = cy + p4y; dq.z4 = cz + p4z;
        dq.dir = face_direction;
        g_debug_quads.push_back(dq);
    }
}

void execute_mesh_generation_pipeline(Chunk* chunk) {
    if (!chunk) return;

    // Pre-fetch 6 neighbors once to avoid thousands of mutex locks in the loops
    Chunk* neighbors[7]; // Directions 1-6
    neighbors[1] = ChunkManager::GetOrCreateChunk(chunk->cx, chunk->cy, chunk->cz + 1, false);
    neighbors[2] = ChunkManager::GetOrCreateChunk(chunk->cx, chunk->cy, chunk->cz - 1, false);
    neighbors[3] = ChunkManager::GetOrCreateChunk(chunk->cx, chunk->cy + 1, chunk->cz, false);
    neighbors[4] = ChunkManager::GetOrCreateChunk(chunk->cx, chunk->cy - 1, chunk->cz, false);
    neighbors[5] = ChunkManager::GetOrCreateChunk(chunk->cx + 1, chunk->cy, chunk->cz, false);
    neighbors[6] = ChunkManager::GetOrCreateChunk(chunk->cx - 1, chunk->cy, chunk->cz, false);

    uint64_t start_time = dmTime::GetTime();
    int current_quad_index = 0, total_face_count = 0;
    int trans_quad_index = 0, trans_face_count = 0;
    static uint32_t greedy_mask[CHUNK_SIZE * CHUNK_SIZE];

    // Allocate max potential size to be safe, then we'll tell Lua the exact count.
    // Max quads for 16x16x16 is 4096 * 3 = 12288 quads = 73728 verts.
    uint32_t max_v = CHUNK_VOLUME * 3 * 6;
    chunk->AllocateMeshBuffers(max_v);

    for (int face_direction = 1; face_direction <= 6; face_direction++) {
        for (int slice_idx = 0; slice_idx < CHUNK_SIZE; slice_idx++) {
            memset(greedy_mask, 0, CHUNK_SIZE * CHUNK_SIZE * sizeof(uint32_t));

            for (int v_idx = 0; v_idx < CHUNK_SIZE; v_idx++) {
                for (int u_idx = 0; u_idx < CHUNK_SIZE; u_idx++) {
                    int x, y, z, nx, ny, nz;
                    if (face_direction == 1)      { x = u_idx;     y = v_idx;     z = slice_idx; nx = x; ny = y; nz = z + 1; }
                    else if (face_direction == 2) { x = u_idx;     y = v_idx;     z = slice_idx; nx = x; ny = y; nz = z - 1; }
                    else if (face_direction == 3) { x = u_idx;     y = slice_idx; z = v_idx;     nx = x; ny = y + 1; nz = z; }
                    else if (face_direction == 4) { x = u_idx;     y = slice_idx; z = v_idx;     nx = x; ny = y - 1; nz = z; }
                    else if (face_direction == 5) { x = slice_idx; y = v_idx;     z = u_idx;     nx = x + 1; ny = y; nz = z; }
                    else                          { x = slice_idx; y = v_idx;     z = u_idx;     nx = x - 1; ny = y; nz = z; }

                    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) continue;
                    
                    int local_idx = x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE;
                    uint8_t current_id = chunk->blocks[local_idx];
                    if (current_id == 0 || !g_block_defs[current_id].registered) continue;
                    
                    int cur_render = g_block_defs[current_id].render_type;

                    if (cur_render == 1) continue;

                    uint8_t neighbor_id = 0;
                    bool neighbor_chunk_missing = false;

                    if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE) {
                        int wnx = chunk->cx * CHUNK_SIZE + nx;
                        int wny = chunk->cy * CHUNK_SIZE + ny;
                        int wnz = chunk->cz * CHUNK_SIZE + nz;
                        
                        int ncx, ncy, ncz, nlx, nly, nlz;
                        ChunkManager::WorldToChunk(wnx, wny, wnz, ncx, ncy, ncz, nlx, nly, nlz);
                        Chunk* nchunk = neighbors[face_direction];
                        neighbor_id = nchunk->blocks[nlx + nlz * CHUNK_SIZE + nly * CHUNK_SIZE * CHUNK_SIZE];
                    } else {
                        neighbor_id = chunk->blocks[nx + nz * CHUNK_SIZE + ny * CHUNK_SIZE * CHUNK_SIZE];
                    }

                    int neighbor_render = (neighbor_id == 0) || !g_block_defs[neighbor_id].registered ? 1 : g_block_defs[neighbor_id].render_type;
                    bool neighbor_is_transparent = (neighbor_render >= 1);

                    bool should_draw = false;
                    if (cur_render == 2) {
                        if (current_id != neighbor_id) should_draw = true;
                    } else {
                        if (neighbor_is_transparent) should_draw = true;
                    }

                    if (!should_draw) continue;

                    uint32_t mask_bit = (cur_render == 2) ? (1U << 31) : (1U << 30);
                    greedy_mask[v_idx * CHUNK_SIZE + u_idx] = (uint32_t)current_id | mask_bit;
                }
            }

            for (int v_idx = 0; v_idx < CHUNK_SIZE; v_idx++) {
                for (int u_idx = 0; u_idx < CHUNK_SIZE; u_idx++) {
                    uint32_t mask_val = greedy_mask[v_idx * CHUNK_SIZE + u_idx];
                    if (!mask_val) continue;

                    uint16_t block_id = (uint16_t)(mask_val & 0xFFFF);
                    bool is_semi = (mask_val & (1U << 31)) != 0;
                    bool can_greedy = g_block_defs[block_id].greedy_mesh;
                    
                    int width = 1, height = 1;
                    if (can_greedy) {
                        while (u_idx + width < CHUNK_SIZE && greedy_mask[v_idx * CHUNK_SIZE + u_idx + width] == mask_val) width++;
                        bool can_merge_row = true;
                        while (v_idx + height < CHUNK_SIZE && can_merge_row) {
                            for (int r = 0; r < width; r++) { if (greedy_mask[(v_idx + height) * CHUNK_SIZE + u_idx + r] != mask_val) { can_merge_row = false; break; } }
                            if (can_merge_row) height++;
                        }
                    }

                    float p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z;
                    // Our vertices are chunk-relative, 0 to 16
                    float off = -0.5f; // if blocks center at 0..15? The original code had 0..side_length-1 block indexing.
                    // Actually let's keep the user's `off` to maintain exact alignment
                    
                    if (face_direction == 1) {
                        p1x=u_idx+off;       p1y=v_idx+off;        p1z=slice_idx+1+off;
                        p2x=u_idx+width+off; p2y=v_idx+off;        p2z=slice_idx+1+off;
                        p3x=u_idx+width+off; p3y=v_idx+height+off; p3z=slice_idx+1+off;
                        p4x=u_idx+off;       p4y=v_idx+height+off; p4z=slice_idx+1+off;
                    } else if (face_direction == 2) {
                        p1x=u_idx+width+off; p1y=v_idx+off;        p1z=slice_idx+off;
                        p2x=u_idx+off;       p2y=v_idx+off;        p2z=slice_idx+off;
                        p3x=u_idx+off;       p3y=v_idx+height+off; p3z=slice_idx+off;
                        p4x=u_idx+width+off; p4y=v_idx+height+off; p4z=slice_idx+off;
                    } else if (face_direction == 3) {
                        p1x=u_idx+off;       p1y=slice_idx+1+off;  p1z=v_idx+height+off;
                        p2x=u_idx+width+off; p2y=slice_idx+1+off;  p2z=v_idx+height+off;
                        p3x=u_idx+width+off; p3y=slice_idx+1+off;  p3z=v_idx+off;
                        p4x=u_idx+off;       p4y=slice_idx+1+off;  p4z=v_idx+off;
                    } else if (face_direction == 4) {
                        p1x=u_idx+off;       p1y=slice_idx+off;    p1z=v_idx+off;
                        p2x=u_idx+width+off; p2y=slice_idx+off;    p2z=v_idx+off;
                        p3x=u_idx+width+off; p3y=slice_idx+off;    p3z=v_idx+height+off;
                        p4x=u_idx+off;       p4y=slice_idx+off;    p4z=v_idx+height+off;
                    } else if (face_direction == 5) {
                        p1x=slice_idx+1+off; p1y=v_idx+off;        p1z=u_idx+width+off;
                        p2x=slice_idx+1+off; p2y=v_idx+off;        p2z=u_idx+off;
                        p3x=slice_idx+1+off; p3y=v_idx+height+off; p3z=u_idx+off;
                        p4x=slice_idx+1+off; p4y=v_idx+height+off; p4z=u_idx+width+off;
                    } else {
                        p1x=slice_idx+off;   p1y=v_idx+off;        p1z=u_idx+off;
                        p2x=slice_idx+off;   p2y=v_idx+off;        p2z=u_idx+width+off;
                        p3x=slice_idx+off;   p3y=v_idx+height+off; p3z=u_idx+width+off;
                        p4x=slice_idx+off;   p4y=v_idx+height+off; p4z=u_idx+off;
                    }

                    if (is_semi) {
                        append_quad_to_mesh_buffers(chunk, trans_quad_index, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, &g_block_defs[block_id].uvs[face_direction], width, height, face_direction, block_id, true);
                        trans_quad_index++; trans_face_count++;
                    } else {
                        append_quad_to_mesh_buffers(chunk, current_quad_index, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, &g_block_defs[block_id].uvs[face_direction], width, height, face_direction, block_id, false);
                        current_quad_index++; total_face_count++;
                    }

                    for (int r=0; r<height; r++) for (int c=0; c<width; c++) greedy_mask[(v_idx+r)*CHUNK_SIZE + u_idx+c] = 0;
                }
            }
        }
    }

    chunk->opaque_quads     = current_quad_index;
    chunk->opaque_faces     = total_face_count;
    chunk->opaque_verts   = current_quad_index * 6;

    chunk->trans_quads   = trans_quad_index;
    chunk->trans_faces   = trans_face_count;
    chunk->trans_verts = trans_quad_index * 6;

    chunk->build_time   = (dmTime::GetTime() - start_time) / 1000.0;
    chunk->is_dirty = false;
}

void copy_array_to_buffer_stream(dmBuffer::HBuffer buffer, dmhash_t stream_name, const float* source_data, uint32_t vertex_count, uint32_t components_per_vertex) {
    float* stream_ptr = 0;
    uint32_t stream_count = 0, stream_components = 0, stream_stride = 0;
    dmBuffer::Result res = dmBuffer::GetStream(buffer, stream_name, (void**)&stream_ptr, &stream_count, &stream_components, &stream_stride);
    if (res != dmBuffer::RESULT_OK || !stream_ptr || stream_count < vertex_count) return;
    for (uint32_t i = 0; i < vertex_count; i++) {
        for (uint32_t c = 0; c < stream_components && c < components_per_vertex; c++) {
            stream_ptr[i * stream_stride + c] = source_data[i * components_per_vertex + c];
        }
    }
}

int Lua_GetMaxVertices(lua_State* L) {
    uint32_t max_verts = CHUNK_VOLUME * 3 * 6;
    lua_pushinteger(L, max_verts);
    return 1;
}

int Lua_GetMeshDebugQuads(lua_State* L) {
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
    g_debug_quads.clear();
    return 1;
}
