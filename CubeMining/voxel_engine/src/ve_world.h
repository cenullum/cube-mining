#pragma once

#include <dmsdk/sdk.h>
#include <vector>
#include <map>
#include <cstdint>

// Constants
extern const int MAX_GRID_SIZE;
extern const int MAX_BLOCKS;

// Types
struct UVData { float u, v, w, h; };

struct BlockDef {
    bool registered, transparent, solid;
    uint8_t light_level;
    dmhash_t name_hash;
    dmhash_t hit_sound_hash;
    dmhash_t break_sound_hash;
    UVData uvs[7];
};

struct BlockType {
    uint32_t id;
    bool is_solid;
    bool is_transparent;
    bool is_liquid;
    uint8_t light_level;
    UVData uvs[7];
};

struct NPCInfo {
    dmhash_t id;
    dmGameObject::HInstance instance;
    dmVMath::Vector3 pos;
    dmVMath::Vector3 vel;
    dmVMath::Vector3 move_dir;
    dmVMath::Vector3 size;
    bool is_dead;
    int state;
    float timer;
    float state_duration;
    float speed;
    float gravity;
    float jump_force;
    float rotation_offset_y;
    float health;
    dmhash_t socket;
};

struct DebugQuad {
    float x1, y1, z1;
    float x2, y2, z2;
    float x3, y3, z3;
    float x4, y4, z4;
    int dir;
};

// Global State
extern int g_grid_size;
extern uint8_t g_blocks[];
extern uint8_t g_sun_light[];
extern uint8_t g_source_light[];
extern BlockDef g_block_defs[256];
extern int g_seed;
extern bool g_ao_enabled;
extern int g_light_mode;
extern std::vector<NPCInfo> g_npcs;
extern std::vector<DebugQuad> g_debug_quads;
extern bool g_debug_enabled;

// Prototypes - World
int calculate_block_index(int x, int y, int z, int side_length);
uint8_t safe_get_block(int x, int y, int z, int s);
uint8_t GetBlock(int x, int y, int z);
void SetBlock(int x, int y, int z, uint8_t id);
bool IsSolid(int x, int y, int z);

// Prototypes - Terrain
void initialize_world_terrain_data();
int calculate_ground_height(int x, int z);
float calculate_perlin_noise_3d(float x, float y, float z);

// Prototypes - Lighting
void perform_lighting_pass(uint8_t* blocks, uint8_t* sun_light_data, uint8_t* source_light_data, int side_length);

// Prototypes - Mesh
void alloc_mesh_buffers(int grid_size);
void execute_mesh_generation_pipeline(const uint8_t* world_blocks, const uint8_t* sun_light, const uint8_t* source_light, int side_length, bool ao_enabled, int light_mode);
void copy_array_to_buffer_stream(dmBuffer::HBuffer buffer, dmhash_t stream_name, const float* source_data, uint32_t vertex_count, uint32_t components_per_vertex);

extern float* g_vertex_positions;
extern float* g_vertex_uvs_base;
extern float* g_vertex_uvs_local;
extern float* g_vertex_face_ids;
extern uint32_t g_result_quad_count;
extern uint32_t g_result_face_count;
extern uint32_t g_result_vertex_count;
extern double g_result_build_time;

// Prototypes - Physics
bool CheckCollision(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z);
void MoveAndSlide(dmVMath::Vector3& pos, dmVMath::Vector3& vel, const dmVMath::Vector3& size, float dt, bool& is_grounded);
bool RayAABBIntersection(const dmVMath::Vector3& ray_origin, const dmVMath::Vector3& ray_dir, const dmVMath::Vector3& box_min, const dmVMath::Vector3& box_max, float& t_out);

// Prototypes - NPC
void UpdateAllNPCs(float dt);
