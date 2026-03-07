// terrain_engine.cpp — All terrain operations in C++ with background thread
#define EXTENSION_NAME terrain_engine
#define LIB_NAME "terrain"

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================
// Constants
// ============================================================
static const int MAX_GRID_SIZE = 64;
static const int MAX_BLOCKS = MAX_GRID_SIZE * MAX_GRID_SIZE * MAX_GRID_SIZE;
static const float AMBIENT_OCCLUSION_LEVELS[4] = {1.0f, 0.8f, 0.6f, 0.4f};
static const float NORMALIZED_LIGHT_STEP = 1.0f / 15.0f;

// ============================================================
// Types
// ============================================================
struct UVData { float u, v, w, h; };
struct BlockDef {
    bool registered, transparent, solid;
    uint8_t light_level;
    UVData uvs[7]; // index 1-6 used
};

enum TaskType { TASK_NONE=0, TASK_MESH_UPDATE };

// ============================================================
// Perlin Noise
// ============================================================
static const int PERM[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,
    23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,
    87,174,20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,
    132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,
    226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,
    42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,
    19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,
    191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,
    115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    // doubled
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,
    23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,
    87,174,20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,
    132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,
    226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,
    42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,
    19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,
    191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,
    115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static float calculate_gradient(int hash_value, float x, float y, float z) {
    int h = hash_value & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14) ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

static float calculate_perlin_noise_3d(float x, float y, float z) {
    int x_floor = (int)floorf(x), y_floor = (int)floorf(y), z_floor = (int)floorf(z);
    int X = (x_floor % 256 + 256) % 256, Y = (y_floor % 256 + 256) % 256, Z = (z_floor % 256 + 256) % 256;
    x -= floorf(x); y -= floorf(y); z -= floorf(z);
    float u_fade = x*x*x*(x*(x*6-15)+10), v_fade = y*y*y*(y*(y*6-15)+10), w_fade = z*z*z*(z*(z*6-15)+10);
    
    int hash_a  = PERM[X] + Y,     hash_b  = PERM[X+1] + Y;
    int hash_aa = PERM[hash_a] + Z, hash_ab = PERM[hash_a+1] + Z;
    int hash_ba = PERM[hash_b] + Z, hash_bb = PERM[hash_b+1] + Z;
    
    float x_minus1 = x - 1, y_minus1 = y - 1, z_minus1 = z - 1;
    
    float grad_1 = calculate_gradient(PERM[hash_aa],   x,        y,        z);
    float grad_2 = calculate_gradient(PERM[hash_ba],   x_minus1, y,        z);
    float grad_3 = calculate_gradient(PERM[hash_ab],   x,        y_minus1, z);
    float grad_4 = calculate_gradient(PERM[hash_bb],   x_minus1, y_minus1, z);
    float grad_5 = calculate_gradient(PERM[hash_aa+1], x,        y,        z_minus1);
    float grad_6 = calculate_gradient(PERM[hash_ba+1], x_minus1, y,        z_minus1);
    float grad_7 = calculate_gradient(PERM[hash_ab+1], x,        y_minus1, z_minus1);
    float grad_8 = calculate_gradient(PERM[hash_bb+1], x_minus1, y_minus1, z_minus1);
    
    float lerp_x1 = grad_1 + u_fade * (grad_2 - grad_1);
    float lerp_x2 = grad_3 + u_fade * (grad_4 - grad_3);
    float lerp_x3 = grad_5 + u_fade * (grad_6 - grad_5);
    float lerp_x4 = grad_7 + u_fade * (grad_8 - grad_7);
    
    float lerp_y1 = lerp_x1 + v_fade * (lerp_x2 - lerp_x1);
    float lerp_y2 = lerp_x3 + v_fade * (lerp_x4 - lerp_x3);
    
    return lerp_y1 + w_fade * (lerp_y2 - lerp_y1);
}

// ============================================================
// Global State
// ============================================================
static int g_grid_size = 0;
static uint8_t g_blocks[MAX_BLOCKS];
static uint8_t g_sun_light[MAX_BLOCKS];
static uint8_t g_source_light[MAX_BLOCKS];
static BlockDef g_block_defs[256];
static int g_seed = 12345;
static bool g_ao_enabled = true;
static int g_light_mode = 1;

static inline int calculate_block_index(int x, int y, int z, int side_length) { 
    return x + y * side_length + z * side_length * side_length; 
}

static inline uint8_t safe_get_block(int x, int y, int z, int s) {
    if (x<0||x>=s||y<0||y>=s||z<0||z>=s) return 0;
    return g_blocks[calculate_block_index(x,y,z,s)];
}

// ============================================================
// Terrain Generation
// ============================================================
static int calculate_ground_height(int x, int z) {
    float seed_offset_x = g_seed * 0.132f, seed_offset_z = g_seed * 0.941f, seed_offset_y = g_seed * 0.42f;
    float frequency_scale = 0.05f, persistence = 0.5f;
    int octaves = 3, target_world_height = 64;
    float noise_x = (x + seed_offset_x) * frequency_scale, noise_z = (z + seed_offset_z) * frequency_scale;
    float height_sum = 0, current_frequency = 1, current_amplitude = 1, max_amplitude_total = 0;
    for (int o = 0; o < octaves; o++) {
        height_sum += calculate_perlin_noise_3d(noise_x * current_frequency, seed_offset_y, noise_z * current_frequency) * current_amplitude;
        max_amplitude_total += current_amplitude; 
        current_amplitude *= persistence; 
        current_frequency *= 2;
    }
    return (int)floorf(((height_sum / max_amplitude_total) + 1) * 0.5f * target_world_height);
}

static uint8_t determine_block_type_at_position(int x, int y, int z, int ground_height) {
    if (y < 0) return 0;
    if (y > ground_height) return 0;
    if (y == ground_height) return (y < 10) ? 1 : 5;
    if (y > ground_height - 4) return 6;
    if (y == 0) return 2;
    float seed_offset_x = g_seed * 0.132f, seed_offset_z = g_seed * 0.941f, frequency_scale = 0.05f;
    float noise_x = (x + seed_offset_x) * frequency_scale, noise_z = (z + seed_offset_z) * frequency_scale;
    float ore_noise = calculate_perlin_noise_3d(noise_x * 2.0f, y * 0.5f, noise_z * 2.0f);
    return (ore_noise > 0.6f) ? 3 : 1;
}

static void initialize_world_terrain_data() {
    int side_length = g_grid_size;
    // Simple layer-based generation matching original Lua init
    srand(g_seed);
    for (int x = 0; x < side_length; x++) {
        for (int y = 0; y < side_length; y++) {
            for (int z = 0; z < side_length; z++) {
                uint8_t block_id = 0;
                if (y == 0) {
                    block_id = 2; // Bedrock
                } else if (y == side_length - 1) {
                    block_id = 5; // Grass
                } else if (y >= side_length - 4) {
                    block_id = 6; // Dirt
                } else {
                    block_id = ((rand() % 100) < 30) ? 3 : 1; // 30% Golden Ore, 70% Stone
                }
                g_blocks[calculate_block_index(x, y, z, side_length)] = block_id;
            }
        }
    }
    dmLogInfo("terrain_engine: Generated terrain, grid_size=%d, total_blocks=%d", side_length, side_length * side_length * side_length);
}

// ============================================================
// BFS Lighting
// ============================================================
static void perform_lighting_pass(uint8_t* blocks, uint8_t* sun_light_data, uint8_t* source_light_data, int side_length) {
    int total_blocks = side_length * side_length * side_length;
    memset(sun_light_data, 0, total_blocks);
    memset(source_light_data, 0, total_blocks);

    // Source light seeds + Sun top-down
    static int bfs_queue_buffer[MAX_BLOCKS * 4]; // x,y,z triples, reused
    int source_queue_length = 0, sun_queue_length = 0;
    
    // Use separate regions of bfs_queue_buffer
    int* source_queue = bfs_queue_buffer;
    int* sun_queue = bfs_queue_buffer + MAX_BLOCKS * 2; 

    // Seed source lights from blocks that emit light
    for (int i = 0; i < total_blocks; i++) {
        uint8_t block_id = blocks[i];
        if (g_block_defs[block_id].registered && g_block_defs[block_id].light_level > 0) {
            source_light_data[i] = g_block_defs[block_id].light_level;
            int z = i / (side_length * side_length);
            int remainder = i % (side_length * side_length);
            int y = remainder / side_length;
            int x = remainder % side_length;
            
            source_queue[source_queue_length * 3]     = x; 
            source_queue[source_queue_length * 3 + 1] = y; 
            source_queue[source_queue_length * 3 + 2] = z;
            source_queue_length++;
        }
    }

    // Sun light top-down propagation (initial pass)
    for (int x = 0; x < side_length; x++) {
        for (int z = 0; z < side_length; z++) {
            int current_sun_light = 15;
            for (int y = side_length - 1; y >= 0; y--) {
                uint8_t block_id = blocks[calculate_block_index(x, y, z, side_length)];
                if (g_block_defs[block_id].registered && !g_block_defs[block_id].transparent) {
                    current_sun_light = 0;
                }
                sun_light_data[calculate_block_index(x, y, z, side_length)] = current_sun_light;
                if (current_sun_light > 0) {
                    sun_queue[sun_queue_length * 3]     = x; 
                    sun_queue[sun_queue_length * 3 + 1] = y; 
                    sun_queue[sun_queue_length * 3 + 2] = z;
                    sun_queue_length++;
                }
            }
        }
    }

    static const int neighbor_offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    // BFS for source (torch) light propagation
    for (int i = 0; i < source_queue_length; i++) {
        int cx = source_queue[i * 3], cy = source_queue[i * 3 + 1], cz = source_queue[i * 3 + 2];
        int current_level = source_light_data[calculate_block_index(cx, cy, cz, side_length)];
        if (current_level <= 1) continue;
        
        for (int d = 0; d < 6; d++) {
            int nx = cx + neighbor_offsets[d][0], ny = cy + neighbor_offsets[d][1], nz = cz + neighbor_offsets[d][2];
            if (nx < 0 || nx >= side_length || ny < 0 || ny >= side_length || nz < 0 || nz >= side_length) continue;
            
            uint8_t neighbor_id = blocks[calculate_block_index(nx, ny, nz, side_length)];
            bool is_transparent = !g_block_defs[neighbor_id].registered || g_block_defs[neighbor_id].transparent;
            
            if (is_transparent && source_light_data[calculate_block_index(nx, ny, nz, side_length)] < current_level - 1) {
                source_light_data[calculate_block_index(nx, ny, nz, side_length)] = current_level - 1;
                if (source_queue_length < MAX_BLOCKS) {
                    source_queue[source_queue_length * 3]     = nx; 
                    source_queue[source_queue_length * 3 + 1] = ny; 
                    source_queue[source_queue_length * 3 + 2] = nz;
                    source_queue_length++;
                }
            }
        }
    }

    // BFS for sun light propagation
    for (int i = 0; i < sun_queue_length; i++) {
        int cx = sun_queue[i * 3], cy = sun_queue[i * 3 + 1], cz = sun_queue[i * 3 + 2];
        int current_level = sun_light_data[calculate_block_index(cx, cy, cz, side_length)];
        if (current_level <= 1) continue;
        
        for (int d = 0; d < 6; d++) {
            int nx = cx + neighbor_offsets[d][0], ny = cy + neighbor_offsets[d][1], nz = cz + neighbor_offsets[d][2];
            if (nx < 0 || nx >= side_length || ny < 0 || ny >= side_length || nz < 0 || nz >= side_length) continue;
            
            uint8_t neighbor_id = blocks[calculate_block_index(nx, ny, nz, side_length)];
            bool is_transparent = !g_block_defs[neighbor_id].registered || g_block_defs[neighbor_id].transparent;
            
            if (is_transparent && sun_light_data[calculate_block_index(nx, ny, nz, side_length)] < current_level - 1) {
                sun_light_data[calculate_block_index(nx, ny, nz, side_length)] = current_level - 1;
                if (sun_queue_length < MAX_BLOCKS) {
                    sun_queue[sun_queue_length * 3]     = nx; 
                    sun_queue[sun_queue_length * 3 + 1] = ny; 
                    sun_queue[sun_queue_length * 3 + 2] = nz;
                    sun_queue_length++;
                }
            }
        }
    }
}

// ============================================================
// AO + Smooth Lighting helpers
// ============================================================
static inline bool is_block_fully_opaque(int x, int y, int z, int side_length, const uint8_t* world_blocks) {
    if (x < 0 || x >= side_length || y < 0 || y >= side_length || z < 0 || z >= side_length) return false;
    uint8_t block_id = world_blocks[calculate_block_index(x, y, z, side_length)];
    if (block_id == 0) return false;
    return g_block_defs[block_id].registered && !g_block_defs[block_id].transparent;
}

static inline int calculate_vertex_ao_factor(bool side1_opaque, bool side2_opaque, bool corner_opaque) {
    if (side1_opaque && side2_opaque) return 3;
    return (side1_opaque ? 1 : 0) + (side2_opaque ? 1 : 0) + (corner_opaque ? 1 : 0);
}

static void calculate_ambient_occlusion_factors(int x, int y, int z, int direction, int side_length, const uint8_t* world_blocks, int out_ao_levels[4]) {
    #define IS_BLOCK_OPAQUE(nx, ny, nz) is_block_fully_opaque(nx, ny, nz, side_length, world_blocks)
    
    if (direction == 1) { // Front (+Z)
        int z_neighbor = z + 1;
        out_ao_levels[0] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y - 1, z_neighbor), IS_BLOCK_OPAQUE(x - 1, y - 1, z_neighbor));
        out_ao_levels[1] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y - 1, z_neighbor), IS_BLOCK_OPAQUE(x + 1, y - 1, z_neighbor));
        out_ao_levels[2] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y + 1, z_neighbor), IS_BLOCK_OPAQUE(x + 1, y + 1, z_neighbor));
        out_ao_levels[3] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y + 1, z_neighbor), IS_BLOCK_OPAQUE(x - 1, y + 1, z_neighbor));
    } else if (direction == 2) { // Back (-Z)
        int z_neighbor = z - 1;
        out_ao_levels[0] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y - 1, z_neighbor), IS_BLOCK_OPAQUE(x + 1, y - 1, z_neighbor));
        out_ao_levels[1] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y - 1, z_neighbor), IS_BLOCK_OPAQUE(x - 1, y - 1, z_neighbor));
        out_ao_levels[2] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y + 1, z_neighbor), IS_BLOCK_OPAQUE(x - 1, y + 1, z_neighbor));
        out_ao_levels[3] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y, z_neighbor), IS_BLOCK_OPAQUE(x, y + 1, z_neighbor), IS_BLOCK_OPAQUE(x + 1, y + 1, z_neighbor));
    } else if (direction == 3) { // Up (+Y)
        int y_neighbor = y + 1;
        out_ao_levels[0] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z + 1), IS_BLOCK_OPAQUE(x - 1, y_neighbor, z + 1));
        out_ao_levels[1] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z + 1), IS_BLOCK_OPAQUE(x + 1, y_neighbor, z + 1));
        out_ao_levels[2] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z - 1), IS_BLOCK_OPAQUE(x + 1, y_neighbor, z - 1));
        out_ao_levels[3] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z - 1), IS_BLOCK_OPAQUE(x - 1, y_neighbor, z - 1));
    } else if (direction == 4) { // Down (-Y)
        int y_neighbor = y - 1;
        out_ao_levels[0] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z - 1), IS_BLOCK_OPAQUE(x - 1, y_neighbor, z - 1));
        out_ao_levels[1] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z - 1), IS_BLOCK_OPAQUE(x + 1, y_neighbor, z - 1));
        out_ao_levels[2] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x + 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z + 1), IS_BLOCK_OPAQUE(x + 1, y_neighbor, z + 1));
        out_ao_levels[3] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x - 1, y_neighbor, z), IS_BLOCK_OPAQUE(x, y_neighbor, z + 1), IS_BLOCK_OPAQUE(x - 1, y_neighbor, z + 1));
    } else if (direction == 5) { // Right (+X)
        int x_neighbor = x + 1;
        out_ao_levels[0] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y - 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z - 1), IS_BLOCK_OPAQUE(x_neighbor, y - 1, z - 1));
        out_ao_levels[1] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y + 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z - 1), IS_BLOCK_OPAQUE(x_neighbor, y + 1, z - 1));
        out_ao_levels[2] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y + 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z + 1), IS_BLOCK_OPAQUE(x_neighbor, y + 1, z + 1));
        out_ao_levels[3] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y - 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z + 1), IS_BLOCK_OPAQUE(x_neighbor, y - 1, z + 1));
    } else { // Left (-X, direction 6)
        int x_neighbor = x - 1;
        out_ao_levels[0] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y + 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z - 1), IS_BLOCK_OPAQUE(x_neighbor, y + 1, z - 1));
        out_ao_levels[1] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y - 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z - 1), IS_BLOCK_OPAQUE(x_neighbor, y - 1, z - 1));
        out_ao_levels[2] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y - 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z + 1), IS_BLOCK_OPAQUE(x_neighbor, y - 1, z + 1));
        out_ao_levels[3] = calculate_vertex_ao_factor(IS_BLOCK_OPAQUE(x_neighbor, y + 1, z), IS_BLOCK_OPAQUE(x_neighbor, y, z + 1), IS_BLOCK_OPAQUE(x_neighbor, y + 1, z + 1));
    }
    #undef IS_BLOCK_OPAQUE
}

