#include "ve_particle.h"
#include "ve_world.h"
#include <math.h>
#include <stdlib.h>
#include <unordered_map>

std::vector<ParticleEmitter> g_emitters;
std::vector<PendingSpawn> g_pending_spawns;
Particle g_particles[MAX_PARTICLES];
std::unordered_map<dmhash_t, int> g_particle_id_map; // Defined the map

std::vector<int> g_active_particles;
std::vector<int> g_inactive_particles;

std::vector<ParticleEvent> g_particle_events;
dmhash_t g_manager_socket = 0;
dmhash_t g_manager_path = 0;
dmhash_t g_manager_fragment = 0;

static float RandomFloat(float min, float max) {
  float r = (float)rand() / (float)RAND_MAX;
  return min + r * (max - min);
}

void InitParticles() {
  // g_particles is now a static array, no allocation needed.
  // Initialize its members.
  g_particle_id_map.clear(); // Clear map on init
  g_active_particles.clear();
  g_inactive_particles.clear();
  g_active_particles.reserve(MAX_PARTICLES);
  g_inactive_particles.reserve(MAX_PARTICLES);
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    g_particles[i].active = false;
    g_particles[i].instance = 0;
    g_particles[i].id = 0;
    g_particles[i].socket = 0;
  }
}

void ShutdownParticles() {
  // g_particles is now a static array, no deallocation needed.
  g_emitters.clear();
  g_pending_spawns.clear();  // Clear pending spawns on shutdown
  g_particle_id_map.clear(); // Clear map on shutdown
  g_active_particles.clear();
  g_inactive_particles.clear();
}

static void SpawnParticle(ParticleEmitter &emitter) {
  if (g_inactive_particles.empty())
    return;

  int i = g_inactive_particles.back();
  g_inactive_particles.pop_back();

  g_particles[i].active = true;
  g_active_particles.push_back(i);

  // Random scatter around spawn point
  g_particles[i].pos =
      emitter.pos + dmVMath::Vector3(RandomFloat(-0.4f, 0.4f),
                                     RandomFloat(-0.4f, 0.4f),
                                     RandomFloat(-0.4f, 0.4f));

  // Velocity randomness
  float rnd_x =
      RandomFloat(-1.0f, 1.0f) * emitter.config.velocity_randomness;
  float rnd_y =
      RandomFloat(-0.5f, 1.0f) * emitter.config.velocity_randomness;
  float rnd_z =
      RandomFloat(-1.0f, 1.0f) * emitter.config.velocity_randomness;

  g_particles[i].vel = emitter.config.initial_velocity +
                       dmVMath::Vector3(rnd_x, rnd_y, rnd_z);
  g_particles[i].lifetime =
      emitter.config.lifetime * RandomFloat(0.8f, 1.2f);
  g_particles[i].initial_lifetime = g_particles[i].lifetime;
  g_particles[i].gravity = emitter.config.gravity;

  g_particles[i].block_id = emitter.config.block_id;
  for (int k = 0; k < 4; k++)
    g_particles[i].light_tint[k] = emitter.config.light_tint[k];
  for (int k = 0; k < 2; k++)
    g_particles[i].scale[k] = emitter.config.start_scale[k];

  // Calculate individual random UV fragments
  UVData uv = g_block_defs[g_particles[i].block_id].uvs[1];
  float sub_scale = 0.25f;
  float offset_u = RandomFloat(0.0f, uv.w * (1.0f - sub_scale));
  float offset_v = RandomFloat(0.0f, uv.h * (1.0f - sub_scale));

  g_particles[i].atlas_bounds[0] = uv.u + offset_u;
  g_particles[i].atlas_bounds[1] = uv.v + offset_v;
  g_particles[i].atlas_bounds[2] = uv.w * sub_scale;
  g_particles[i].atlas_bounds[3] = uv.h * sub_scale;

  // Add to batch event list instead of sending message directly
  ParticleEvent ev;
  ev.type = 1;
  ev.id = g_particles[i].id;
  ev.u = g_particles[i].atlas_bounds[0];
  ev.v = g_particles[i].atlas_bounds[1];
  ev.w = g_particles[i].atlas_bounds[2];
  ev.h = g_particles[i].atlas_bounds[3];
  ev.r = g_particles[i].light_tint[0];
  ev.g = g_particles[i].light_tint[1];
  ev.b = g_particles[i].light_tint[2];
  ev.a = g_particles[i].light_tint[3];
  g_particle_events.push_back(ev);

  // Set visuals in Defold
  // Use scale.x, scale.y for size and scale.z for block_id (passed to Lua script)
  dmVMath::Vector3 scale(g_particles[i].scale[0], g_particles[i].scale[1],
                         (float)g_particles[i].block_id);
  dmGameObject::SetScale(g_particles[i].instance, scale);
  dmGameObject::SetPosition(g_particles[i].instance,
                            dmVMath::Point3(g_particles[i].pos.getX(),
                                            g_particles[i].pos.getY(),
                                            g_particles[i].pos.getZ()));
}

