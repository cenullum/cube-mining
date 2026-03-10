import re

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'r') as f:
    content = f.read()

# Add Lua_UpdateLightBuffer
new_func = """static int Lua_UpdateLightBuffer(lua_State* L) {
    if (!lua_isuserdata(L, 1)) return 0;
    dmScript::LuaHBuffer* luabuf = dmScript::CheckBuffer(L, 1);
    if (!luabuf) return 0;
    dmBuffer::HBuffer buffer_handle = luabuf->m_Buffer;
    
    uint8_t* stream_ptr = 0;
    uint32_t stream_count = 0, stream_components = 0, stream_stride = 0;
    dmBuffer::Result res = dmBuffer::GetStream(buffer_handle, dmHashString64("rgba"), (void**)&stream_ptr, &stream_count, &stream_components, &stream_stride);
    
    if (res != dmBuffer::RESULT_OK || !stream_ptr || stream_count < (uint32_t)(g_grid_size * g_grid_size * g_grid_size)) {
        dmLogError("terrain_engine: Invalid light buffer stream or size! Expected %d, got %d", g_grid_size * g_grid_size * g_grid_size, stream_count);
        return 0;
    }

    for (int y = 0; y < g_grid_size; ++y) {
        for (int z = 0; z < g_grid_size; ++z) {
            for (int x = 0; x < g_grid_size; ++x) {
                int block_idx = calculate_block_index(x, y, z, g_grid_size);
                int sun = g_sun_light[block_idx];
                int torch = g_source_light[block_idx];
                
                float sun_f = sun / 15.0f;
                float torch_f = torch / 15.0f;
                
                float out_r = sun_f + torch_f;
                float out_g = sun_f + torch_f * 0.9f;
                float out_b = sun_f + torch_f * 0.6f;
                
                out_r = out_r > 1.0f ? 1.0f : out_r;
                out_g = out_g > 1.0f ? 1.0f : out_g;
                out_b = out_b > 1.0f ? 1.0f : out_b;
                
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

static int Lua_PerformLightingPassSync"""

content = content.replace("static int Lua_PerformLightingPassSync", new_func)

# Expose to Lua
content = content.replace('{"recalc_lighting_sync", Lua_PerformLightingPassSync},', '{"update_light_buffer",  Lua_UpdateLightBuffer},\n    {"recalc_lighting_sync", Lua_PerformLightingPassSync},')

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'w') as f:
    f.write(content)
