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