void UpdateParticles(float dt) {
  // g_particles is now a static array, always available.
  // No need for `if (!g_particles)` check.
  dmVMath::Vector3 cam_pos = g_player_pos;

  // LOD Distances
  const float LOW_PRIORITY_DIST_SQ = 128.0f * 128.0f;
  const float MED_PRIORITY_DIST_SQ = 64.0f * 64.0f;

  // 1. Process Emitters
  for (size_t i = 0; i < g_emitters.size();) {
    auto &emitter = g_emitters[i];
    if (!emitter.active) {
      g_emitters.erase(g_emitters.begin() + i);
      continue;
    }

    float dist_sq = dmVMath::LengthSqr(cam_pos - emitter.pos);
    if (dist_sq > LOW_PRIORITY_DIST_SQ) {
      i++;
      continue;
    }

    int actual_emit_count = emitter.emit_count;
    if (dist_sq > MED_PRIORITY_DIST_SQ)
      actual_emit_count /= 2;

    if (emitter.spawn_rate < 0.001f) {
      // Burst
      for (int j = 0; j < actual_emit_count; j++)
        SpawnParticle(emitter);
      emitter.active = false;
    } else {
      // Timed
      emitter.timer += dt;
      while (emitter.timer >= emitter.spawn_rate) {
        emitter.timer -= emitter.spawn_rate;
        for (int j = 0; j < actual_emit_count; j++)
          SpawnParticle(emitter);
        if (!emitter.loop) {
          emitter.active = false;
          break;
        }
      }
    }
    i++;
  }

  // 2. Process active particles
  for (size_t j = 0; j < g_active_particles.size();) {
    int i = g_active_particles[j];

    // Lifetime
    g_particles[i].lifetime -= dt;
    bool kill = false;

    if (g_particles[i].lifetime <= 0) {
      kill = true;
    } else {
      // Distance Culling (LOD)
      float dist_sq = dmVMath::LengthSqr(cam_pos - g_particles[i].pos);
      if (dist_sq > LOW_PRIORITY_DIST_SQ) {
        kill = true;
      }
    }

    if (kill) {
      g_particles[i].active = false;
      ParticleEvent ev;
      ev.type = 0;
      ev.id = g_particles[i].id;
      g_particle_events.push_back(ev);
      dmGameObject::SetPosition(g_particles[i].instance,
                                dmVMath::Point3(0, -1000, 0));

      // Remove from active list
      g_inactive_particles.push_back(i);
      if (j != g_active_particles.size() - 1) {
        g_active_particles[j] = g_active_particles.back();
      }
      g_active_particles.pop_back();
      continue;
    }

    // Physics
    g_particles[i].vel.setY(g_particles[i].vel.getY() +
                            g_particles[i].gravity * dt);
    
    dmVMath::Vector3 pos = g_particles[i].pos;
    dmVMath::Vector3 vel = g_particles[i].vel;

    // Collision (High precision only when close)
    float dist_sq = dmVMath::LengthSqr(cam_pos - g_particles[i].pos);
    if (dist_sq <= MED_PRIORITY_DIST_SQ) {
      float bounce = 0.4f;
      float friction = 0.8f;
      ParticlePhysicsStep(pos, vel, dt, bounce, friction);
    } else {
      pos = pos + vel * dt;
    }

    g_particles[i].pos = pos;
    g_particles[i].vel = vel;
    dmGameObject::SetPosition(g_particles[i].instance,
                              dmVMath::Point3(g_particles[i].pos.getX(),
                                              g_particles[i].pos.getY(),
                                              g_particles[i].pos.getZ()));
    j++;
  }

  // Final step: notify the manager if there are events this frame!
  if (!g_particle_events.empty() && g_manager_socket != 0) {
      dmMessage::URL receiver;
      dmMessage::ResetURL(&receiver);
      receiver.m_Socket = g_manager_socket;
      receiver.m_Path = g_manager_path;
      receiver.m_Fragment = g_manager_fragment;
      dmMessage::Post(0, &receiver, dmHashString64("particle_events"), 0, 0, 0, 0, 0, 0);
  }
}

