#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/mutex.h>

const int CHUNK_SIZE = 16;
const int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct Chunk {
    int cx, cy, cz;
    uint8_t blocks[CHUNK_VOLUME];
    uint8_t sun_light[CHUNK_VOLUME];
    uint8_t source_light[CHUNK_VOLUME];

    bool is_empty;
    bool is_dirty;
    bool light_dirty;
    bool is_generated;
    bool is_spawned;

    // Allocated slot in the global light texture atlas
    int light_tex_u, light_tex_v;

    // Temporary storage for built meshes before Lua polls
    float* opaque_pos, *opaque_uvb, *opaque_uvl, *opaque_face;
    uint32_t opaque_verts, opaque_faces, opaque_quads;

    float* trans_pos, *trans_uvb, *trans_uvl, *trans_face;
    uint32_t trans_verts, trans_faces, trans_quads;

    double build_time;

    Chunk(int x, int y, int z);
    ~Chunk();
    void FreeMeshBuffers();
    void AllocateMeshBuffers(uint32_t max_verts);
};

enum class ChunkUpdateType {
    SPAWN,
    DESPAWN,
    MESH
};

struct LuaUpdate {
    ChunkUpdateType type;
    int cx, cy, cz;
    Chunk* chunk_ptr; 
};

class ChunkManager {
public:
    static void Init();
    static void Shutdown();
    static void Update(float player_x, float player_y, float player_z);
    
    // Chunk caching
    static uint64_t GetChunkKey(int cx, int cy, int cz);
    static Chunk* GetChunk(int cx, int cy, int cz);
    static Chunk* GetOrCreateChunk(int cx, int cy, int cz, bool spawn_in_lua = true);
    static void RemoveChunk(int cx, int cy, int cz);
    static void GenerateTerrain(Chunk* chunk);

    // Coordinate mapping
    static void WorldToChunk(int x, int y, int z, int& cx, int& cy, int& cz, int& lx, int& ly, int& lz);
    static uint8_t GetBlock(int x, int y, int z);
    static void SetBlock(int x, int y, int z, uint8_t id);
    static bool IsSolid(int x, int y, int z);
    static uint8_t GetSunlight(int x, int y, int z);
    static void SetSunlight(int x, int y, int z, uint8_t val);
    static uint8_t GetSourceLight(int x, int y, int z);
    static void SetSourceLight(int x, int y, int z, uint8_t val);

    // Queue processing (called by worker thread)
    static void ProcessQueues();

    // Stats
    static uint32_t GetTotalFaces();
    static uint32_t GetTotalVertices();

    // Lua Polling integration
    static bool PollUpdate(LuaUpdate& out_update);
    static void PushLuaUpdate(ChunkUpdateType type, int cx, int cy, int cz, Chunk* chunk);
    
    // Thread safe operations
    static void QueueMeshRebuild(Chunk* chunk);
    static void QueueLightingUpdate(Chunk* chunk);
    
    static dmMutex::HMutex mutex;
    static std::map<uint64_t, Chunk*> active_chunks;

    // Render parameters
    static int view_distance;

private:
    static std::vector<Chunk*> lighting_queue;
    static std::vector<Chunk*> mesh_queue;
    static std::vector<LuaUpdate> lua_pending_updates;

    static uint32_t stats_faces;
    static uint32_t stats_vertices;

    static void AllocateLightTextureSlot(Chunk* chunk);
    static void FreeLightTextureSlot(Chunk* chunk);
    
    static std::map<uint64_t, bool> light_slots; // simple tracker for light slots
};

int Lua_PollChunkUpdates(lua_State* L);

