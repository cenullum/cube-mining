#define EXTENSION_NAME voxel_engine
#define LIB_NAME "voxel_engine"

#include "ve_world.h"
#include "chunk_manager.h"
#include <cstdint>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/time.h>

// Globals defined here
BlockDef g_block_defs[256];
int g_seed = 0;
bool g_ao_enabled = true;
int g_light_mode = 1;
bool g_debug_enabled = false;
std::vector<NPCInfo> g_npcs;
std::vector<DebugQuad> g_debug_quads;

// Global Wrappers
uint8_t GetBlock(int x, int y, int z) { return ChunkManager::GetBlock(x, y, z); }
void SetBlock(int x, int y, int z, uint8_t id) { ChunkManager::SetBlock(x, y, z, id); }
bool IsSolid(int x, int y, int z) { return ChunkManager::IsSolid(x, y, z); }

// Worker Thread State
static dmThread::Thread g_worker_thread = 0;
static dmMutex::HMutex g_mutex = 0;
static volatile bool g_worker_running = false;

static void WorkerThreadLoop(void *arg) {
  extern void ProcessLightQueues();
  while (g_worker_running) {
    ChunkManager::ProcessQueues();
    ProcessLightQueues();
    dmTime::Sleep(10); // 10ms rest
  }
}

void TriggerAsyncMeshUpdate() {} // deprecated, ChunkManager handles this internally

// Lua Engine
static int Lua_InitializeVoxelEngine(lua_State *L) {
  ChunkManager::view_distance = luaL_optinteger(L, 1, 2);
  g_seed = luaL_checkinteger(L, 2);

  g_block_defs[0].registered = true;
  g_block_defs[0].render_type = 1; 
  g_block_defs[0].light_level = 0;

  ChunkManager::Init();
  
  extern dmMutex::HMutex g_light_mutex;
  if (!g_light_mutex) g_light_mutex = dmMutex::New();

  if (!g_mutex) g_mutex = dmMutex::New();
  if (!g_worker_running) {
    g_worker_running = true;
    g_worker_thread = dmThread::New(WorkerThreadLoop, 0x80000, 0, "VoxelWorker");
  }
  return 0;
}

static int Lua_ShutdownVoxelEngine(lua_State *L) {
  g_worker_running = false;
  if (g_worker_thread) {
    dmThread::Join(g_worker_thread);
    g_worker_thread = 0;
  }
  if (g_mutex) {
    dmMutex::Delete(g_mutex);
    g_mutex = 0;
  }
  ShutdownParticles();
  ChunkManager::Shutdown();
  return 0;
}

static int Lua_UpdateWorld(lua_State* L) {
    dmVMath::Vector3 pos = *dmScript::ToVector3(L, 1);
    g_player_pos = pos;
    ChunkManager::Update(pos.getX(), pos.getY(), pos.getZ());
    return 0;
}

static int Lua_RequestAsyncMeshUpdate(lua_State *L) {
  g_ao_enabled = lua_toboolean(L, 1);
  g_light_mode = luaL_checkinteger(L, 2);
  g_debug_enabled = lua_toboolean(L, 3);
  
  // Now we just queue a full relight/remesh if settings changed
  dmMutex::Lock(ChunkManager::mutex);
  for(auto& pair : ChunkManager::active_chunks) {
      ChunkManager::QueueLightingUpdate(pair.second);
  }
  dmMutex::Unlock(ChunkManager::mutex);
  return 0;
}

