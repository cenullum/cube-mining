#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_atlas_metadata; // [u, v, w, h]
out vec4 out_fragColor;

uniform mediump sampler2D texture0;

uniform fs_uniforms
{
    mediump vec4 tint;
};

void main()
{
    // Clamp sampling within the atlas sub-region to avoid bleeding
    vec2 atlas_uv = mod(var_texcoord0, var_atlas_metadata.zw) + var_atlas_metadata.xy;
    vec4 color = texture(texture0, atlas_uv) * tint;
    
    if (color.a < 0.1) discard;
    out_fragColor = color;
}
