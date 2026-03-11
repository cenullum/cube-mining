#include "ve_world.h"
#include <stdlib.h>
#include <string.h>

void alloc_mesh_buffers(int grid_size) {
    uint32_t max_quads = grid_size * grid_size * grid_size * 3;
    uint32_t max_verts = max_quads * 6;
    g_vertex_positions   = (float*)realloc(g_vertex_positions,   max_verts * 3 * sizeof(float));
    g_vertex_uvs_base    = (float*)realloc(g_vertex_uvs_base,    max_verts * 4 * sizeof(float));
    g_vertex_uvs_local   = (float*)realloc(g_vertex_uvs_local,   max_verts * 4 * sizeof(float));
    g_vertex_face_ids    = (float*)realloc(g_vertex_face_ids,    max_verts * 1 * sizeof(float));
}

static void append_quad_to_mesh_buffers(int quad_idx, float p1x, float p1y, float p1z, float p2x, float p2y, float p2z,
    float p3x, float p3y, float p3z, float p4x, float p4y, float p4z,
    const UVData* uv_data, int quad_width, int quad_height, int face_direction) {

    int base_v3 = quad_idx * 18;
    int base_v4 = quad_idx * 24;
    int base_v1 = quad_idx * 6;
    
    float px[4] = {p1x, p2x, p3x, p4x};
    float py[4] = {p1y, p2y, p3y, p4y};
    float pz[4] = {p1z, p2z, p3z, p4z};
    int indices[6] = {0, 1, 2, 0, 2, 3};

    float lux[4] = {0, 1, 1, 0};
    float luy[4] = {0, 0, 1, 1};

    for(int i = 0; i < 6; i++) {
        int idx = indices[i];
        int v_off = i;
        
        g_vertex_positions[base_v3 + v_off * 3 + 0] = px[idx];
        g_vertex_positions[base_v3 + v_off * 3 + 1] = py[idx];
        g_vertex_positions[base_v3 + v_off * 3 + 2] = pz[idx];

        g_vertex_uvs_base[base_v4 + v_off * 4 + 0] = uv_data->u;
        g_vertex_uvs_base[base_v4 + v_off * 4 + 1] = uv_data->v;
        g_vertex_uvs_base[base_v4 + v_off * 4 + 2] = uv_data->w;
        g_vertex_uvs_base[base_v4 + v_off * 4 + 3] = uv_data->h;

        g_vertex_uvs_local[base_v4 + v_off * 4 + 0] = lux[idx];
        g_vertex_uvs_local[base_v4 + v_off * 4 + 1] = luy[idx];
        g_vertex_uvs_local[base_v4 + v_off * 4 + 2] = (float)quad_width;
        g_vertex_uvs_local[base_v4 + v_off * 4 + 3] = (float)quad_height;

        g_vertex_face_ids[base_v1 + v_off] = (float)face_direction;
    }

    if (g_debug_enabled) {
        DebugQuad dq;
        dq.x1 = p1x; dq.y1 = p1y; dq.z1 = p1z;
        dq.x2 = p2x; dq.y2 = p2y; dq.z2 = p2z;
        dq.x3 = p3x; dq.y3 = p3y; dq.z3 = p3z;
        dq.x4 = p4x; dq.y4 = p4y; dq.z4 = p4z;
        dq.dir = face_direction;
        g_debug_quads.push_back(dq);
    }
}

void execute_mesh_generation_pipeline(const uint8_t* world_blocks, const uint8_t* sun_light, const uint8_t* source_light,
    int side_length, bool ao_enabled, int light_mode) {

    uint64_t start_time = dmTime::GetTime();
    int current_quad_index = 0, total_face_count = 0;
    static uint32_t greedy_mask[64 * 64];

    g_debug_quads.clear();

    for (int face_direction = 1; face_direction <= 6; face_direction++) {
        for (int slice_idx = 0; slice_idx < side_length; slice_idx++) {
            memset(greedy_mask, 0, side_length * side_length * sizeof(uint32_t));

            for (int v_idx = 0; v_idx < side_length; v_idx++) {
                for (int u_idx = 0; u_idx < side_length; u_idx++) {
                    int x, y, z, nx, ny, nz;
                    if (face_direction == 1)      { x = u_idx;     y = v_idx;     z = slice_idx; nx = x; ny = y; nz = z + 1; }
                    else if (face_direction == 2) { x = u_idx;     y = v_idx;     z = slice_idx; nx = x; ny = y; nz = z - 1; }
                    else if (face_direction == 3) { x = u_idx;     y = slice_idx; z = v_idx;     nx = x; ny = y + 1; nz = z; }
                    else if (face_direction == 4) { x = u_idx;     y = slice_idx; z = v_idx;     nx = x; ny = y - 1; nz = z; }
                    else if (face_direction == 5) { x = slice_idx; y = v_idx;     z = u_idx;     nx = x + 1; ny = y; nz = z; }
                    else                          { x = slice_idx; y = v_idx;     z = u_idx;     nx = x - 1; ny = y; nz = z; }

                    if (x < 0 || x >= side_length || y < 0 || y >= side_length || z < 0 || z >= side_length) continue;
                    uint8_t current_id = world_blocks[calculate_block_index(x, y, z, side_length)];
                    if (current_id == 0 || !g_block_defs[current_id].registered || g_block_defs[current_id].transparent) continue;

                    uint8_t neighbor_id = (nx < 0 || nx >= side_length || ny < 0 || ny >= side_length || nz < 0 || nz >= side_length) ? 0 : world_blocks[calculate_block_index(nx, ny, nz, side_length)];
                    bool neighbor_is_transparent = (neighbor_id == 0) || !g_block_defs[neighbor_id].registered || g_block_defs[neighbor_id].transparent;
                    if (!neighbor_is_transparent) continue;

                    greedy_mask[v_idx * side_length + u_idx] = (uint32_t)current_id | (1U << 30);
                }
            }

            for (int v_idx = 0; v_idx < side_length; v_idx++) {
                for (int u_idx = 0; u_idx < side_length; u_idx++) {
                    uint32_t mask_val = greedy_mask[v_idx * side_length + u_idx];
                    if (!mask_val) continue;

                    uint16_t block_id = (uint16_t)(mask_val & 0xFFFF);
                    
                    int width = 1, height = 1;
                    while (u_idx + width < side_length && greedy_mask[v_idx * side_length + u_idx + width] == mask_val) width++;
                    bool can_merge_row = true;
                    while (v_idx + height < side_length && can_merge_row) {
                        for (int r = 0; r < width; r++) { if (greedy_mask[(v_idx + height) * side_length + u_idx + r] != mask_val) { can_merge_row = false; break; } }
                        if (can_merge_row) height++;
                    }

                    float p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z;
                    float off=-0.5f;
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

                    append_quad_to_mesh_buffers(current_quad_index, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, &g_block_defs[block_id].uvs[face_direction], width, height, face_direction);
                    current_quad_index++;
                    for (int r=0; r<height; r++) for (int c=0; c<width; c++) greedy_mask[(v_idx+r)*side_length + u_idx+c] = 0;
                    total_face_count++;
                }
            }
        }
    }

    g_result_quad_count   = current_quad_index;
    g_result_face_count   = total_face_count;
    g_result_vertex_count = current_quad_index * 6;
    g_result_build_time   = (dmTime::GetTime() - start_time) / 1000.0;
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
    uint32_t max_verts = (uint32_t)g_grid_size * g_grid_size * g_grid_size * 3 * 6;
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
    return 1;
}