int Lua_PollChunkUpdates(lua_State *L) {
  int count_returned = 0;
  lua_newtable(L);

  LuaUpdate update;
  // Send up to 20 updates per frame to avoid choking Lua
  while(count_returned < 20 && ChunkManager::PollUpdate(update)) {
      lua_newtable(L);
      if (update.type == ChunkUpdateType::SPAWN) lua_pushstring(L, "SPAWN");
      else if (update.type == ChunkUpdateType::DESPAWN) lua_pushstring(L, "DESPAWN");
      else lua_pushstring(L, "MESH");
      lua_setfield(L, -2, "type");
      
      lua_pushinteger(L, update.cx); lua_setfield(L, -2, "cx");
      lua_pushinteger(L, update.cy); lua_setfield(L, -2, "cy");
      lua_pushinteger(L, update.cz); lua_setfield(L, -2, "cz");
      
      if (update.type == ChunkUpdateType::MESH && update.chunk_ptr) {
          Chunk* c = update.chunk_ptr;
          lua_pushinteger(L, c->light_tex_u); lua_setfield(L, -2, "light_tex_u");
          lua_pushinteger(L, c->light_tex_v); lua_setfield(L, -2, "light_tex_v");

          static dmBuffer::StreamDeclaration streams_decl[] = {
              {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
              {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 4},
              {dmHashString64("texcoord1"), dmBuffer::VALUE_TYPE_FLOAT32, 4},
              {dmHashString64("texcoord2"), dmBuffer::VALUE_TYPE_FLOAT32, 2}};

          // Opaque
          if (c->opaque_verts > 0) {
              dmBuffer::HBuffer buf = 0;
              if (dmBuffer::Create(c->opaque_verts, streams_decl, 4, &buf) == dmBuffer::RESULT_OK && buf) {
                  copy_array_to_buffer_stream(buf, dmHashString64("position"), c->opaque_pos, c->opaque_verts, 3);
                  copy_array_to_buffer_stream(buf, dmHashString64("texcoord0"), c->opaque_uvb, c->opaque_verts, 4);
                  copy_array_to_buffer_stream(buf, dmHashString64("texcoord1"), c->opaque_uvl, c->opaque_verts, 4);
                  copy_array_to_buffer_stream(buf, dmHashString64("texcoord2"), c->opaque_face, c->opaque_verts, 2);
                  
                  // Frustum culling metadata - the renderer handles chunks at local 0,0,0 to 16,16,16
                  float aabb[6] = {-0.5f, -0.5f, -0.5f, (float)CHUNK_SIZE-0.5f, (float)CHUNK_SIZE-0.5f, (float)CHUNK_SIZE-0.5f};
                  dmBuffer::SetMetaData(buf, dmHashString64("AABB"), aabb, 6, dmBuffer::VALUE_TYPE_FLOAT32);
                  dmBuffer::UpdateContentVersion(buf);
                  
                  dmScript::PushBuffer(L, dmScript::LuaHBuffer(buf, dmScript::OWNER_LUA));
                  lua_setfield(L, -2, "opaque_buffer");
              }
          }
          // Trans
          if (c->trans_verts > 0) {
              dmBuffer::HBuffer buf = 0;
              if (dmBuffer::Create(c->trans_verts, streams_decl, 4, &buf) == dmBuffer::RESULT_OK && buf) {
                  copy_array_to_buffer_stream(buf, dmHashString64("position"), c->trans_pos, c->trans_verts, 3);
                  copy_array_to_buffer_stream(buf, dmHashString64("texcoord0"), c->trans_uvb, c->trans_verts, 4);
                  copy_array_to_buffer_stream(buf, dmHashString64("texcoord1"), c->trans_uvl, c->trans_verts, 4);
                  copy_array_to_buffer_stream(buf, dmHashString64("texcoord2"), c->trans_face, c->trans_verts, 2);
                  
                  float aabb[6] = {-0.5f, -0.5f, -0.5f, (float)CHUNK_SIZE-0.5f, (float)CHUNK_SIZE-0.5f, (float)CHUNK_SIZE-0.5f};
                  dmBuffer::SetMetaData(buf, dmHashString64("AABB"), aabb, 6, dmBuffer::VALUE_TYPE_FLOAT32);
                  dmBuffer::UpdateContentVersion(buf);
                  
                  dmScript::PushBuffer(L, dmScript::LuaHBuffer(buf, dmScript::OWNER_LUA));
                  lua_setfield(L, -2, "trans_buffer");
              }
          }
          lua_pushinteger(L, c->opaque_verts + c->trans_verts); lua_setfield(L, -2, "total_verts");
          lua_pushinteger(L, c->opaque_faces + c->trans_faces); lua_setfield(L, -2, "total_faces");
      }
      lua_rawseti(L, -2, ++count_returned);
  }
  
  if (count_returned == 0) {
      lua_pop(L, 1);
      lua_pushnil(L);
  }
  return 1;
}

int Lua_RegisterBlockType(lua_State* L) {
    uint8_t id = luaL_checkinteger(L, 1);
    g_block_defs[id].registered = true;
    g_block_defs[id].render_type = luaL_checkinteger(L, 2);
    g_block_defs[id].light_level = luaL_optinteger(L, 3, 0);
    g_block_defs[id].greedy_mesh = lua_toboolean(L, 4);
    
    // Assuming UVS is a table of 6 faces where each is {u,v,w,h}
    if (lua_istable(L, 5)) {
        for(int i=1; i<=6; i++) {
            lua_rawgeti(L, 5, i);
            if(lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1); g_block_defs[id].uvs[i].u = lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); g_block_defs[id].uvs[i].v = lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 3); g_block_defs[id].uvs[i].w = lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 4); g_block_defs[id].uvs[i].h = lua_tonumber(L, -1); lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
    return 0;
}

