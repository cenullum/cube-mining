#pragma once

// Computes robust 2D Perlin noise
float FastPerlin2D(float x, float y, int seed);

// Deterministic 3D hash for feature placement
int Hash3D(int x, int y, int z, int seed);

// Calculates ground height for a given world x, z
int CalculateGroundHeight(int x, int z, int seed);