static inline void get_light_levels_safe(int x, int y, int z, int side_length, const uint8_t* sun_data, const uint8_t* source_data, int& sun_out, int& source_out) {
    if (x < 0 || x >= side_length || y < 0 || y >= side_length || z < 0 || z >= side_length) { 
        sun_out = 15; 
        source_out = 0; 
        return; 
    }
    sun_out = sun_data[calculate_block_index(x, y, z, side_length)]; 
    source_out = source_data[calculate_block_index(x, y, z, side_length)];
}

static void calculate_smoothed_vertex_light(int px, int py, int pz, int dx1, int dy1, int dz1, int dx2, int dy2, int dz2, int dx3, int dy3, int dz3,
    int side_length, const uint8_t* blocks, const uint8_t* sun_data, const uint8_t* source_data, float& sun_out, float& torch_out) {
    int l1s, l1t, l2s, l2t, l3s, l3t, l4s, l4t;
    get_light_levels_safe(px, py, pz, side_length, sun_data, source_data, l1s, l1t);
    get_light_levels_safe(px + dx1, py + dy1, pz + dz1, side_length, sun_data, source_data, l2s, l2t);
    get_light_levels_safe(px + dx2, py + dy2, pz + dz2, side_length, sun_data, source_data, l3s, l3t);
    
    bool is_op1 = is_block_fully_opaque(px + dx1, py + dy1, pz + dz1, side_length, blocks);
    bool is_op2 = is_block_fully_opaque(px + dx2, py + dy2, pz + dz2, side_length, blocks);
    
    if (is_op1 && is_op2) { 
        l4s = 0; 
        l4t = 0; 
    } else { 
        get_light_levels_safe(px + dx3, py + dy3, pz + dz3, side_length, sun_data, source_data, l4s, l4t); 
    }
    
    sun_out = (l1s + l2s + l3s + l4s) / 60.0f; // Average of 4 corners, normalized (4 * 15 = 60)
    torch_out = (l1t + l2t + l3t + l4t) / 60.0f;
}

