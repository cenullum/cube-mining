#version 140

// --- Inputs from Vertex Shader ---
in mediump vec2 var_texcoord0;     // Tiling coordinate (scaled by quad size)
in mediump vec4 var_atlas_metadata; // Atlas bounds
in mediump float var_light;        // Directional Lighting factor
in mediump float var_ao;           // Ambient Occlusion factor
in mediump float var_torch_light;
in mediump float var_sun_light;
in mediump vec3 var_view_pos;       // View-space position
in mediump vec3 var_pos;            // Local position for block breaking
in mediump vec3 var_normal;         // Normal for block breaking

// --- Fragment Output ---
out vec4 out_fragColor;

// --- Samplers ---
uniform mediump sampler2D texture0; // The block texture atlas (nearest filtered)
uniform mediump sampler2D texture1; // The breaking texture spritesheet

// --- Uniforms ---
uniform fs_uniforms
{
    mediump vec4 tint; // Face tint color (default white)
    mediump vec4 fog_color;
    mediump vec4 fog_params;
    mediump vec4 break_info; // x: frame, y: enabled, z: total_frames
    mediump vec4 break_pos;  // xyz: block grid coordinates
};

void main()
{
    // Pre-multiply alpha for proper engine blending
    vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);
    
    // UV Tiling & Atlas Wrapping Logic:
    // 1. mod(var_texcoord0, var_atlas_metadata.zw): 
    //    Repeats the UV coordinate only within the width/height of the tile.
    // 2. + var_atlas_metadata.xy: 
    //    Offsets the repeated coordinate to the correct location in the atlas.
    vec2 atlas_uv = mod(var_texcoord0, var_atlas_metadata.zw) + var_atlas_metadata.xy;
    
    // Sample texture with the calculated atlas UV
    vec4 color = texture(texture0, atlas_uv) * tint_pm;

    // Overlay Breaking Texture
    if (break_info.y > 0.5) {
        // Offset position slightly anti-normal to get the coordinate of the internal block
        // to avoid floating point precision issues at block boundaries
        vec3 interior_pos = var_pos - var_normal * 0.05;
        vec3 grid_pos = floor(interior_pos + 0.5);
        
        // Exact match for the targeted block
        if (all(lessThan(abs(grid_pos - break_pos.xyz), vec3(0.01)))) {
            // Calculate UVs relative to the block's local 1x1x1 volume
            // fract(var_pos + 0.5) gives a 0..1 range within each block unit
            vec2 block_uv;
            if (abs(var_normal.x) > 0.5) {
                block_uv = fract(var_pos.zy + 0.5);
            } else if (abs(var_normal.y) > 0.5) {
                block_uv = fract(var_pos.xz + 0.5);
            } else {
                block_uv = fract(var_pos.xy + 0.5);
            }
            
            float frame = floor(break_info.x);
            float total_frames = max(1.0, break_info.z);
            
            // Map the 0..1 block_uv to the correct frame in the spritesheet
            vec2 break_uv = vec2((block_uv.x + frame) / total_frames, 1.0 - block_uv.y);
            vec4 break_color = texture(texture1, break_uv);
            
            // Apply overlay
            color.rgb = mix(color.rgb, color.rgb * break_color.rgb, break_color.a);
        }
    }
    
    // Light calculation
    // Torch color is yellowish: vmath.vector3(1.0, 0.9, 0.6)
    vec3 torch_color = vec3(1.0, 0.9, 0.6) * var_torch_light * 1.5; // Slightly boosted
    
    // Sun color is slightly warm white, affected by directional light `var_light`
    vec3 sun_color = vec3(1.0, 1.0, 1.0) * var_sun_light * var_light;
    
    // Combine lighting: (Sun + Torch) * AO
    // max prevents going completely black, though sun_light/torch_light handle darkness.
    vec3 final_light = max(vec3(0.02), sun_color + torch_color) * var_ao;
    color.rgb *= final_light;

    // Apply Fog (Per-fragment)
    float dist = length(var_view_pos);
    float fog_factor = clamp((fog_params.y - dist) / (fog_params.y - fog_params.x), 0.0, 1.0);
    color.rgb = mix(fog_color.rgb, color.rgb, fog_factor);

    // Output final color (force alpha to 1.0 for solid blocks)
    out_fragColor = vec4(color.rgb, 1.0);
}
