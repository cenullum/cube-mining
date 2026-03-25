#pragma once
#include <dmsdk/sdk.h>
#include <vector>
#include <unordered_map>

extern std::unordered_map<dmhash_t, int> g_particle_id_map;

const int MAX_PARTICLES = 10000;

struct ParticleEmitterInfo {
    float lifetime;
    dmVMath::Vector3 initial_velocity;
    float velocity_randomness; 
    float gravity;
    float atlas_bounds[4]; 
    float start_scale[2];
    int block_id;
    float light_tint[4];
};

struct PendingSpawn {
    float x, y, z;
    int block_id;
    int count;
    int frames_waiting;
    bool must_wait;
};

struct ParticleEmitter {
    uint32_t id;
    dmVMath::Vector3 pos;
    bool active;
    int emit_count;  // quantity of particles to spawn per event
    bool loop;       // whether it repeats or burst once
    float timer;
    float spawn_rate; // frequency of events
    
    ParticleEmitterInfo config;
};

struct Particle {
    bool active;
    dmGameObject::HInstance instance;
    dmhash_t id;
    dmhash_t socket;
    
    dmVMath::Vector3 pos;
    dmVMath::Vector3 vel;
    float lifetime;
    float initial_lifetime;
    float gravity;
    
    float atlas_bounds[4];
    float scale[2];
    int block_id;
    float light_tint[4];
};

struct ParticleEvent {
    int type; // 1 = spawn, 0 = despawn
    dmhash_t id;
    float u, v, w, h;
    float r, g, b, a;
};

extern std::vector<ParticleEmitter> g_emitters;
extern Particle g_particles[MAX_PARTICLES]; 
extern std::vector<int> g_active_particles;
extern std::vector<int> g_inactive_particles;
extern std::vector<ParticleEvent> g_particle_events;


void InitParticles();
void ShutdownParticles();
void UpdateParticles(float dt);
void ProcessPendingSpawns(bool ready);
void SpawnBlockParticles(float x, float y, float z, int block_id, int count = 15, bool must_wait = false);
int Lua_SpawnBlockParticles(lua_State* L);
int Lua_RegisterParticle(lua_State* L);
int Lua_GetParticleInitData(lua_State* L);
int Lua_PullParticleEvents(lua_State* L);
int Lua_SetParticleManager(lua_State* L);
