#include "ve_particle.h"
#include "ve_world.h"
#include <math.h>
#include <stdlib.h>

std::vector<ParticleEmitter> g_emitters;
std::vector<PendingSpawn> g_pending_spawns;
Particle g_particles[MAX_PARTICLES];
std::unordered_map<dmhash_t, int> g_particle_id_map; // Defined the map

static float RandomFloat(float min, float max) {
  float r = (float)rand() / (float)RAND_MAX;
  return min + r * (max - min);
}

void InitParticles() {
  // g_particles is now a static array, no allocation needed.
  // Initialize its members.
  g_particle_id_map.clear(); // Clear map on init
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
}

static void SpawnParticle(ParticleEmitter &emitter) {
  // g_particles is now a static array, always available.
  // No need for `if (!g_particles)` check.

  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (!g_particles[i].active && g_particles[i].instance != 0) {
      g_particles[i].active = true;

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

      // Send Enable message

      // Send Enable message
      dmMessage::URL receiver;
      dmMessage::ResetURL(&receiver);
      receiver.m_Socket = g_particles[i].socket;
      receiver.m_Path = dmGameObject::GetIdentifier(g_particles[i].instance);
      dmMessage::Post(0, &receiver, dmHashString64("enable"), 0, 0, 0, 0, 0, 0);

      // Set visuals in Defold
      // Use scale.x, scale.y for size and scale.z for block_id (passed to Lua
      // script)
      dmVMath::Vector3 scale(g_particles[i].scale[0], g_particles[i].scale[1],
                             (float)g_particles[i].block_id);
      dmGameObject::SetScale(g_particles[i].instance, scale);
      dmGameObject::SetPosition(g_particles[i].instance,
                                dmVMath::Point3(g_particles[i].pos.getX(),
                                                g_particles[i].pos.getY(),
                                                g_particles[i].pos.getZ()));

      break;
    }
  }
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
  for (int i = 0; i < MAX_PARTICLES; ++i) {
    if (!g_particles[i].active)
      continue;

    // Lifetime
    g_particles[i].lifetime -= dt;
    if (g_particles[i].lifetime <= 0) {
      g_particles[i].active = false;
      dmMessage::URL receiver;
      dmMessage::ResetURL(&receiver);
      receiver.m_Socket = g_particles[i].socket;
      receiver.m_Path = dmGameObject::GetIdentifier(g_particles[i].instance);
      dmMessage::Post(0, &receiver, dmHashString64("disable"), 0, 0, 0, 0, 0,
                      0);
      dmGameObject::SetPosition(g_particles[i].instance,
                                dmVMath::Point3(0, -1000, 0));
      continue;
    }

    // Distance Culling (LOD)
    float dist_sq = dmVMath::LengthSqr(cam_pos - g_particles[i].pos);
    if (dist_sq > LOW_PRIORITY_DIST_SQ) {
      g_particles[i].active = false;
      dmMessage::URL receiver;
      dmMessage::ResetURL(&receiver);
      receiver.m_Socket = g_particles[i].socket;
      receiver.m_Path = dmGameObject::GetIdentifier(g_particles[i].instance);
      dmMessage::Post(0, &receiver, dmHashString64("disable"), 0, 0, 0, 0, 0,
                      0);
      dmGameObject::SetPosition(g_particles[i].instance,
                                dmVMath::Point3(0, -1000, 0));
      continue;
    }

    // Physics
    g_particles[i].vel.setY(g_particles[i].vel.getY() +
                            g_particles[i].gravity * dt);
    
    dmVMath::Vector3 pos = g_particles[i].pos;
    dmVMath::Vector3 vel = g_particles[i].vel;

    // Collision (High precision only when close)
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

      // Hide immediately
      dmMessage::URL receiver;
      dmMessage::ResetURL(&receiver);
      receiver.m_Socket = socket;
      receiver.m_Path = dmGameObject::GetIdentifier(instance);
      dmMessage::Post(0, &receiver, dmHashString64("disable"), 0, 0, 0, 0, 0,
                      0);
      dmGameObject::SetPosition(instance, dmVMath::Point3(0, -1000, 0));

      return 0;
    }
  }
  return 0;
}

void SpawnBlockParticles(float x, float y, float z, int block_id,
                         bool must_wait) {
  g_pending_spawns.push_back({x, y, z, block_id, 0, must_wait});
}

void SpawnBlockParticles(float x, float y, float z, int block_id) {
  SpawnBlockParticles(x, y, z, block_id, false);
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
      emitter.emit_count = 15;
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
      int lx = (int)floorf(spawn.x + 0.5f);
      int ly = (int)floorf(spawn.y + 0.5f);
      int lz = (int)floorf(spawn.z + 0.5f);

      float r = 1.0f, g = 1.0f, b = 1.0f;
      if (lx >= 0 && lx < g_grid_size && ly >= 0 && ly < g_grid_size &&
          lz >= 0 && lz < g_grid_size) {
        int idx = calculate_block_index(lx, ly, lz, g_grid_size);
        float sun_f = (float)g_sun_light[idx] / 15.0f;
        float source_f = ((float)g_source_light[idx] / 15.0f) * 1.5f;
        r = fmaxf(0.02f, sun_f + source_f * 1.0f);
        g = fmaxf(0.02f, sun_f + source_f * 0.9f);
        b = fmaxf(0.02f, sun_f + source_f * 0.6f);
      }
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

  SpawnBlockParticles(x, y, z, block_id, must_wait);
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
