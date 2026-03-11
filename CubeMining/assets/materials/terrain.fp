#version 140

// --- Inputs from Vertex Shader ---
in mediump vec2 var_texcoord0;      // Tiling coordinate (legacy/unused for greedy)
in mediump vec4 var_atlas_metadata; // x,y = base_uv, z,w = unit_size
in mediump float var_light;         // Directional Lighting factor
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
uniform mediump sampler2D texture2; // The chunk light map

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

vec4 sample_light(vec3 pc) {
    // Clamp to chunk boundaries
    pc = clamp(pc, vec3(0.0), vec3(15.0));
    // Calculate 256x16 texture coordinates (u = x + z*16, v = y)
    float u = (floor(pc.x) + 0.5 + floor(pc.z) * 16.0) / 256.0;
    float v = (floor(pc.y) + 0.5) / 16.0;
    return texture(texture2, vec2(u, v));
}

vec4 bilinear_light(vec3 pos, vec3 normal) {
    vec3 i = floor(pos);
    vec3 f = fract(pos);
    
    if (abs(normal.x) > 0.5) {
        // YZ plane interpolation
        vec4 c00 = sample_light(i + vec3(0.0, 0.0, 0.0));
        vec4 c10 = sample_light(i + vec3(0.0, 1.0, 0.0));
        vec4 c01 = sample_light(i + vec3(0.0, 0.0, 1.0));
        vec4 c11 = sample_light(i + vec3(0.0, 1.0, 1.0));
        return mix(mix(c00, c10, f.y), mix(c01, c11, f.y), f.z);
    } else if (abs(normal.y) > 0.5) {
        // XZ plane interpolation
        vec4 c00 = sample_light(i + vec3(0.0, 0.0, 0.0));
        vec4 c10 = sample_light(i + vec3(1.0, 0.0, 0.0));
        vec4 c01 = sample_light(i + vec3(0.0, 0.0, 1.0));
        vec4 c11 = sample_light(i + vec3(1.0, 0.0, 1.0));
        return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.z);
    } else {
        // XY plane interpolation
        vec4 c00 = sample_light(i + vec3(0.0, 0.0, 0.0));
        vec4 c10 = sample_light(i + vec3(1.0, 0.0, 0.0));
        vec4 c01 = sample_light(i + vec3(0.0, 1.0, 0.0));
        vec4 c11 = sample_light(i + vec3(1.0, 1.0, 0.0));
        return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
    }
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
    vec3 sample_pos = var_pos + var_normal * 0.5;
    vec4 light_data = bilinear_light(sample_pos, var_normal);
    
    // light_data.rgb contains our combined sun and torch
    // light_data.a contains our Ambient Occlusion factor (0.0 = solid shadow, 1.0 = open air)
    float ao = mix(0.2, 1.0, light_data.a); // Map AO curve purely from alpha

    // Combine intensities footprint
    vec3 final_light = clamp(light_data.rgb, 0.02, 1.5) * var_light * ao;
    color.rgb *= final_light;

    // Apply Fog (Per-fragment)
    lowp float dist = length(var_view_pos);
    lowp float fog_factor = clamp((fog_params.y - dist) / (fog_params.y - fog_params.x), 0.0, 1.0);
    color.rgb = mix(fog_color.rgb, color.rgb, fog_factor);

    // Output final color (force alpha to 1.0 for solid blocks)
    out_fragColor = vec4(color.rgb, 1.0);
}
