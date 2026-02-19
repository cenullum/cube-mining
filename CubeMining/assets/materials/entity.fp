#version 140

in mediump vec2 var_texcoord0;
in mediump float var_light;
in mediump vec3 var_view_pos;

out vec4 out_fragColor;

uniform mediump sampler2D tex0;

uniform fs_uniforms
{
    uniform mediump vec4 tint;
    uniform mediump vec4 fog_color;
    uniform mediump vec4 fog_params;
};

void main()
{
    vec4 color = texture(tex0, var_texcoord0) * tint;
    if (color.a < 0.1) discard;
    
    vec3 final_rgb = color.rgb * var_light;
    
    float dist = length(var_view_pos);
    float fog_factor = clamp((fog_params.y - dist) / (fog_params.y - fog_params.x), 0.0, 1.0);
    final_rgb = mix(fog_color.rgb, final_rgb, fog_factor);
    
    out_fragColor = vec4(final_rgb, color.a);
}
