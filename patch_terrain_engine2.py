import re

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'r') as f:
    content = f.read()

# Delete AO helper functions
content = re.sub(
    r'// ============================================================\n// AO \+ Smooth Lighting helpers\n// ============================================================\n.*?// ============================================================\n// 5\. Mesh Generation\n// ============================================================',
    '// ============================================================\n// 5. Mesh Generation\n// ============================================================',
    content,
    flags=re.DOTALL
)

# Replace visibility mask logic
content = re.sub(
    r'int ao_levels\[4\] = \{0, 0, 0, 0\};\n\s*if \(ao_enabled\) calculate_ambient_occlusion_factors.*?1ULL << 48\);',
    'greedy_mask[v_idx * side_length + u_idx] = (uint64_t)current_id | (1ULL << 48);',
    content,
    flags=re.DOTALL
)

# Replace mask unpacking
content = re.sub(
    r'int packed_ao = \(mask_val >> 16\) & 0xFF;\n\s*int packed_light = \(mask_val >> 24\) & 0xFF;\n\s*int sun_lvl = packed_light & 0xF, torch_lvl = \(packed_light >> 4\) & 0xF;\n\s*int ao_levels\[4\] = \{.*?};\n',
    '',
    content,
    flags=re.DOTALL
)

# Replace smooth lighting interpolation logic
content = re.sub(
    r'float vertex_sun\[4\], vertex_torch\[4\];\n\s*if \(light_mode == 1\) \{ // Smooth.*?\} else \{ // Flat.*?\}\n\n\s*// Quad geometry:',
    '// Quad geometry:',
    content,
    flags=re.DOTALL
)

# Replace append call site
content = re.sub(
    r'append_quad_to_mesh_buffers\(current_quad_index, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, &g_block_defs\[block_id\]\.uvs\[face_direction\], width, height, nrm_x, nrm_y, nrm_z, ao_levels, vertex_sun, vertex_torch, face_direction\);',
    'append_quad_to_mesh_buffers(current_quad_index, p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z, &g_block_defs[block_id].uvs[face_direction], width, height, nrm_x, nrm_y, nrm_z, face_direction);',
    content,
    flags=re.DOTALL
)

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'w') as f:
    f.write(content)