// ============================================================
// Mesh Output Buffers (pre-allocated)
// ============================================================
static float* g_out_pos   = 0;    // 3 per vert: x, y, z
static float* g_out_tc0   = 0;    // 4 per vert: base_u, base_v, unit_u, unit_v
static float* g_out_tc1   = 0;    // 4 per vert: local_u, local_v, width, height
static uint8_t* g_out_col = 0;    // 4 per vert: AO corners (ubyte4)
static uint8_t* g_out_col1= 0;    // 4 per vert: Torch corners (ubyte4)
static uint8_t* g_out_col2= 0;    // 4 per vert: Sun corners (ubyte4)
static float* g_out_f_id  = 0;    // 1 per vert: face_id
static uint32_t g_max_quads = 0;
static uint32_t g_result_quad_count = 0;
static uint32_t g_result_face_count = 0;
static uint32_t g_result_vertex_count = 0;
static double g_result_build_time = 0;

static void alloc_mesh_buffers(int grid_size) {
    g_max_quads = grid_size * grid_size * grid_size * 3;
    uint32_t max_verts = g_max_quads * 6;
    g_out_pos  = (float*)realloc(g_out_pos,  max_verts * 3 * sizeof(float));
    g_out_tc0  = (float*)realloc(g_out_tc0,  max_verts * 4 * sizeof(float));
    g_out_tc1  = (float*)realloc(g_out_tc1,  max_verts * 4 * sizeof(float));
    g_out_col  = (uint8_t*)realloc(g_out_col, max_verts * 4 * sizeof(uint8_t));
    g_out_col1 = (uint8_t*)realloc(g_out_col1,max_verts * 4 * sizeof(uint8_t));
    g_out_col2 = (uint8_t*)realloc(g_out_col2,max_verts * 4 * sizeof(uint8_t));
    g_out_f_id = (float*)realloc(g_out_f_id,  max_verts * 1 * sizeof(float));
    
}

