#define EXTENSION_NAME voxel_engine
#define LIB_NAME "voxel_engine"

#include "ve_world.h"
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/time.h>

// Worker Thread State
static dmThread::Thread g_worker_thread = 0;
static dmMutex::HMutex g_mutex = 0;
static volatile bool g_worker_running = false;
static volatile bool g_worker_has_work = false;
static volatile bool g_worker_result_ready = false;

static void WorkerThreadLoop(void *arg) {
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
      execute_mesh_generation_pipeline(g_blocks, g_sun_light, g_source_light,
                                       g_grid_size, g_ao_enabled, g_light_mode);

      dmMutex::Lock(g_mutex);
      g_worker_result_ready = true;
      dmMutex::Unlock(g_mutex);
    }
    dmTime::Sleep(10); // 0.01ms
  }
}

void TriggerAsyncMeshUpdate() {
  if (!g_worker_thread || !g_mutex)
    return;
  dmMutex::Lock(g_mutex);
  g_worker_has_work = true;
  g_worker_result_ready = false;
  dmMutex::Unlock(g_mutex);
}

// Lua Bridge
static int Lua_InitializeVoxelEngine(lua_State *L) {
  int side_length = luaL_checkinteger(L, 1);
  if (side_length > MAX_GRID_SIZE)
    side_length = MAX_GRID_SIZE;
  g_grid_size = side_length;
  g_seed = luaL_checkinteger(L, 2);

  // Explicitly initialize Air block def
  g_block_defs[0].registered = true;
  g_block_defs[0].render_type = 1; // 1 = Transparent
  g_block_defs[0].light_level = 0;

  initialize_world_terrain_data();
  alloc_mesh_buffers(side_length);

  if (!g_mutex)
    g_mutex = dmMutex::New();
  if (!g_worker_running) {
    g_worker_running = true;
    g_worker_has_work = false;
    g_worker_result_ready = false;
    g_worker_thread =
        dmThread::New(WorkerThreadLoop, 0x80000, 0, "VoxelWorker");
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

  free(g_vertex_positions);
  g_vertex_positions = 0;
  free(g_vertex_uvs_base);
  g_vertex_uvs_base = 0;
  free(g_vertex_uvs_local);
  g_vertex_uvs_local = 0;
  free(g_vertex_face_ids);
  g_vertex_face_ids = 0;

  free(g_trans_vertex_positions);
  g_trans_vertex_positions = 0;
  free(g_trans_vertex_uvs_base);
  g_trans_vertex_uvs_base = 0;
  free(g_trans_vertex_uvs_local);
  g_trans_vertex_uvs_local = 0;
  free(g_trans_vertex_face_ids);
  g_trans_vertex_face_ids = 0;

  return 0;
}

static int Lua_RequestAsyncMeshUpdate(lua_State *L) {
  if (!g_worker_thread || !g_mutex)
    return 0;
  g_ao_enabled = lua_toboolean(L, 1);
  g_light_mode = luaL_checkinteger(L, 2);
  g_debug_enabled = lua_toboolean(L, 3);
  dmMutex::Lock(g_mutex);
  g_worker_has_work = true;
  g_worker_result_ready = false;
  dmMutex::Unlock(g_mutex);
  return 0;
}

static int Lua_PollMeshBuffer(lua_State *L) {
  if (!g_worker_thread || !g_vertex_positions) {
    lua_pushnil(L);
    return 1;
  }
  dmMutex::Lock(g_mutex);
  bool is_ready = g_worker_result_ready;
  uint32_t v_count = g_result_vertex_count;
  uint32_t f_count = g_result_face_count;
  uint32_t q_count = g_result_quad_count;

  uint32_t tr_v_count = g_trans_result_vertex_count;
  uint32_t tr_f_count = g_trans_result_face_count;
  uint32_t tr_q_count = g_trans_result_quad_count;

  double build_time = g_result_build_time;
  dmMutex::Unlock(g_mutex);

  if (!is_ready) {
    lua_pushnil(L);
    return 1;
  }

  static dmBuffer::StreamDeclaration streams_decl[] = {
      {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
      {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 4},
      {dmHashString64("texcoord1"), dmBuffer::VALUE_TYPE_FLOAT32, 4},
      {dmHashString64("texcoord2"), dmBuffer::VALUE_TYPE_FLOAT32, 2}};

  // Opaque Buffer
  if (v_count > 0) {
    dmBuffer::HBuffer buffer_handle = 0;
    dmBuffer::Create(v_count, streams_decl, 4, &buffer_handle);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("position"),
                                g_vertex_positions, v_count, 3);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord0"),
                                g_vertex_uvs_base, v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord1"),
                                g_vertex_uvs_local, v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord2"),
                                g_vertex_face_ids, v_count, 2);
    dmBuffer::UpdateContentVersion(buffer_handle);
    dmScript::PushBuffer(
        L, dmScript::LuaHBuffer(buffer_handle, dmScript::OWNER_LUA));
  } else {
    lua_pushnil(L);
  }
  lua_pushinteger(L, v_count);
  lua_pushinteger(L, f_count);
  lua_pushinteger(L, q_count);

  // Transparent Buffer
  if (tr_v_count > 0) {
    dmBuffer::HBuffer buffer_handle = 0;
    dmBuffer::Create(tr_v_count, streams_decl, 4, &buffer_handle);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("position"),
                                g_trans_vertex_positions, tr_v_count, 3);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord0"),
                                g_trans_vertex_uvs_base, tr_v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord1"),
                                g_trans_vertex_uvs_local, tr_v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord2"),
                                g_trans_vertex_face_ids, tr_v_count, 2);
    dmBuffer::UpdateContentVersion(buffer_handle);
    dmScript::PushBuffer(
        L, dmScript::LuaHBuffer(buffer_handle, dmScript::OWNER_LUA));
  } else {
    lua_pushnil(L);
  }
  lua_pushinteger(L, tr_v_count);
  lua_pushinteger(L, tr_f_count);
  lua_pushinteger(L, tr_q_count);

  lua_pushnumber(L, build_time);

  dmMutex::Lock(g_mutex);
  g_worker_result_ready = false;
  dmMutex::Unlock(g_mutex);
  return 9;
}