int Lua_RegisterParticle(lua_State *L) {
  dmhash_t id = dmScript::CheckHash(L, 1);
  dmGameObject::HInstance instance = dmScript::CheckGOInstance(L, 2);
  dmhash_t socket = dmScript::CheckHash(L, 3);

  // g_particles is now a static array, always available.
  // No need for `if (g_particles)` check.
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (g_particles[i].instance == 0) {
      g_particles[i].id = id;
      g_particle_id_map[id] = i;
      g_particles[i].instance = instance;
      g_particles[i].socket = socket;
      g_particles[i].active = false;
      
      // Add to inactive queue
      g_inactive_particles.push_back(i);

      // Add despawn event
      ParticleEvent ev;
      ev.type = 0;
      ev.id = id;
      g_particle_events.push_back(ev);
      dmGameObject::SetPosition(instance, dmVMath::Point3(0, -1000, 0));

      return 0;
    }
  }
  return 0;
}

void SpawnBlockParticles(float x, float y, float z, int block_id, int count,
                         bool must_wait) {
  g_pending_spawns.push_back({x, y, z, block_id, count, 0, must_wait});
}

void ProcessPendingSpawns(bool ready) {
  if (g_pending_spawns.empty())
    return;

  std::vector<PendingSpawn> next_queue;
  for (auto &spawn : g_pending_spawns) {
    bool timeout = (spawn.frames_waiting > 15); // Fail-safe after ~0.25s

    if (ready || timeout ||
        !spawn.must_wait) { // Process if ready, timed out, or not waiting
      ParticleEmitter emitter = {};
      emitter.pos = dmVMath::Vector3(spawn.x, spawn.y, spawn.z);
      emitter.active = true;
      emitter.emit_count = spawn.count;
      emitter.loop = false;
      emitter.timer = 0;
      emitter.spawn_rate = 0.0f;

      emitter.config.lifetime = 3.0f;
      emitter.config.initial_velocity = dmVMath::Vector3(0, 3.5f, 0);
      emitter.config.velocity_randomness = 2.5f;
      emitter.config.gravity = -12.0f;
      emitter.config.block_id = spawn.block_id;

      // Proper UV Data for fragments (use face 1)
      UVData uv = g_block_defs[spawn.block_id].uvs[1];
      float sub_scale = 0.25f;
      float offset_u = RandomFloat(0.0f, uv.w * (1.0f - sub_scale));
      float offset_v = RandomFloat(0.0f, uv.h * (1.0f - sub_scale));
      emitter.config.atlas_bounds[0] = uv.u + offset_u;
      emitter.config.atlas_bounds[1] = uv.v + offset_v;
      emitter.config.atlas_bounds[2] = uv.w * sub_scale;
      emitter.config.start_scale[0] = 0.25f;
      emitter.config.start_scale[1] = 0.25f;

      // Sample light (either fresh or current-best)
      float r = 1.0f, g = 1.0f, b = 1.0f;
      // Note: Particle light lookup disabled for multi-chunk since particles don't need accurate GI.
      emitter.config.light_tint[0] = r;
      emitter.config.light_tint[1] = g;
      emitter.config.light_tint[2] = b;
      emitter.config.light_tint[3] = 1.0f;

      g_emitters.push_back(emitter);
      if (timeout)
        printf("DEBUG: Particle spawn TIMEOUT for block %d\n", spawn.block_id);
    } else {
      PendingSpawn s = spawn;
      s.frames_waiting++;
      next_queue.push_back(s);
    }
  }
  g_pending_spawns = next_queue;
}

