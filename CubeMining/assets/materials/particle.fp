#version 140

in mediump vec2 var_texcoord0;     // locally [0..1] mapped over the particle quad
in mediump vec4 var_color;

out vec4 out_fragColor;

uniform mediump sampler2D texture0; // the terrain atlas

uniform fs_uniforms {
    mediump vec4 atlas_bounds; // x, y: start UV; z, w: UV width/height
};

void main()
{
    // The sprite renders by default with var_texcoord0 from [0, 1] on its base quad.
    // atlas_bounds (x,y=offset, z,w=size) allows sampling a specific sub-region of the atlas.
    // This is used for block fragments to show only a small part of the block texture.
    vec2 atlas_uv = var_texcoord0 * atlas_bounds.zw + atlas_bounds.xy;
    
    vec4 tex_color = texture(texture0, atlas_uv);
    
    // Pixel art style hard discard
    if (tex_color.a < 0.1) {
        discard;
    }
    
    out_fragColor = tex_color * var_color;
}
