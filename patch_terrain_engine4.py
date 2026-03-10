import re

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'r') as f:
    content = f.read()

# Replace streams_decl
old_streams = """    dmBuffer::StreamDeclaration streams_decl[] = {
        { dmHashString64("position"),  dmBuffer::VALUE_TYPE_FLOAT32, 3 },
        { dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 4 }, // base_u, base_v, unit_w, unit_h
        { dmHashString64("texcoord1"), dmBuffer::VALUE_TYPE_FLOAT32, 4 }, // local_u, local_v, quad_w, quad_h
        { dmHashString64("color"),     dmBuffer::VALUE_TYPE_UINT8,   4 }, // AO (ubyte4)
        { dmHashString64("color1"),    dmBuffer::VALUE_TYPE_UINT8,   4 }, // Torch (ubyte4)
        { dmHashString64("color2"),    dmBuffer::VALUE_TYPE_UINT8,   4 }, // Sun (ubyte4)
        { dmHashString64("texcoord2"), dmBuffer::VALUE_TYPE_FLOAT32, 1 }  // face_id
    };

    dmBuffer::HBuffer buffer_handle = 0;
    dmBuffer::Result res = dmBuffer::Create(v_count, streams_decl, 7, &buffer_handle);"""

new_streams = """    dmBuffer::StreamDeclaration streams_decl[] = {
        { dmHashString64("position"),  dmBuffer::VALUE_TYPE_FLOAT32, 3 },
        { dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 4 }, // base_u, base_v, unit_w, unit_h
        { dmHashString64("texcoord1"), dmBuffer::VALUE_TYPE_FLOAT32, 4 }, // local_u, local_v, quad_w, quad_h
        { dmHashString64("texcoord2"), dmBuffer::VALUE_TYPE_FLOAT32, 1 }  // face_id
    };

    dmBuffer::HBuffer buffer_handle = 0;
    dmBuffer::Result res = dmBuffer::Create(v_count, streams_decl, 4, &buffer_handle);"""

content = content.replace(old_streams, new_streams)

old_copies = """    copy_array_to_buffer_stream(buffer_handle, dmHashString64("position"),  g_vertex_positions,  v_count, 3);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord0"), g_vertex_uvs_base,   v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord1"), g_vertex_uvs_local,  v_count, 4);
    copy_byte_array_to_buffer_stream(buffer_handle, dmHashString64("color"), g_vertex_ao,        v_count, 4);
    copy_byte_array_to_buffer_stream(buffer_handle, dmHashString64("color1"),g_vertex_light_torch,v_count, 4);
    copy_byte_array_to_buffer_stream(buffer_handle, dmHashString64("color2"),g_vertex_light_sun,  v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord2"), g_vertex_face_ids,   v_count, 1);"""

new_copies = """    copy_array_to_buffer_stream(buffer_handle, dmHashString64("position"),  g_vertex_positions,  v_count, 3);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord0"), g_vertex_uvs_base,   v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord1"), g_vertex_uvs_local,  v_count, 4);
    copy_array_to_buffer_stream(buffer_handle, dmHashString64("texcoord2"), g_vertex_face_ids,   v_count, 1);"""

content = content.replace(old_copies, new_copies)

with open('CubeMining/terrain_engine/src/terrain_engine.cpp', 'w') as f:
    f.write(content)
