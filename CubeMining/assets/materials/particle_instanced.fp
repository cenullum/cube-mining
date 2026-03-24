#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_color;
in mediump vec4 var_atlas_bounds;

out vec4 out_fragColor;

uniform mediump sampler2D texture0;
uniform fs_uniforms {
    mediump vec4 light_tint;
};

void main()
{
    vec2 atlas_uv = var_texcoord0 * var_atlas_bounds.zw + var_atlas_bounds.xy;
    vec4 tex_color = texture(texture0, atlas_uv);

    if (tex_color.a < 0.1) {
        discard;
    }

    out_fragColor = tex_color * var_color * vec4(light_tint.rgb, 1.0);
}
