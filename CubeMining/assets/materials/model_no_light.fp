#version 140

in mediump vec2 var_texcoord0;
out vec4 out_fragColor;

uniform fs_uniforms
{
    uniform mediump vec4 tint;
};

void main()
{
    // Simply output the tint color, ignoring texture and lighting
    out_fragColor = vec4(tint.xyz * tint.w, tint.w);
}
