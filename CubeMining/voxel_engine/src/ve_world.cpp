#include "ve_world.h"

// Global Variables Definitions
BlockDef g_block_defs[256];
int g_seed = 12345;
bool g_ao_enabled = true;
int g_light_mode = 1;

std::vector<NPCInfo> g_npcs;
std::vector<DebugQuad> g_debug_quads;
bool g_debug_enabled = false;

int Lua_RegisterBlockType(lua_State *L) {
  int id = luaL_checkinteger(L, 1);
  if (id < 0 || id > 255)
    return 0;

  BlockDef &bd = g_block_defs[id];
  bd.registered = true;
  bd.render_type = luaL_checkinteger(L, 2);
  bd.light_level = luaL_checkinteger(L, 3);

  if (lua_isboolean(L, 8))
    bd.greedy_mesh = lua_toboolean(L, 8);
  else
    bd.greedy_mesh = true;

  if (lua_isstring(L, 4))
    bd.name_hash = dmHashString64(lua_tostring(L, 4));
  else
    bd.name_hash = 0;

  if (lua_isstring(L, 5))
    bd.hit_sound_hash = dmHashString64(lua_tostring(L, 5));
  else
    bd.hit_sound_hash = 0;

  if (lua_isstring(L, 6))
    bd.break_sound_hash = dmHashString64(lua_tostring(L, 6));
  else
    bd.break_sound_hash = 0;

  if (lua_istable(L, 7)) {
    for (int face = 1; face <= 6; face++) {
      lua_rawgeti(L, 7, face);
      if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1);
        bd.uvs[face].u = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 2);
        bd.uvs[face].v = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 3);
        bd.uvs[face].w = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 4);
        bd.uvs[face].h = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
    }
  }
  return 0;
}

int Lua_GetBlockInfo(lua_State *L) {
  int id = luaL_checkinteger(L, 1);
  if (id < 0 || id > 255 || !g_block_defs[id].registered) {
    lua_pushnil(L);
    return 1;
  }
  const BlockDef &bd = g_block_defs[id];
  lua_newtable(L);
  lua_pushinteger(L, bd.render_type);
  lua_setfield(L, -2, "render_type");
  lua_pushboolean(L, bd.greedy_mesh);
  lua_setfield(L, -2, "greedy_mesh");
  lua_pushinteger(L, bd.light_level);
  lua_setfield(L, -2, "light_level");
  if (bd.name_hash != 0) {
    dmScript::PushHash(L, bd.name_hash);
    lua_setfield(L, -2, "name");
  }
  if (bd.hit_sound_hash != 0) {
    dmScript::PushHash(L, bd.hit_sound_hash);
    lua_setfield(L, -2, "hit_sound");
  }
  if (bd.break_sound_hash != 0) {
    dmScript::PushHash(L, bd.break_sound_hash);
    lua_setfield(L, -2, "break_sound");
  }
  lua_newtable(L);
  for (int face = 1; face <= 6; face++) {
    lua_newtable(L);
    lua_pushnumber(L, bd.uvs[face].u);
    lua_setfield(L, -2, "u");
    lua_pushnumber(L, bd.uvs[face].v);
    lua_setfield(L, -2, "v");
    lua_pushnumber(L, bd.uvs[face].w);
    lua_setfield(L, -2, "w");
    lua_pushnumber(L, bd.uvs[face].h);
    lua_setfield(L, -2, "h");
    lua_rawseti(L, -2, face);
  }
  lua_setfield(L, -2, "uvs");
  return 1;
}

int Lua_SetBlockInWorld(lua_State *L) {
  int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2),
      z = luaL_checkinteger(L, 3);
  int id = luaL_checkinteger(L, 4);
  SetBlock(x, y, z, (uint8_t)id);
  TriggerAsyncMeshUpdate();
  return 0;
}

int Lua_GetBlockFromWorld(lua_State *L) {
  int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2),
      z = luaL_checkinteger(L, 3);
  lua_pushinteger(L, GetBlock(x, y, z));
  return 1;
}