// ============================================================
// Greedy Mesh Generation
// ============================================================
static void append_quad_to_mesh_buffers(int quad_idx, float p1x, float p1y, float p1z, float p2x, float p2y, float p2z,
    float p3x, float p3y, float p3z, float p4x, float p4y, float p4z,
    const UVData* uv_data, int quad_width, int quad_height, float normal_x, float normal_y, float normal_z,
    int ao_levels[4], float vertex_sun_light[4], float vertex_source_light[4], int face_direction) {

    int base_v3 = quad_idx * 18; // 6 verts * 3 comps
    int base_v4 = quad_idx * 24; // 6 verts * 4 comps
    int base_v1 = quad_idx * 6;  // 6 verts * 1 comp
    
    // Normalized light [0..1] -> [0..255]
    uint8_t ao_b[4], sun_b[4], torch_b[4];
    for(int i=0; i<4; i++) {
        ao_b[i]    = (uint8_t)(AMBIENT_OCCLUSION_LEVELS[ao_levels[i]] * 255.0f);
        sun_b[i]   = (uint8_t)(vertex_sun_light[i] * 255.0f);
        torch_b[i] = (uint8_t)(vertex_source_light[i] * 255.0f);
    }

    // Standard Winding (BL, BR, TR, TL) -> two triangles: (0, 1, 2) and (0, 2, 3)
    float px[4] = {p1x, p2x, p3x, p4x};
    float py[4] = {p1y, p2y, p3y, p4y};
    float pz[4] = {p1z, p2z, p3z, p4z};
    int indices[6] = {0, 1, 2, 0, 2, 3};

    // tc1: x,y = local_uv (0..1), z,w = quad_size
    float lux[4] = {0, 1, 1, 0};
    float luy[4] = {0, 0, 1, 1};

    for(int i = 0; i < 6; i++) {
        int idx = indices[i];
        int v_off = i;
        
        // Position
        g_out_pos[base_v3 + v_off * 3 + 0] = px[idx];
        g_out_pos[base_v3 + v_off * 3 + 1] = py[idx];
        g_out_pos[base_v3 + v_off * 3 + 2] = pz[idx];

        // tc0: base_u, base_v, unit_w, unit_h
        g_out_tc0[base_v4 + v_off * 4 + 0] = uv_data->u;
        g_out_tc0[base_v4 + v_off * 4 + 1] = uv_data->v;
        g_out_tc0[base_v4 + v_off * 4 + 2] = uv_data->w;
        g_out_tc0[base_v4 + v_off * 4 + 3] = uv_data->h;

        // tc1: local_u, local_v, quad_w, quad_h
        g_out_tc1[base_v4 + v_off * 4 + 0] = lux[idx];
        g_out_tc1[base_v4 + v_off * 4 + 1] = luy[idx];
        g_out_tc1[base_v4 + v_off * 4 + 2] = (float)quad_width;
        g_out_tc1[base_v4 + v_off * 4 + 3] = (float)quad_height;

        // face_id
        g_out_f_id[base_v1 + v_off] = (float)face_direction;

        // Packed Colors (Pass all 4 corners to every vertex)
        for(int j=0; j<4; j++) {
            g_out_col [base_v4 + v_off * 4 + j] = ao_b[j];
            g_out_col1[base_v4 + v_off * 4 + j] = torch_b[j];
            g_out_col2[base_v4 + v_off * 4 + j] = sun_b[j];
        }
    }
}

