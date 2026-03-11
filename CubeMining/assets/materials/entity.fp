#version 140

in mediump vec2 var_texcoord0;
in mediump float var_light;
in mediump vec3 var_view_pos;
in lowp float var_fog_factor;

out vec4 out_fragColor;

uniform mediump sampler2D tex0;

uniform fs_uniforms
{
    uniform mediump vec4 tint;
    uniform lowp vec4 fog_color;
    uniform mediump vec4 fog_params;
    uniform mediump vec4 cam_pos;
};

void main()
{
    vec4 color = texture(tex0, var_texcoord0);
    if (color.a < 0.1) discard;
    
    // var_light contains diffuse directional light. 
    // tint contains environmental ambient light (overridden by damage/death in script).
    vec3 final_rgb = color.rgb * var_light * tint.xyz;
    
    final_rgb = mix(fog_color.rgb, final_rgb, var_fog_factor);
    
    out_fragColor = vec4(final_rgb, color.a);
}
