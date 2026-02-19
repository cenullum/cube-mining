#version 140

// --- Inputs from Vertex Shader ---
in mediump vec2 var_texcoord0;     // Tiling coordinate (scaled by quad size)
in mediump vec4 var_atlas_metadata; // Atlas bounds
in mediump float var_light;        // Lighting factor
in mediump float var_ao;           // Ambient Occlusion factor
in mediump float var_fog_factor;   // Fog factor

// --- Fragment Output ---
out vec4 out_fragColor;

// --- Samplers ---
uniform mediump sampler2D texture0; // The block texture atlas (nearest filtered)

// --- Uniforms ---
uniform fs_uniforms
{
    mediump vec4 tint; // Face tint color (default white)
    mediump vec4 fog_color;
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
    
    // Combine lighting: Diffuse/Ambient + AO
    color.rgb *= var_light * var_ao;

    // Apply Fog
    color.rgb = mix(fog_color.rgb, color.rgb, var_fog_factor);

    // Output final color (force alpha to 1.0 for solid blocks)
    out_fragColor = vec4(color.rgb, 1.0);
}