static void execute_mesh_generation_pipeline(const uint8_t* world_blocks, const uint8_t* sun_light, const uint8_t* source_light,
    int side_length, bool ao_enabled, int light_mode) {

    uint64_t start_time = dmTime::GetTime();
    int current_quad_index = 0, total_face_count = 0;
    static uint64_t greedy_mask[MAX_GRID_SIZE * MAX_GRID_SIZE];

    for (int face_direction = 1; face_direction <= 6; face_direction++) {
        for (int slice_idx = 0; slice_idx < side_length; slice_idx++) {
            memset(greedy_mask, 0, side_length * side_length * sizeof(uint64_t));

            // Visibility scan for current slice and direction
            for (int v_idx = 0; v_idx < side_length; v_idx++) {
                for (int u_idx = 0; u_idx < side_length; u_idx++) {
                    int x, y, z, nx, ny, nz;
                    if (face_direction == 1)      { x = u_idx;     y = v_idx;     z = slice_idx; nx = x; ny = y; nz = z + 1; }
                    else if (face_direction == 2) { x = u_idx;     y = v_idx;     z = slice_idx; nx = x; ny = y; nz = z - 1; }
                    else if (face_direction == 3) { x = u_idx;     y = slice_idx; z = v_idx;     nx = x; ny = y + 1; nz = z; }
                    else if (face_direction == 4) { x = u_idx;     y = slice_idx; z = v_idx;     nx = x; ny = y - 1; nz = z; }
                    else if (face_direction == 5) { x = slice_idx; y = v_idx;     z = u_idx;     nx = x + 1; ny = y; nz = z; }
                    else                          { x = slice_idx; y = v_idx;     z = u_idx;     nx = x - 1; ny = y; nz = z; }

                    if (x < 0 || x >= side_length || y < 0 || y >= side_length || z < 0 || z >= side_length) continue;
                    uint8_t current_id = world_blocks[calculate_block_index(x, y, z, side_length)];
                    if (current_id == 0 || !g_block_defs[current_id].registered || g_block_defs[current_id].transparent) continue;

                    uint8_t neighbor_id = (nx < 0 || nx >= side_length || ny < 0 || ny >= side_length || nz < 0 || nz >= side_length) ? 0 : world_blocks[calculate_block_index(nx, ny, nz, side_length)];
                    bool neighbor_is_transparent = (neighbor_id == 0) || !g_block_defs[neighbor_id].registered || g_block_defs[neighbor_id].transparent;
                    if (!neighbor_is_transparent) continue;

                    int ao_levels[4] = {0, 0, 0, 0};
                    if (ao_enabled) calculate_ambient_occlusion_factors(x, y, z, face_direction, side_length, world_blocks, ao_levels);

                    int sun_lvl = 15, torch_lvl = 0;
                    if (light_mode == 0 || light_mode == 1) get_light_levels_safe(nx, ny, nz, side_length, sun_light, source_light, sun_lvl, torch_lvl);
                    
                    uint64_t packed_ao = ao_levels[0] | (ao_levels[1] << 2) | (ao_levels[2] << 4) | (ao_levels[3] << 6);
                    uint64_t packed_light = sun_lvl | (torch_lvl << 4);
                    greedy_mask[v_idx * side_length + u_idx] = (uint64_t)current_id | (packed_ao << 16) | (packed_light << 24) | (1ULL << 48);
                }
            }

            // Greedy merge on the current slice mask
            for (int v_idx = 0; v_idx < side_length; v_idx++) {
                for (int u_idx = 0; u_idx < side_length; u_idx++) {
                    uint64_t mask_val = greedy_mask[v_idx * side_length + u_idx];
                    if (!mask_val) continue;

                    uint16_t block_id = mask_val & 0xFFFF;
                    int packed_ao = (mask_val >> 16) & 0xFF;
                    int packed_light = (mask_val >> 24) & 0xFF;
                    int sun_lvl = packed_light & 0xF, torch_lvl = (packed_light >> 4) & 0xF;
                    int ao_levels[4] = {packed_ao & 3, (packed_ao >> 2) & 3, (packed_ao >> 4) & 3, (packed_ao >> 6) & 3};

                    int width = 1, height = 1;
                    while (u_idx + width < side_length && greedy_mask[v_idx * side_length + u_idx + width] == mask_val) width++;
                    bool can_merge_row = true;
                    while (v_idx + height < side_length && can_merge_row) {
                        for (int r = 0; r < width; r++) { if (greedy_mask[(v_idx + height) * side_length + u_idx + r] != mask_val) { can_merge_row = false; break; } }
                        if (can_merge_row) height++;
                    }

                    float vertex_sun[4], vertex_torch[4];
                    if (light_mode == 1) { // Smooth
                        int cx, cy, cz;
                        if      (face_direction == 1) { cx = u_idx; cy = v_idx; cz = slice_idx + 1; }
                        else if (face_direction == 2) { cx = u_idx; cy = v_idx; cz = slice_idx - 1; }
                        else if (face_direction == 3) { cx = u_idx; cy = slice_idx + 1; cz = v_idx; }
                        else if (face_direction == 4) { cx = u_idx; cy = slice_idx - 1; cz = v_idx; }
                        else if (face_direction == 5) { cx = slice_idx + 1; cy = v_idx; cz = u_idx; }
                        else                          { cx = slice_idx - 1; cy = v_idx; cz = u_idx; }

                        #define COMP_LT(idx, px, py, pz, dx1, dy1, dz1, dx2, dy2, dz2, dx3, dy3, dz3) \
                            calculate_smoothed_vertex_light(px, py, pz, dx1, dy1, dz1, dx2, dy2, dz2, dx3, dy3, dz3, side_length, world_blocks, sun_light, source_light, vertex_sun[idx], vertex_torch[idx])
                        
                        // Standardized Corner Order: 0:BL, 1:BR, 2:TR, 3:TL
                        if (face_direction == 1) { // +Z: H:X, V:Y
                            COMP_LT(0, cx, cy, cz, -1,0,0, 0,-1,0, -1,-1,0); COMP_LT(1, cx+width, cy, cz, 1,0,0, 0,-1,0, 1,-1,0); 
                            COMP_LT(2, cx+width, cy+height, cz, 1,0,0, 0,1,0, 1,1,0); COMP_LT(3, cx, cy+height, cz, -1,0,0, 0,1,0, -1,1,0);
                        } else if (face_direction == 2) { // -Z: H:X, V:Y (CCW)
                            COMP_LT(0, cx+width, cy, cz, 1,0,0, 0,-1,0, 1,-1,0); COMP_LT(1, cx, cy, cz, -1,0,0, 0,-1,0, -1,-1,0);
                            COMP_LT(2, cx, cy+height, cz, -1,0,0, 0,1,0, -1,1,0); COMP_LT(3, cx+width, cy+height, cz, 1,0,0, 0,1,0, 1,1,0);
                        } else if (face_direction == 3) { // +Y: H:X, V:Z
                            COMP_LT(0, cx, cy, cz, -1,0,0, 0,0,-1, -1,0,-1); COMP_LT(1, cx+width, cy, cz, 1,0,0, 0,0,-1, 1,0,-1);
                            COMP_LT(2, cx+width, cy, cz+height, 1,0,0, 0,0,1, 1,0,1); COMP_LT(3, cx, cy, cz+height, -1,0,0, 0,0,1, -1,0,1);
                        } else if (face_direction == 4) { // -Y: H:X, V:Z 
                            COMP_LT(0, cx, cy, cz+height, -1,0,0, 0,0,1, -1,0,1); COMP_LT(1, cx+width, cy, cz+height, 1,0,0, 0,0,1, 1,0,1);
                            COMP_LT(2, cx+width, cy, cz, 1,0,0, 0,0,-1, 1,0,-1); COMP_LT(3, cx, cy, cz, -1,0,0, 0,0,-1, -1,0,-1);
                        } else if (face_direction == 5) { // +X: H:Z, V:Y
                            COMP_LT(0, cx, cy, cz, 0,-1,0, 0,0,-1, 0,-1,-1); COMP_LT(1, cx, cy, cz+width, 0,-1,0, 0,0,1, 0,-1,1);
                            COMP_LT(2, cx, cy+height, cz+width, 0,1,0, 0,0,1, 0,1,1); COMP_LT(3, cx, cy+height, cz, 0,1,0, 0,0,-1, 0,1,-1);
                        } else { // -X: H:Z, V:Y
                            COMP_LT(0, cx, cy, cz+width, 0,-1,0, 0,0,1, 0,-1,1); COMP_LT(1, cx, cy, cz, 0,-1,0, 0,0,-1, 0,-1,-1);
                            COMP_LT(2, cx, cy+height, cz, 0,1,0, 0,0,-1, 0,1,-1); COMP_LT(3, cx, cy+height, cz+width, 0,1,0, 0,0,1, 0,1,1);
                        }
                        #undef COMP_LT
                    } else { // Flat
                        for (int i=0; i<4; i++) { vertex_sun[i] = (float)sun_lvl / 15.0f; vertex_torch[i] = (float)torch_lvl / 15.0f; }
                    }

                    // Quad geometry: Shift by -0.5f and map Corners: 0:BL, 1:BR, 2:TR, 3:TL
                    float p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z;
                    float nrm_x=0, nrm_y=0, nrm_z=0, off=-0.5f;
                    if (face_direction == 1) { // +Z: Normal (0,0,1)
                        p1x=u_idx+off;       p1y=v_idx+off;        p1z=slice_idx+1+off;
                        p2x=u_idx+width+off; p2y=v_idx+off;        p2z=slice_idx+1+off;
                        p3x=u_idx+width+off; p3y=v_idx+height+off; p3z=slice_idx+1+off;
                        p4x=u_idx+off;       p4y=v_idx+height+off; p4z=slice_idx+1+off; nrm_z=1;
                    } else if (face_direction == 2) { // -Z: Normal (0,0,-1)
                        p1x=u_idx+width+off; p1y=v_idx+off;        p1z=slice_idx+off;
                        p2x=u_idx+off;       p2y=v_idx+off;        p2z=slice_idx+off;
                        p3x=u_idx+off;       p3y=v_idx+height+off; p3z=slice_idx+off;
                        p4x=u_idx+width+off; p4y=v_idx+height+off; p4z=slice_idx+off; nrm_z=-1;
                    } else if (face_direction == 3) { // +Y: Normal (0,1,0)
                        p1x=u_idx+off;       p1y=slice_idx+1+off;  p1z=v_idx+height+off;
                        p2x=u_idx+width+off; p2y=slice_idx+1+off;  p2z=v_idx+height+off;
                        p3x=u_idx+width+off; p3y=slice_idx+1+off;  p3z=v_idx+off;
                        p4x=u_idx+off;       p4y=slice_idx+1+off;  p4z=v_idx+off; nrm_y=1;
                    } else if (face_direction == 4) { // -Y: Normal (0,-1,0)
                        p1x=u_idx+off;       p1y=slice_idx+off;    p1z=v_idx+off;
                        p2x=u_idx+width+off; p2y=slice_idx+off;    p2z=v_idx+off;
                        p3x=u_idx+width+off; p3y=slice_idx+off;    p3z=v_idx+height+off;
                        p4x=u_idx+off;       p4y=slice_idx+off;    p4z=v_idx+height+off; nrm_y=-1;
                    } else if (face_direction == 5) { // +X: Normal (1,0,0)
                        p1x=slice_idx+1+off; p1y=v_idx+off;        p1z=u_idx+width+off;
                        p2x=slice_idx+1+off; p2y=v_idx+off;        p2z=u_idx+off;
                        p3x=slice_idx+1+off; p3y=v_idx+height+off; p3z=u_idx+off;
                        p4x=slice_idx+1+off; p4y=v_idx+height+off; p4z=u_idx+width+off; nrm_x=1;
                    } else { // -X: Normal (-1,0,0)
                        p1x=slice_idx+off;   p1y=v_idx+off;        p1z=u_idx+off;
                        p2x=slice_idx+off;   p2y=v_idx+off;        p2z=u_idx+width+off;
                        p3x=slice_idx+off;   p3y=v_idx+height+off; p3z=u_idx+width+off;
                        p4x=slice_idx+off;   p4y=v_idx+height+off; p4z=u_idx+off; nrm_x=-1;
                    }

                    append_quad_to_mesh_buffers(current_quad_index, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, &g_block_defs[block_id].uvs[face_direction], width, height, nrm_x, nrm_y, nrm_z, ao_levels, vertex_sun, vertex_torch, face_direction);
                    current_quad_index++;
                    for (int r=0; r<height; r++) for (int c=0; c<width; c++) greedy_mask[(v_idx+r)*side_length + u_idx+c] = 0;
                    total_face_count++;
                }
            }
        }
    }

    g_result_quad_count   = current_quad_index;
    g_result_face_count   = total_face_count;
    g_result_vertex_count = current_quad_index * 6;
    g_result_build_time   = (dmTime::GetTime() - start_time) / 1000.0;
}

