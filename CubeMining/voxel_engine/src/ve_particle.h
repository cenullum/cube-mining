#pragma once
#include <dmsdk/sdk.h>
#include <vector>

const int MAX_PARTICLES = 10000;

struct ParticleEmitterInfo {
    float lifetime;
    dmVMath::Vector3 initial_velocity;
    float velocity_randomness; 
    float gravity;
    float atlas_bounds[4]; 
    float start_scale[2];
    int block_id;
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
};

extern std::vector<ParticleEmitter> g_emitters;
extern Particle* g_particles; // dynamic allocation

void InitParticles();
void ShutdownParticles();
void UpdateParticles(float dt);
void SpawnBlockParticles(float x, float y, float z, int block_id);
int Lua_SpawnBlockParticles(lua_State* L);
int Lua_RegisterParticle(lua_State* L);
