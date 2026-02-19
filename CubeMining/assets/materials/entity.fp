#version 140

in mediump vec2 var_texcoord0;
in mediump float var_light;
in mediump float var_fog_factor;

out vec4 out_fragColor;

uniform mediump sampler2D tex0;

uniform fs_uniforms
{
    uniform mediump vec4 tint;
    uniform mediump vec4 fog_color;
};

void main()
{
    vec4 color = texture(tex0, var_texcoord0) * tint;
    if (color.a < 0.1) discard;
    
    vec3 final_rgb = color.rgb * var_light;
    final_rgb = mix(fog_color.rgb, final_rgb, var_fog_factor);
    
    out_fragColor = vec4(final_rgb, color.a);
}
