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
in mediump float var_block_id;      // Block ID

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
    mediump vec4 light_offset; // x,y: atlas start in pixels
};

vec4 sample_light(vec3 pc) {
    // pc ranges from roughly -0.5 to 16.5 at boundaries
    // We add 1.0 because the padded block 0 is at offset +1.
    float py = floor(pc.y + 1.0);
    float px = floor(pc.x + 1.0);
    float pz = floor(pc.z + 1.0);
    
    // Clamp to 0..17 just as a safeguard against precision artifacts
    px = clamp(px, 0.0, 17.0);
    py = clamp(py, 0.0, 17.0);
    pz = clamp(pz, 0.0, 17.0);
    
    float tex_x = light_offset.x + px;
    float tex_y = light_offset.y + py * 18.0 + pz;
    
    return texture(texture2, vec2((tex_x + 0.5) / 2048.0, (tex_y + 0.5) / 2048.0));
}

vec4 bilinear_light(vec3 pos, vec3 normal) {
    vec3 i = floor(pos);
    vec3 f = fract(pos);

    vec3 mask = 1.0 - abs(normal);

    vec3 d1 = vec3(mask.x, (1.0 - mask.x) * mask.y, 0.0);
    vec3 d2 = mask - d1;

    float u = dot(f, d1);
    float v = dot(f, d2);

    vec4 c00 = sample_light(i);
    vec4 c10 = sample_light(i + d1);
    vec4 c01 = sample_light(i + d2);
    vec4 c11 = sample_light(i + d1 + d2);

    return mix(mix(c00, c10, u), mix(c01, c11, u), v);
}

void main()
{
    vec2 atlas_uv = var_atlas_metadata.xy
                  + fract(var_local_uv * var_quad_size) * var_atlas_metadata.zw;

    vec4 color = texture(texture0, atlas_uv) * vec4(tint.xyz * tint.w, tint.w);

    if (break_info.y > 0.5) {
        vec3 grid_pos = round(var_pos - var_normal * 0.05);
        if (all(lessThan(abs(grid_pos - break_pos.xyz), vec3(0.01)))) {

            vec3 an     = abs(var_normal);
            float use_x = step(an.y, an.x) * step(an.z, an.x);
            float use_y = step(an.x, an.y) * step(an.z, an.y);

            vec2 block_uv = mix(
                mix(fract(var_pos.xy + 0.5),
                    fract(var_pos.xz + 0.5), use_y),
                    fract(var_pos.zy + 0.5), use_x);

            vec2 b_uv = vec2(
                (block_uv.x + floor(break_info.x)) / max(1.0, break_info.z),
                1.0 - block_uv.y);

            vec4 break_color = texture(texture1, b_uv);
            color.rgb = mix(color.rgb, color.rgb * break_color.rgb, break_color.a);
        }
    }


    vec4 light_data = bilinear_light(var_pos + var_normal * 0.5, var_normal);
    color.rgb *= clamp(light_data.rgb, 0.02, 1.5)
               * var_light
               * mix(0.2, 1.0, light_data.a);


    lowp float fog_factor = clamp(
        (fog_params.y - (-var_view_pos.z)) / (fog_params.y - fog_params.x),
        0.0, 1.0);
    color.rgb = mix(fog_color.rgb, color.rgb, fog_factor);
    
    if (var_block_id > 7.5 && var_block_id < 8.5) {
        color.a = 0.6;
    }

    out_fragColor = vec4(color.rgb, color.a);
}