// ============================================================
// Worker Thread
// ============================================================
static dmThread::Thread g_worker_thread = 0;
static dmMutex::HMutex g_mutex = 0;
static volatile bool g_worker_running = false;
static volatile bool g_worker_has_work = false;
static volatile bool g_worker_result_ready = false;

static void WorkerThreadLoop(void* arg) {
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
            execute_mesh_generation_pipeline(g_blocks, g_sun_light, g_source_light, g_grid_size, g_ao_enabled, g_light_mode);

            dmMutex::Lock(g_mutex);
            g_worker_result_ready = true;
            dmMutex::Unlock(g_mutex);
        }
        dmTime::Sleep(1000); // 1ms idle sleep
    }
}

// ============================================================
// Lua API Bridge
// ============================================================

static int Lua_InitializeTerrainEngine(lua_State* L) {
    int side_length = luaL_checkinteger(L, 1);
    if (side_length > MAX_GRID_SIZE) side_length = MAX_GRID_SIZE;
    g_grid_size = side_length;
    g_seed = luaL_checkinteger(L, 2);
    
    initialize_world_terrain_data();
    alloc_mesh_buffers(side_length);

    if (!g_mutex) g_mutex = dmMutex::New();
    if (!g_worker_running) {
        g_worker_running = true;
        g_worker_has_work = false;
        g_worker_result_ready = false;
        g_worker_thread = dmThread::New(WorkerThreadLoop, 0x80000, 0, "TerrainWorker");
    }
    return 0;
}

