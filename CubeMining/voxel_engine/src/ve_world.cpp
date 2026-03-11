#include "ve_world.h"

// Global Variables Definitions
const int MAX_GRID_SIZE = 64;
const int MAX_BLOCKS = MAX_GRID_SIZE * MAX_GRID_SIZE * MAX_GRID_SIZE;

int g_grid_size = 0;
uint8_t g_blocks[MAX_BLOCKS];
uint8_t g_sun_light[MAX_BLOCKS];
uint8_t g_source_light[MAX_BLOCKS];
BlockDef g_block_defs[256];
int g_seed = 12345;
bool g_ao_enabled = true;
int g_light_mode = 1;

std::vector<NPCInfo> g_npcs;
std::vector<DebugQuad> g_debug_quads;
bool g_debug_enabled = false;

// Mesh related globals
float* g_vertex_positions = 0;
float* g_vertex_uvs_base = 0;
float* g_vertex_uvs_local = 0;
float* g_vertex_face_ids = 0;
uint32_t g_result_quad_count = 0;
uint32_t g_result_face_count = 0;
uint32_t g_result_vertex_count = 0;
double g_result_build_time = 0;

int calculate_block_index(int x, int y, int z, int side_length) {
    return x + y * side_length + z * side_length * side_length;
}

uint8_t safe_get_block(int x, int y, int z, int s) {
    if (x < 0 || x >= s || y < 0 || y >= s || z < 0 || z >= s) return 0;
    return g_blocks[calculate_block_index(x, y, z, s)];
}

uint8_t GetBlock(int x, int y, int z) {
    return safe_get_block(x, y, z, g_grid_size);
}

void SetBlock(int x, int y, int z, uint8_t id) {
    if (x < 0 || x >= g_grid_size || y < 0 || y >= g_grid_size || z < 0 || z >= g_grid_size) return;
    g_blocks[calculate_block_index(x, y, z, g_grid_size)] = id;
}

bool IsSolid(int x, int y, int z) {
    if (x < 0 || x >= g_grid_size || y < 0 || y >= g_grid_size || z < 0 || z >= g_grid_size) {
        return false;
    }
    int idx = calculate_block_index(x, y, z, g_grid_size);
    int id = g_blocks[idx];
    if (id == 0) return false;
    return g_block_defs[id].registered && g_block_defs[id].solid;
}

int Lua_RegisterBlockType(lua_State* L) {
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

int Lua_GetBlockInfo(lua_State* L) {
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

int Lua_SetBlockInWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    int id = luaL_checkinteger(L, 4);
    SetBlock(x, y, z, (uint8_t)id);
    return 0;
}

int Lua_GetBlockFromWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    lua_pushinteger(L, GetBlock(x, y, z));
    return 1;
}