static const luaL_reg Module_methods[] = {
    {"init", Lua_InitializeVoxelEngine},
    {"shutdown", Lua_ShutdownVoxelEngine},
    {"register_block", Lua_RegisterBlockType},
    {"get_block_info", Lua_GetBlockInfo},
    {"set_block", Lua_SetBlockInWorld},
    {"get_block", Lua_GetBlockFromWorld},
    {"get_ambient_light", Lua_GetAmbientLight},
    {"get_max_vertices", Lua_GetMaxVertices},
    {"request_mesh_update", Lua_RequestAsyncMeshUpdate},
    {"poll_mesh_buffer", Lua_PollMeshBuffer},
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

static dmExtension::Result AppInit(dmExtension::AppParams *params) {
  return dmExtension::RESULT_OK;
}
static dmExtension::Result Init(dmExtension::Params *params) {
  luaL_register(params->m_L, LIB_NAME, Module_methods);
  lua_pop(params->m_L, 1);

  InitParticles();

  return dmExtension::RESULT_OK;
}
static uint64_t g_last_time = 0;
static dmExtension::Result UpdateExtension(dmExtension::Params *params) {
  if (g_grid_size == 0)
    return dmExtension::RESULT_OK;
  uint64_t now = dmTime::GetTime();
  if (g_last_time == 0 ||
      (now - g_last_time) > 1000000) { // Reset if first frame or pause > 1s
    g_last_time = now;
    return dmExtension::RESULT_OK;
  }
  float dt = (float)(now - g_last_time) / 1000000.0f;
  g_last_time = now;

  UpdateAllNPCs(dt);

  // Process deferred particles if lighting is ready
  dmMutex::Lock(g_mutex);
  bool ready = g_worker_result_ready;
  dmMutex::Unlock(g_mutex);

  ProcessPendingSpawns(ready);

  // Passing a dummy camera position for LOD optimization for now.
  UpdateParticles(dt);

  return dmExtension::RESULT_OK;
}
static dmExtension::Result Finalize(dmExtension::Params *params) {
  return dmExtension::RESULT_OK;
}
static dmExtension::Result AppFinalFunc(dmExtension::AppParams *params) {
  if (g_worker_running) {
    g_worker_running = false;
    if (g_worker_thread) {
      dmThread::Join(g_worker_thread);
      g_worker_thread = 0;
    }
    if (g_mutex) {
      dmMutex::Delete(g_mutex);
      g_mutex = 0;
    }
    free(g_vertex_positions);
    free(g_vertex_uvs_base);
    free(g_vertex_uvs_local);
    free(g_vertex_face_ids);
  }
  return dmExtension::RESULT_OK;
}
DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInit, AppFinalFunc, Init,
                     UpdateExtension, 0, Finalize)