static int Lua_ShutdownTerrainEngine(lua_State* L) {
    g_worker_running = false;
    if (g_worker_thread) { 
        dmThread::Join(g_worker_thread); 
        g_worker_thread = 0; 
    }
    if (g_mutex) { 
        dmMutex::Delete(g_mutex); 
        g_mutex = 0; 
    }
    
    free(g_out_pos);  g_out_pos = 0;
    free(g_out_tc0);  g_out_tc0 = 0;
    free(g_out_tc1);  g_out_tc1 = 0;
    free(g_out_col);  g_out_col = 0;
    free(g_out_col1); g_out_col1 = 0;
    free(g_out_col2); g_out_col2 = 0;
    free(g_out_f_id); g_out_f_id = 0;
    
    return 0;
}

static int Lua_RegisterBlockType(lua_State* L) {
    int id = luaL_checkinteger(L, 1);
    if (id < 0 || id > 255) return 0;
    
    BlockDef& bd = g_block_defs[id];
    bd.registered = true;
    bd.transparent = lua_toboolean(L, 2);
    bd.solid = lua_toboolean(L, 3);
    bd.light_level = luaL_checkinteger(L, 4);

    if (lua_istable(L, 5)) {
        for (int face = 1; face <= 6; face++) {
            lua_rawgeti(L, 5, face);
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1); bd.uvs[face].u = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); bd.uvs[face].v = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 3); bd.uvs[face].w = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 4); bd.uvs[face].h = (float)lua_tonumber(L, -1); lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int Lua_SetBlockInWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    int id = luaL_checkinteger(L, 4);
    if (x >= 0 && x < g_grid_size && y >= 0 && y < g_grid_size && z >= 0 && z < g_grid_size) {
        g_blocks[calculate_block_index(x, y, z, g_grid_size)] = (uint8_t)id;
    }
    return 0;
}

static int Lua_GetBlockFromWorld(lua_State* L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    if (x < 0 || x >= g_grid_size || y < 0 || y >= g_grid_size || z < 0 || z >= g_grid_size) {
        lua_pushinteger(L, 0); 
        return 1; 
    }
    lua_pushinteger(L, g_blocks[calculate_block_index(x, y, z, g_grid_size)]);
    return 1;
}

static int Lua_GetLightLevels(lua_State* L) {
    int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2), z = luaL_checkinteger(L, 3);
    if (x < 0 || x >= g_grid_size || y < 0 || y >= g_grid_size || z < 0 || z >= g_grid_size) {
        lua_pushinteger(L, 15); 
        lua_pushinteger(L, 0); 
        return 2; 
    }
    lua_pushinteger(L, g_sun_light[calculate_block_index(x, y, z, g_grid_size)]);
    lua_pushinteger(L, g_source_light[calculate_block_index(x, y, z, g_grid_size)]);
    return 2;
}

static int Lua_GetMaxVertices(lua_State* L) {
    uint32_t max_verts = (uint32_t)g_grid_size * g_grid_size * g_grid_size * 3 * 6;
    lua_pushinteger(L, max_verts);
    return 1;
}

static int Lua_RequestAsyncMeshUpdate(lua_State* L) {
    if (!g_worker_thread || !g_mutex) return 0;
    
    g_ao_enabled = lua_toboolean(L, 1);
    g_light_mode = luaL_checkinteger(L, 2);
    
    dmMutex::Lock(g_mutex);
    g_worker_has_work = true;
    dmMutex::Unlock(g_mutex);
    return 0;
}

static void copy_array_to_buffer_stream(dmBuffer::HBuffer buffer, dmhash_t stream_name, const float* source_data, uint32_t vertex_count, uint32_t components_per_vertex) {
    float* stream_ptr = 0;
    uint32_t stream_count = 0, stream_components = 0, stream_stride = 0;
    dmBuffer::Result res = dmBuffer::GetStream(buffer, stream_name, (void**)&stream_ptr, &stream_count, &stream_components, &stream_stride);
    if (res != dmBuffer::RESULT_OK || !stream_ptr || stream_count < vertex_count) return;
    for (uint32_t i = 0; i < vertex_count; i++) {
        for (uint32_t c = 0; c < stream_components && c < components_per_vertex; c++) {
            stream_ptr[i * stream_stride + c] = source_data[i * components_per_vertex + c];
        }
    }
}

static void copy_byte_array_to_buffer_stream(dmBuffer::HBuffer buffer, dmhash_t stream_name, const uint8_t* source_data, uint32_t vertex_count, uint32_t components_per_vertex) {
    uint8_t* stream_ptr = 0;
    uint32_t stream_count = 0, stream_components = 0, stream_stride = 0;
    dmBuffer::Result res = dmBuffer::GetStream(buffer, stream_name, (void**)&stream_ptr, &stream_count, &stream_components, &stream_stride);
    if (res != dmBuffer::RESULT_OK || !stream_ptr || stream_count < vertex_count) return;
    for (uint32_t i = 0; i < vertex_count; i++) {
        for (uint32_t c = 0; c < stream_components && c < components_per_vertex; c++) {
            stream_ptr[i * stream_stride + c] = source_data[i * components_per_vertex + c];
        }
    }
}

