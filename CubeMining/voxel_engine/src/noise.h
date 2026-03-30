#pragma once

// Computes robust 2D Perlin noise
float FastPerlin2D(float x, float y, int seed);

// Calculates ground height for a given world x, z
int CalculateGroundHeight(int x, int z, int seed);