int Lua_GetBlockInfo(lua_State* L) {
    uint8_t id = luaL_checkinteger(L, 1);
    lua_newtable(L);
    lua_pushboolean(L, g_block_defs[id].registered); lua_setfield(L, -2, "registered");
    lua_pushinteger(L, g_block_defs[id].render_type); lua_setfield(L, -2, "render_type");
    lua_pushinteger(L, g_block_defs[id].light_level); lua_setfield(L, -2, "light_level");
    return 1;
}

int Lua_SetBlockInWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int z = luaL_checkinteger(L, 3);
    uint8_t id = luaL_checkinteger(L, 4);
    SetBlock(x, y, z, id);
    return 0;
}

static int Lua_SetViewDistance(lua_State *L) {
    ChunkManager::view_distance = luaL_checkinteger(L, 1);
    return 0;
}

int Lua_GetBlockFromWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int z = luaL_checkinteger(L, 3);
    uint8_t id = GetBlock(x, y, z);
    lua_pushinteger(L, id);
    return 1;
}

static const luaL_reg Module_methods[] = {
    {"init", Lua_InitializeVoxelEngine},
    {"shutdown", Lua_ShutdownVoxelEngine},
    {"update_world", Lua_UpdateWorld},
    {"register_block", Lua_RegisterBlockType},
    {"get_block_info", Lua_GetBlockInfo},
    {"set_block", Lua_SetBlockInWorld},
    {"get_block", Lua_GetBlockFromWorld},
    {"set_view_distance", Lua_SetViewDistance},
    {"get_ambient_light", Lua_GetAmbientLight},
    {"get_max_vertices", Lua_GetMaxVertices},
    {"request_mesh_update", Lua_RequestAsyncMeshUpdate},
    {"poll_chunk_updates", Lua_PollChunkUpdates},
    {"get_debug_quads", Lua_GetMeshDebugQuads},
    {"update_light_buffer", Lua_UpdateLightBuffer},
    {"recalc_lighting_sync", Lua_PerformLightingPassSync},
    {"move_and_slide", Lua_MoveAndSlide},
    {"check_collision", Lua_CheckCollision},
    {"register_npc", Lua_RegisterNPC},
    {"unregister_npc", Lua_UnregisterNPC},
    {"explode", Lua_Explosion},
    {"shoot", Lua_ShootRay},
    {"spawn_block_particles", Lua_SpawnBlockParticles},
    {"register_particle", Lua_RegisterParticle},
    {"get_particle_init_data", Lua_GetParticleInitData},
    {"set_particle_manager", Lua_SetParticleManager},
    {"pull_particle_events", Lua_PullParticleEvents},
    {0, 0}};

static dmExtension::Result AppInit(dmExtension::AppParams *params) { return dmExtension::RESULT_OK; }

static dmExtension::Result Init(dmExtension::Params *params) {
  luaL_register(params->m_L, LIB_NAME, Module_methods);
  lua_pop(params->m_L, 1);
  InitParticles();
  return dmExtension::RESULT_OK;
}

static uint64_t g_last_time = 0;
static dmExtension::Result UpdateExtension(dmExtension::Params *params) {
  uint64_t now = dmTime::GetTime();
  if (g_last_time == 0 || (now - g_last_time) > 1000000) { g_last_time = now; return dmExtension::RESULT_OK; }
  float dt = (float)(now - g_last_time) / 1000000.0f;
  g_last_time = now;

  UpdateAllNPCs(dt);
  ProcessPendingSpawns(true);
  UpdateParticles(dt);

  return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalize(dmExtension::Params *params) { return dmExtension::RESULT_OK; }

static dmExtension::Result AppFinalFunc(dmExtension::AppParams *params) {
  if (g_worker_running) {
    g_worker_running = false;
    if (g_worker_thread) { dmThread::Join(g_worker_thread); g_worker_thread = 0; }
    if (g_mutex) { dmMutex::Delete(g_mutex); g_mutex = 0; }
  }
  return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInit, AppFinalFunc, Init, UpdateExtension, 0, Finalize)
