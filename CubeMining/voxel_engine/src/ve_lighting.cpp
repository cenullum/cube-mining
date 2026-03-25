#include "ve_world.h"
#include <string.h>

void perform_lighting_pass(uint8_t* blocks, uint8_t* sun_light_data, uint8_t* source_light_data, int side_length) {
    int total_blocks = side_length * side_length * side_length;
    memset(sun_light_data, 0, total_blocks);
    memset(source_light_data, 0, total_blocks);

    static int source_queue[64 * 64 * 64];
    static int sun_queue[64 * 64 * 64];
    int source_queue_length = 0, sun_queue_length = 0;

    for (int i = 0; i < total_blocks; i++) {
        uint8_t block_id = blocks[i];
        if (g_block_defs[block_id].registered && g_block_defs[block_id].light_level > 0) {
            source_light_data[i] = g_block_defs[block_id].light_level;
            if (source_queue_length < MAX_BLOCKS) {
                source_queue[source_queue_length++] = i;
            }
        }
    }

    for (int x = 0; x < side_length; x++) {
        for (int z = 0; z < side_length; z++) {
            int current_sun_light = 15;
            for (int y = side_length - 1; y >= 0; y--) {
                int cur_idx = calculate_block_index(x, y, z, side_length);
                uint8_t block_id = blocks[cur_idx];
                if (g_block_defs[block_id].registered && g_block_defs[block_id].render_type == 0) {
                    current_sun_light = 0;
                }
                sun_light_data[cur_idx] = current_sun_light;
                if (current_sun_light > 0) {
                    if (sun_queue_length < MAX_BLOCKS) {
                        sun_queue[sun_queue_length++] = cur_idx;
                    }
                }
            }
        }
    }

    static const int neighbor_offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    // BFS Source
    for (int i = 0; i < source_queue_length; i++) {
        int cur_idx = source_queue[i];
        int cz  = cur_idx / (side_length * side_length);
        int rem = cur_idx % (side_length * side_length);
        int cy  = rem / side_length;
        int cx  = rem % side_length;
        int current_level = source_light_data[cur_idx];
        if (current_level <= 1) continue;
        
        for (int d = 0; d < 6; d++) {
            int nx = cx + neighbor_offsets[d][0], ny = cy + neighbor_offsets[d][1], nz = cz + neighbor_offsets[d][2];
            if (nx < 0 || nx >= side_length || ny < 0 || ny >= side_length || nz < 0 || nz >= side_length) continue;
            
            int n_idx = calculate_block_index(nx, ny, nz, side_length);
            uint8_t neighbor_id = blocks[n_idx];
            bool is_transparent = !g_block_defs[neighbor_id].registered || (g_block_defs[neighbor_id].render_type >= 1);
            
            if (is_transparent && source_light_data[n_idx] < current_level - 1) {
                source_light_data[n_idx] = current_level - 1;
                if (source_queue_length < MAX_BLOCKS) {
                    source_queue[source_queue_length++] = n_idx;
                }
            }
        }
    }

    // BFS Sun
    for (int i = 0; i < sun_queue_length; i++) {
        int cur_idx = sun_queue[i];
        int cz  = cur_idx / (side_length * side_length);
        int rem = cur_idx % (side_length * side_length);
        int cy  = rem / side_length;
        int cx  = rem % side_length;
        int current_level = sun_light_data[cur_idx];
        if (current_level <= 1) continue;
        
        for (int d = 0; d < 6; d++) {
            int nx = cx + neighbor_offsets[d][0], ny = cy + neighbor_offsets[d][1], nz = cz + neighbor_offsets[d][2];
            if (nx < 0 || nx >= side_length || ny < 0 || ny >= side_length || nz < 0 || nz >= side_length) continue;
            
            int n_idx = calculate_block_index(nx, ny, nz, side_length);
            uint8_t neighbor_id = blocks[n_idx];
            bool is_transparent = !g_block_defs[neighbor_id].registered || (g_block_defs[neighbor_id].render_type >= 1);
            
            if (is_transparent && sun_light_data[n_idx] < current_level - 1) {
                sun_light_data[n_idx] = current_level - 1;
                if (sun_queue_length < MAX_BLOCKS) {
                    sun_queue[sun_queue_length++] = n_idx;
                }
            }
        }
    }
}

int Lua_GetAmbientLight(lua_State* L) {
    dmVMath::Vector3 pos = *dmScript::ToVector3(L, 1);
    int x = (int)floorf(pos.getX() + 0.5f);
    int y = (int)floorf(pos.getY() + 0.5f);
    int z = (int)floorf(pos.getZ() + 0.5f);

    float sun_f = 1.0f, source_f = 0.0f;
    if (x >= 0 && x < g_grid_size && y >= 0 && y < g_grid_size && z >= 0 && z < g_grid_size) {
        int idx = calculate_block_index(x, y, z, g_grid_size);
        sun_f = (float)g_sun_light[idx] / 15.0f;
        source_f = (float)g_source_light[idx] / 15.0f;
    }
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

int Lua_PerformLightingPassSync(lua_State* L) {
    perform_lighting_pass(g_blocks, g_sun_light, g_source_light, g_grid_size);
    return 0;
}