static int Lua_PollMeshBuffer(lua_State* L) {
    if (!g_worker_thread || !g_out_pos) { lua_pushnil(L); return 1; }

    dmMutex::Lock(g_mutex);
    bool is_ready = g_worker_result_ready;
    uint32_t v_count = g_result_vertex_count;
    uint32_t f_count = g_result_face_count;
    uint32_t q_count = g_result_quad_count;
    double build_time = g_result_build_time;
    dmMutex::Unlock(g_mutex);

    if (!is_ready) { lua_pushnil(L); return 1; }

    if (v_count == 0) {
        // Return 0-size buffer to clear old mesh
        dmBuffer::StreamDeclaration empty_decl[] = { { dmHashString64("position"),  dmBuffer::VALUE_TYPE_FLOAT32, 3 } };
        dmBuffer::HBuffer buffer_handle = 0;
        dmBuffer::Create(0, empty_decl, 1, &buffer_handle);
        dmScript::LuaHBuffer luabuf(buffer_handle, dmScript::OWNER_LUA);
        dmScript::PushBuffer(L, luabuf);
        lua_pushinteger(L, 0); lua_pushinteger(L, 0); lua_pushinteger(L, 0);
        lua_pushnumber(L, build_time);
        
        dmMutex::Lock(g_mutex);
        g_worker_result_ready = false;
        dmMutex::Unlock(g_mutex);
        return 5;
    }

    dmBuffer::StreamDeclaration streams_decl[] = {
        { dmHashString64("position"),  dmBuffer::VALUE_TYPE_FLOAT32, 3 },
        { dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 4 }, // base_u, base_v, unit_w, unit_h
        { dmHashString64("texcoord1"), dmBuffer::VALUE_TYPE_FLOAT32, 4 }, // local_u, local_v, quad_w, quad_h
        { dmHashString64("color"),     dmBuffer::VALUE_TYPE_UINT8,   4 }, // AO (ubyte4)
        { dmHashString64("color1"),    dmBuffer::VALUE_TYPE_UINT8,   4 }, // Torch (ubyte4)
        { dmHashString64("color2"),    dmBuffer::VALUE_TYPE_UINT8,   4 }, // Sun (ubyte4)
        { dmHashString64("texcoord2"), dmBuffer::VALUE_TYPE_FLOAT32, 1 }  // face_id
    };

    dmBuffer::HBuffer buffer_handle = 0;
    dmBuffer::Result res = dmBuffer::Create(v_count, streams_decl, 7, &buffer_handle);
    if (res != dmBuffer::RESULT_OK) {
        dmLogError("terrain_engine: Failed to create dmBuffer, error code=%d", res);
        lua_pushnil(L);
        return 1;
    }

    copy_array_to_buffer_stream(buffer_handle, dmHashString64("position"),  g_out_pos,  v_count, 3);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord0"), g_out_tc0,  v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord1"), g_out_tc1,  v_count, 4);
    copy_byte_array_to_buffer_stream(buffer_handle, dmHashString64("color"), g_out_col,  v_count, 4);
    copy_byte_array_to_buffer_stream(buffer_handle, dmHashString64("color1"),g_out_col1, v_count, 4);
    copy_byte_array_to_buffer_stream(buffer_handle, dmHashString64("color2"),g_out_col2, v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord2"), g_out_f_id, v_count, 1);

    dmBuffer::UpdateContentVersion(buffer_handle);

    dmScript::LuaHBuffer luabuf(buffer_handle, dmScript::OWNER_LUA);
    dmScript::PushBuffer(L, luabuf);
    lua_pushinteger(L, v_count);
    lua_pushinteger(L, f_count);
    lua_pushinteger(L, q_count);
    lua_pushnumber(L, build_time);

    dmMutex::Lock(g_mutex);
    g_worker_result_ready = false;
    dmMutex::Unlock(g_mutex);

    return 5;
}

static int Lua_PerformLightingPassSync(lua_State* L) {
    perform_lighting_pass(g_blocks, g_sun_light, g_source_light, g_grid_size);
    return 0;
}

// ============================================================
// Extension Lifecycle
// ============================================================

static const luaL_reg Module_methods[] = {
    {"init",                 Lua_InitializeTerrainEngine},
    {"shutdown",             Lua_ShutdownTerrainEngine},
    {"register_block",       Lua_RegisterBlockType},
    {"set_block",            Lua_SetBlockInWorld},
    {"get_block",            Lua_GetBlockFromWorld},
    {"get_lights",           Lua_GetLightLevels},
    {"get_max_vertices",     Lua_GetMaxVertices},
    {"request_mesh_update",  Lua_RequestAsyncMeshUpdate},
    {"poll_mesh_buffer",     Lua_PollMeshBuffer},
    {"recalc_lighting_sync", Lua_PerformLightingPassSync},
    {0, 0}
};

static dmExtension::Result AppInit(dmExtension::AppParams* params) { return dmExtension::RESULT_OK; }
static dmExtension::Result AppFinal(dmExtension::AppParams* params) { return dmExtension::RESULT_OK; }

static dmExtension::Result Init(dmExtension::Params* params) {
    luaL_register(params->m_L, LIB_NAME, Module_methods);
    lua_pop(params->m_L, 1);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Final(dmExtension::Params* params) {
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalFunc(dmExtension::AppParams* params) {
    // Safety cleanup if shutdown wasn't called
    if (g_worker_running) {
        g_worker_running = false;
        if (g_worker_thread) { dmThread::Join(g_worker_thread); g_worker_thread = 0; }
        if (g_mutex) { dmMutex::Delete(g_mutex); g_mutex = 0; }
        free(g_out_pos); free(g_out_tc0); free(g_out_tc1);
        free(g_out_col); free(g_out_col1); free(g_out_col2); free(g_out_f_id);
        g_out_pos=g_out_tc0=g_out_tc1=0;
        g_out_col=g_out_col1=g_out_col2=0; g_out_f_id=0;
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInit, AppFinalFunc, Init, Final, 0, 0)
