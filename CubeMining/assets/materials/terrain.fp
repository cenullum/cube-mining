#version 140

// --- Inputs from Vertex Shader ---
in mediump vec2 var_texcoord0;      // Tiling coordinate (legacy/unused for greedy)
in mediump vec4 var_atlas_metadata; // x,y = base_uv, z,w = unit_size
in mediump float var_light;         // Directional Lighting factor
in mediump vec4 var_corner_ao;      // AO for 4 corners [0..1]
in mediump vec4 var_corner_torch;   // Torch Light for 4 corners [0..1]
in mediump vec4 var_corner_sun;     // Sun Light for 4 corners [0..1]
in mediump vec2 var_local_uv;       // Local UV (0..1)
in mediump vec2 var_quad_size;      // Quad size in blocks
in mediump vec3 var_view_pos;       // View-space position
in mediump vec3 var_pos;            // Local position for block breaking
in mediump vec3 var_normal;         // Reconstructed normal

// --- Fragment Output ---
out vec4 out_fragColor;

// --- Samplers ---
uniform mediump sampler2D texture0; // The block texture atlas (nearest filtered)
uniform mediump sampler2D texture1; // The breaking texture spritesheet

// --- Uniforms ---
uniform fs_uniforms
{
    mediump vec4 tint; // Face tint color
    lowp vec4 fog_color;
    mediump vec4 fog_params;
    mediump vec4 break_info; // x: frame, y: enabled, z: total_frames
    mediump vec4 break_pos;  // xyz: block grid coordinates
    mediump vec4 cam_pos;
};

// Bilinear interpolation for a quad
float bilinear(vec4 corners, vec2 uv) {
    // corners: x=BL, y=BR, z=TR, w=TL (Matches C++ corners)
    float bottom = mix(corners.x, corners.y, uv.x);
    float top    = mix(corners.w, corners.z, uv.x);
    return mix(bottom, top, uv.y);
}

void main()
{
    vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);
    
    // Robust Greedy Tiling
    vec2 base_uv = var_atlas_metadata.xy;
    vec2 unit_size = var_atlas_metadata.zw;
    vec2 atlas_uv = base_uv + fract(var_local_uv * var_quad_size) * unit_size;
    
    vec4 color = texture(texture0, atlas_uv) * tint_pm;

    // Overlay Breaking Texture
    if (break_info.y > 0.5) {
        vec3 interior_pos = var_pos - var_normal * 0.05;
        vec3 grid_pos = floor(interior_pos + 0.5);
        if (all(lessThan(abs(grid_pos - break_pos.xyz), vec3(0.01)))) {
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
            vec2 b_uv = vec2((block_uv.x + frame) / total_frames, 1.0 - block_uv.y);
            vec4 break_color = texture(texture1, b_uv);
            color.rgb = mix(color.rgb, color.rgb * break_color.rgb, break_color.a);
        }
    }
    
    // Light calculation
    float ao = bilinear(var_corner_ao, var_local_uv);
    float torch_l = bilinear(var_corner_torch, var_local_uv);
    float sun_l  = bilinear(var_corner_sun, var_local_uv);

    // Combine intensities conservatively
    vec3 torch_color = vec3(1.0, 0.9, 0.6) * torch_l * 1.35;
    vec3 sun_color   = vec3(1.0, 1.0, 1.0) * sun_l   * var_light;
    
    // Use clamp to prevent bone-white explosion (max 1.5 for glow)
    vec3 final_light = clamp(sun_color + torch_color, 0.02, 1.5) * ao;
    color.rgb *= final_light;

    // Apply Fog (Per-fragment)
    lowp float dist = length(var_view_pos);
    lowp float fog_factor = clamp((fog_params.y - dist) / (fog_params.y - fog_params.x), 0.0, 1.0);
    color.rgb = mix(fog_color.rgb, color.rgb, fog_factor);

    // Output final color (force alpha to 1.0 for solid blocks)
    out_fragColor = vec4(color.rgb, 1.0);
}