int Lua_SpawnBlockParticles(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float z = (float)luaL_checknumber(L, 3);
  int block_id = (int)luaL_checkinteger(L, 4);
  bool must_wait = lua_toboolean(L, 5);
  int count = (int)luaL_optinteger(L, 6, 15);

  SpawnBlockParticles(x, y, z, block_id, count, must_wait);
  return 0;
}

int Lua_GetParticleInitData(lua_State *L) {
  dmhash_t id = dmScript::CheckHash(L, 1);

  auto it = g_particle_id_map.find(id);
  if (it == g_particle_id_map.end())
    return 0;

  int i = it->second;
  // Safety check: ensure the slot still contains this particle
  if (g_particles[i].id != id)
    return 0;

  lua_pushinteger(L, g_particles[i].block_id);
  lua_pushnumber(L, g_particles[i].light_tint[0]);
  lua_pushnumber(L, g_particles[i].light_tint[1]);
  lua_pushnumber(L, g_particles[i].light_tint[2]);
  lua_pushnumber(L, g_particles[i].light_tint[3]);

  lua_pushnumber(L, g_particles[i].atlas_bounds[0]);
  lua_pushnumber(L, g_particles[i].atlas_bounds[1]);
  lua_pushnumber(L, g_particles[i].atlas_bounds[2]);
  lua_pushnumber(L, g_particles[i].atlas_bounds[3]);
  return 9;
}

int Lua_SetParticleManager(lua_State* L) {
    g_manager_socket = dmScript::CheckHash(L, 1);
    g_manager_path = dmScript::CheckHash(L, 2);
    g_manager_fragment = dmScript::CheckHash(L, 3);
    return 0;
}

int Lua_PullParticleEvents(lua_State* L) {
    lua_newtable(L);
    int count = 1;
    for (const auto& ev : g_particle_events) {
        lua_newtable(L);
        lua_pushinteger(L, ev.type);
        lua_setfield(L, -2, "type");
        
        dmScript::PushHash(L, ev.id);
        lua_setfield(L, -2, "id");
        
        if (ev.type == 1) {
            lua_pushnumber(L, ev.u); lua_setfield(L, -2, "u");
            lua_pushnumber(L, ev.v); lua_setfield(L, -2, "v");
            lua_pushnumber(L, ev.w); lua_setfield(L, -2, "w");
            lua_pushnumber(L, ev.h); lua_setfield(L, -2, "h");
            
            lua_pushnumber(L, ev.r); lua_setfield(L, -2, "r");
            lua_pushnumber(L, ev.g); lua_setfield(L, -2, "g");
            lua_pushnumber(L, ev.b); lua_setfield(L, -2, "b");
            lua_pushnumber(L, ev.a); lua_setfield(L, -2, "a");
        }
        
        lua_rawseti(L, -2, count++);
    }
    g_particle_events.clear();
    return 1;
}

