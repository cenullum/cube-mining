import re

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'r') as f:
    content = f.read()

# Remove AO and Light buffers
content = re.sub(r'static uint8_t\* g_vertex_ao.*?\n', '', content)
content = re.sub(r'static uint8_t\* g_vertex_light_torch.*?\n', '', content)
content = re.sub(r'static uint8_t\* g_vertex_light_sun.*?\n', '', content)

# Remove AMBIENT_OCCLUSION_LEVELS and NORMALIZED_LIGHT_STEP
content = re.sub(r'static const float AMBIENT_OCCLUSION_LEVELS.*?;\n', '', content)
content = re.sub(r'static const float NORMALIZED_LIGHT_STEP.*?;\n', '', content)

# alloc_mesh_buffers
content = re.sub(r'g_vertex_ao\s*=\s*\(uint8_t\*\)realloc\(g_vertex_ao,.*?\);\n', '', content)
content = re.sub(r'g_vertex_light_torch\s*=\s*\(uint8_t\*\)realloc\(g_vertex_light_torch,.*?\);\n', '', content)
content = re.sub(r'g_vertex_light_sun\s*=\s*\(uint8_t\*\)realloc\(g_vertex_light_sun,.*?\);\n', '', content)

# append_quad_to_mesh_buffers signature
content = re.sub(r'int ao_levels\[4\], float vertex_sun_light\[4\], float vertex_source_light\[4\], ', '', content)

# append_quad_to_mesh_buffers body
content = re.sub(r'// Normalized light.*?torch_b\[i\] = \(uint8_t\)\(vertex_source_light\[i\] \* 255.0f\);\n    }\n', '', content, flags=re.DOTALL)
content = re.sub(r'g_vertex_ao\s*\[.*?\] = ao_b\[j\];\n', '', content)
content = re.sub(r'g_vertex_light_torch\[.*?\] = torch_b\[j\];\n', '', content)
content = re.sub(r'g_vertex_light_sun\s*\[.*?\] = sun_b\[j\];\n', '', content)

# Shutdown memory freeing
content = re.sub(r'free\(g_vertex_ao\);\s*g_vertex_ao = 0;\n', '', content)
content = re.sub(r'free\(g_vertex_light_torch\);\s*g_vertex_light_torch = 0;\n', '', content)
content = re.sub(r'free\(g_vertex_light_sun\);\s*g_vertex_light_sun = 0;\n', '', content)

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'w') as f:
    f.write(content)
