#version 140

// --- Inputs from Vertex Shader ---
in mediump vec2 var_texcoord0;     // Tiling coordinate (scaled by quad size)
in mediump vec4 var_atlas_metadata; // Atlas bounds
in mediump float var_light;        // Directional Lighting factor
in mediump float var_ao;           // Ambient Occlusion factor
in mediump float var_torch_light;
in mediump float var_sun_light;
in mediump vec3 var_view_pos;       // View-space position

// --- Fragment Output ---
out vec4 out_fragColor;

// --- Samplers ---
uniform mediump sampler2D texture0; // The block texture atlas (nearest filtered)

// --- Uniforms ---
uniform fs_uniforms
{
    mediump vec4 tint; // Face tint color (default white)
    mediump vec4 fog_color;
    mediump vec4 fog_params;
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
