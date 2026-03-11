#include "ve_world.h"
#include <math.h>
#include <stdlib.h>

// Perlin Noise logic
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

float calculate_perlin_noise_3d(float x, float y, float z) {
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

int calculate_ground_height(int x, int z) {
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

void initialize_world_terrain_data() {
    int side_length = g_grid_size;
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
    dmLogInfo("voxel_engine: Generated terrain, grid_size=%d", side_length);
}
