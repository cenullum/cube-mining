#version 140

// --- Inputs ---
in mediump vec2 var_texcoord0;     // Normalized quad coordinate [0..1]
in mediump vec4 var_atlas_metadata; // Atlas bounds [u, v, width, height]

// --- Output ---
out vec4 out_fragColor;

uniform mediump sampler2D texture0;

uniform fs_uniforms
{
    mediump vec4 tint;
};

void main()
{
    // Linear sub-region sampling: maps 0..1 quad coordinates to the atlas region.
    // This avoids "bleeding" or "wrapping" artifacts from neighboring atlas tiles.
    vec2 atlas_uv = var_texcoord0 * var_atlas_metadata.zw + var_atlas_metadata.xy;
    vec4 color = texture(texture0, atlas_uv) * tint;
    
    // Hard discard for pixel art transparency
    if (color.a < 0.1) discard;
    
    out_fragColor = color;
}
