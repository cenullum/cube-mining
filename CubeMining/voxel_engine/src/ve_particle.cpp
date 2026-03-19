#include "ve_particle.h"
#include "ve_world.h"
#include <math.h>
#include <new>
#include <stdlib.h>

std::vector<ParticleEmitter> g_emitters;
Particle *g_particles = nullptr;

static float RandomFloat(float min, float max) {
  float r = (float)rand() / (float)RAND_MAX;
  return min + r * (max - min);
}

void InitParticles() {
  g_particles = new (std::nothrow) Particle[MAX_PARTICLES];
  if (g_particles) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
      g_particles[i].active = false;
      g_particles[i].instance = 0;
      g_particles[i].id = 0;
      g_particles[i].socket = 0;
    }
  }
}

void ShutdownParticles() {
  if (g_particles) {
    delete[] g_particles;
    g_particles = nullptr;
  }
  g_emitters.clear();
}

static void SpawnParticle(ParticleEmitter &emitter) {
  if (!g_particles)
    return;

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

      // Copy visual data
      for (int k = 0; k < 4; k++)
        g_particles[i].atlas_bounds[k] = emitter.config.atlas_bounds[k];
      for (int k = 0; k < 2; k++)
        g_particles[i].scale[k] = emitter.config.start_scale[k];
      g_particles[i].block_id = emitter.config.block_id;

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
  if (!g_particles)
    return;
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
    dmVMath::Vector3 next_pos = g_particles[i].pos + g_particles[i].vel * dt;

    // Collision (High precision only when close)
    if (dist_sq <= MED_PRIORITY_DIST_SQ) {
      if (CheckPointCollision(next_pos.getX(), next_pos.getY(),
                              next_pos.getZ())) {
        g_particles[i].vel = g_particles[i].vel * -0.4f; // bounce
        next_pos = g_particles[i].pos;
      }
    }

    g_particles[i].pos = next_pos;
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

  if (g_particles) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
      if (g_particles[i].instance == 0) {
        g_particles[i].id = id;
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
  }
  return 0;
}

void SpawnBlockParticles(float x, float y, float z, int block_id) {
  if (block_id <= 0 || block_id > 255)
    return;

  ParticleEmitter emitter = {};
  emitter.pos = dmVMath::Vector3(x, y, z);
  emitter.active = true;
  emitter.emit_count = 15;
  emitter.loop = false;
  emitter.timer = 0;
  emitter.spawn_rate = 0.0f;

  emitter.config.lifetime = 5.0f;
  emitter.config.initial_velocity = dmVMath::Vector3(0, 3.5f, 0);
  emitter.config.velocity_randomness = 2.5f;
  emitter.config.gravity = -12.0f;
  emitter.config.block_id = block_id;

  // Proper UV Data for fragments
  UVData uv = g_block_defs[block_id].uvs[0];

  // Pick a random 1/4th sub-region of the block texture for this emitter's
  // particles
  float sub_scale = 0.25f;
  float offset_u = RandomFloat(0.0f, uv.w * (1.0f - sub_scale));
  float offset_v = RandomFloat(0.0f, uv.h * (1.0f - sub_scale));

  emitter.config.atlas_bounds[0] = uv.u + offset_u;
  emitter.config.atlas_bounds[1] = uv.v + offset_v;
  emitter.config.atlas_bounds[2] = uv.w * sub_scale;
  emitter.config.atlas_bounds[3] = uv.h * sub_scale;

  emitter.config.start_scale[0] = 0.25f;
  emitter.config.start_scale[1] = 0.25f;

  g_emitters.push_back(emitter);
}

int Lua_SpawnBlockParticles(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float z = (float)luaL_checknumber(L, 3);
  int block_id = (int)luaL_checkinteger(L, 4);

  SpawnBlockParticles(x, y, z, block_id);
  return 0;
}
