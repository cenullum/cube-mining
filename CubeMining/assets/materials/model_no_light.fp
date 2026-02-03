#version 140

in highp vec4 var_position;
in mediump vec2 var_texcoord0;
in mediump vec4 var_atlas_offset;

out vec4 out_fragColor;

uniform mediump sampler2D tex0;

uniform fs_uniforms
{
    mediump vec4 tint;
};

void main()
{
    // Pre-multiply alpha since all runtime textures already are
    vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);
    
    // UV Modulo logic for atlas textures:
    // var_texcoord0: ranges from 0 to (Count * Scale)
    // var_atlas_offset.xy: tile offset in atlas (min_u, min_v)
    // var_atlas_offset.zw: tile size in atlas (width, height)
    vec2 mod_uv = mod(var_texcoord0, var_atlas_offset.zw) + var_atlas_offset.xy;
    
    vec4 color = texture(tex0, mod_uv) * tint_pm;

    out_fragColor = vec4(color.rgb, 1.0);
}
