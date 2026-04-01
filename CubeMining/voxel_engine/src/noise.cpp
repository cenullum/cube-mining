#include "noise.h"
#include <cmath>

static inline float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static inline float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static inline float grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

// Memory-less hash function instead of generating a permutation array
static inline int hash2(int x, int y, int seed) {
    int h = seed + x * 374761393 + y * 668265263;
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}

int Hash3D(int x, int y, int z, int seed) {
    int h = seed + x * 374761393 + y * 668265263 + z * 884748317;
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}

// Extremely fast 2D Perlin Noise using inline hash
float FastPerlin2D(float x, float y, int seed) {
    int X = (int)floorf(x);
    int Y = (int)floorf(y);
    x -= X;
    y -= Y;
    
    float u = fade(x);
    float v = fade(y);
    
    int aa = hash2(X, Y, seed);
    int ab = hash2(X, Y + 1, seed);
    int ba = hash2(X + 1, Y, seed);
    int bb = hash2(X + 1, Y + 1, seed);
    
    float res = lerp(v,
        lerp(u, grad(aa, x, y), grad(ba, x - 1, y)),
        lerp(u, grad(ab, x, y - 1), grad(bb, x - 1, y - 1)));
        
    return res;
}

// Generates height value for world coordinates
int CalculateGroundHeight(int x, int z, int seed) {
    float frequency = 0.012f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    float height_val = 0.0f;
    
    // Fractal Brownian Motion
    for (int i = 0; i < 4; i++) {
        height_val += FastPerlin2D((float)x * frequency, (float)z * frequency, seed) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    // Normalize to roughly 0-1 range (Perlin returns ~ -1 to 1)
    float normalized = (height_val / maxValue) * 0.5f + 0.5f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    // Base height 10, variation 54 (Total max 64)
    return 10 + (int)(normalized * 54.0f);
}
