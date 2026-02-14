#version 140

in mediump vec2 var_texcoord0;
in mediump float var_light;

out vec4 out_fragColor;

uniform mediump sampler2D tex0;

uniform fs_uniforms
{
    uniform mediump vec4 tint;
};

void main()
{
    vec4 color = texture(tex0, var_texcoord0) * tint;
    if (color.a < 0.1) discard;
    
    out_fragColor = vec4(color.rgb * var_light, color.a);
}
