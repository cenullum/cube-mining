#include "chunk_manager.h"
#include "ve_world.h"
#include "noise.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

dmMutex::HMutex ChunkManager::mutex = 0;
std::map<uint64_t, Chunk *> ChunkManager::active_chunks;
std::vector<Chunk *> ChunkManager::lighting_queue;
std::vector<Chunk *> ChunkManager::mesh_queue;
std::vector<LuaUpdate> ChunkManager::lua_pending_updates;
uint32_t ChunkManager::stats_faces = 0;
uint32_t ChunkManager::stats_vertices = 0;
int ChunkManager::view_distance = 2; // Default 2 chunk distance
std::map<uint64_t, bool> ChunkManager::light_slots;

Chunk::Chunk(int x, int y, int z) : cx(x), cy(y), cz(z) {
  memset(blocks, 0, CHUNK_VOLUME);
  memset(sun_light, 0, CHUNK_VOLUME);
  memset(source_light, 0, CHUNK_VOLUME);
  is_empty = true;
  is_dirty = false;
  light_dirty = false;
  is_generated = false;
  light_tex_u = -1;
  light_tex_v = -1;

  opaque_pos = opaque_uvb = opaque_uvl = opaque_face = nullptr;
  opaque_verts = opaque_faces = opaque_quads = 0;

  trans_pos = trans_uvb = trans_uvl = trans_face = nullptr;
  trans_verts = trans_faces = trans_quads = 0;
  build_time = 0.0;
}

Chunk::~Chunk() { FreeMeshBuffers(); }

void Chunk::FreeMeshBuffers() {
  if (opaque_pos) {
    free(opaque_pos);
    opaque_pos = nullptr;
  }
  if (opaque_uvb) {
    free(opaque_uvb);
    opaque_uvb = nullptr;
  }
  if (opaque_uvl) {
    free(opaque_uvl);
    opaque_uvl = nullptr;
  }
  if (opaque_face) {
    free(opaque_face);
    opaque_face = nullptr;
  }

  if (trans_pos) {
    free(trans_pos);
    trans_pos = nullptr;
  }
  if (trans_uvb) {
    free(trans_uvb);
    trans_uvb = nullptr;
  }
  if (trans_uvl) {
    free(trans_uvl);
    trans_uvl = nullptr;
  }
  if (trans_face) {
    free(trans_face);
    trans_face = nullptr;
  }
}

void Chunk::AllocateMeshBuffers(uint32_t max_verts) {
  if (!opaque_pos) {
    opaque_pos = (float *)malloc(max_verts * 3 * sizeof(float));
    opaque_uvb = (float *)malloc(max_verts * 4 * sizeof(float));
    opaque_uvl = (float *)malloc(max_verts * 4 * sizeof(float));
    opaque_face = (float *)malloc(max_verts * 2 * sizeof(float));

    trans_pos = (float *)malloc(max_verts * 3 * sizeof(float));
    trans_uvb = (float *)malloc(max_verts * 4 * sizeof(float));
    trans_uvl = (float *)malloc(max_verts * 4 * sizeof(float));
    trans_face = (float *)malloc(max_verts * 2 * sizeof(float));
  }
}

void ChunkManager::Init() { mutex = dmMutex::New(); }

void ChunkManager::Shutdown() {
  if (mutex) {
    dmMutex::Delete(mutex);
    mutex = 0;
  }
  for (auto &pair : active_chunks) {
    delete pair.second;
  }
  active_chunks.clear();
  lighting_queue.clear();
  mesh_queue.clear();
  lua_pending_updates.clear();
  light_slots.clear();
}

uint64_t ChunkManager::GetChunkKey(int cx, int cy, int cz) {
  return ((uint64_t)(cx & 0x3FFFFF) << 42) | ((uint64_t)(cy & 0xFFFFF) << 22) |
         ((uint64_t)(cz & 0x3FFFFF));
}

Chunk *ChunkManager::GetChunk(int cx, int cy, int cz) {
  dmMutex::Lock(mutex);
  uint64_t key = GetChunkKey(cx, cy, cz);
  auto it = active_chunks.find(key);
  Chunk *c = (it != active_chunks.end()) ? it->second : nullptr;
  dmMutex::Unlock(mutex);
  return c;
}

void ChunkManager::AllocateLightTextureSlot(Chunk *chunk) {
  if (chunk->is_empty)
    return;
  if (chunk->light_tex_u != -1 && chunk->light_tex_v != -1)
    return;

  int max_slots = 113 * 6; // 2048 / 18 = 113, 2048 / 324 = 6 -> 678 chunks
  for (int i = 0; i < max_slots; i++) {
    uint64_t slot_id = i;
    if (light_slots.find(slot_id) == light_slots.end() ||
        !light_slots[slot_id]) {
      light_slots[slot_id] = true;
      chunk->light_tex_u = (i % 113) * 18;
      chunk->light_tex_v = (i / 113) * 324;
      return;
    }
  }
  chunk->light_tex_u = -1;
  chunk->light_tex_v = -1;
}

void ChunkManager::FreeLightTextureSlot(Chunk *chunk) {
  if (chunk->light_tex_u == -1 || chunk->light_tex_v == -1)
    return;
  int index = (chunk->light_tex_u / 18) + (chunk->light_tex_v / 324) * 113;
  light_slots[index] = false;
  chunk->light_tex_u = -1;
  chunk->light_tex_v = -1;
}

Chunk *ChunkManager::CreateChunk(int cx, int cy, int cz) {
  Chunk *chunk = new Chunk(cx, cy, cz);
  AllocateLightTextureSlot(chunk);

  dmMutex::Lock(mutex);
  active_chunks[GetChunkKey(cx, cy, cz)] = chunk;
  dmMutex::Unlock(mutex);

  GenerateTerrain(chunk);

  // Notify Lua to spawn GameObject
  PushLuaUpdate(ChunkUpdateType::SPAWN, cx, cy, cz, chunk);

  // Immediately queue for lighting and mesh
  QueueLightingUpdate(chunk);

  dmLogInfo("voxel_engine: Created Chunk(%d, %d, %d)", cx, cy, cz);
  return chunk;
}

void ChunkManager::RemoveChunk(int cx, int cy, int cz) {
  dmMutex::Lock(mutex);
  uint64_t key = GetChunkKey(cx, cy, cz);
  auto it = active_chunks.find(key);
  if (it != active_chunks.end()) {
    Chunk *chunk = it->second;

    // Remove from queues
    lighting_queue.erase(
        std::remove(lighting_queue.begin(), lighting_queue.end(), chunk),
        lighting_queue.end());
    mesh_queue.erase(std::remove(mesh_queue.begin(), mesh_queue.end(), chunk),
                     mesh_queue.end());

    PushLuaUpdate(ChunkUpdateType::DESPAWN, cx, cy, cz, nullptr);

    FreeLightTextureSlot(chunk);

    stats_faces -= (chunk->opaque_faces + chunk->trans_faces);
    stats_vertices -= (chunk->opaque_verts + chunk->trans_verts);

    delete chunk;
    active_chunks.erase(it);
    dmLogInfo("voxel_engine: Removed Chunk(%d, %d, %d)", cx, cy, cz);
  }
  dmMutex::Unlock(mutex);
}

void ChunkManager::Update(float player_x, float player_y, float player_z) {
  int center_cx = (int)floorf(player_x / CHUNK_SIZE);
  int center_cy = (int)floorf(player_y / CHUNK_SIZE);
  int center_cz = (int)floorf(player_z / CHUNK_SIZE);

  // 1. Generate missing chunks within view distance
  // From center outwards
  std::vector<std::pair<int, dmVMath::Vector3>> spawn_candidates;
  for (int x = -view_distance; x <= view_distance; x++) {
    for (int y = -view_distance; y <= view_distance; y++) {
      for (int z = -view_distance; z <= view_distance; z++) {
        int cx = center_cx + x;
        int cy = center_cy + y;
        int cz = center_cz + z;
        int dist_sq = x * x + y * y + z * z;
        if (dist_sq <= view_distance * view_distance * 3) {
          if (!GetChunk(cx, cy, cz)) {
            spawn_candidates.push_back({dist_sq, dmVMath::Vector3(cx, cy, cz)});
          }
        }
      }
    }
  }

  // Sort by distance
  std::sort(spawn_candidates.begin(), spawn_candidates.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  // Limit creations per frame to avoid freezing (handled mostly by threads, but
  // good to pace)
  int creations_this_frame = 0;
  for (auto &cand : spawn_candidates) {
    if (creations_this_frame > 2)
      break; // Throttle
    CreateChunk((int)cand.second.getX(), (int)cand.second.getY(),
                (int)cand.second.getZ());
    creations_this_frame++;
  }

  // 2. Remove out-of-range chunks
  std::vector<dmVMath::Vector3> to_remove;
  int removal_dist = view_distance + 2;

  dmMutex::Lock(mutex);
  for (auto &pair : active_chunks) {
    Chunk *c = pair.second;
    int dx = abs(c->cx - center_cx);
    int dy = abs(c->cy - center_cy);
    int dz = abs(c->cz - center_cz);

    if (dx > removal_dist || dy > removal_dist || dz > removal_dist) {
      to_remove.push_back(dmVMath::Vector3(c->cx, c->cy, c->cz));
    }
  }
  dmMutex::Unlock(mutex);
  for (auto &p : to_remove) {
    RemoveChunk((int)p.getX(), (int)p.getY(), (int)p.getZ());
  }
}

void ChunkManager::GenerateTerrain(Chunk *chunk) {
  chunk->is_empty = true;
  int water_level = 30;
  for (int x = 0; x < CHUNK_SIZE; x++) {
    for (int z = 0; z < CHUNK_SIZE; z++) {
      int world_x = chunk->cx * CHUNK_SIZE + x;
      int world_z = chunk->cz * CHUNK_SIZE + z;
      int ground_y = CalculateGroundHeight(world_x, world_z, g_seed);

      for (int y = 0; y < CHUNK_SIZE; y++) {
        int world_y = chunk->cy * CHUNK_SIZE + y;
        uint8_t block_id = 0;

        if (world_y == 0) {
          block_id = 2; // Bedrock
        } else if (world_y < ground_y - 3) {
          if ((rand() % 100) < 5) block_id = 3; // Gold
          else block_id = 1; // Stone
        } else if (world_y < ground_y) {
          if (ground_y <= water_level + 2) block_id = 7; // Sand near water
          else block_id = 6; // Dirt
        } else if (world_y == ground_y) {
          if (ground_y <= water_level + 2) block_id = 7; // Sand near water
          else block_id = 5; // Grass
        } else if (world_y <= water_level) {
          block_id = 8; // Water
        }

        if (block_id != 0) {
          chunk->blocks[x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE] =
              block_id;
          chunk->is_empty = false;
        }
      }
    }
  }
  chunk->is_generated = true;
  if (!chunk->is_empty) {
    AllocateLightTextureSlot(chunk);
  }
}

void ChunkManager::WorldToChunk(int x, int y, int z, int &cx, int &cy, int &cz,
                                int &lx, int &ly, int &lz) {
  cx = (int)floorf((float)x / CHUNK_SIZE);
  cy = (int)floorf((float)y / CHUNK_SIZE);
  cz = (int)floorf((float)z / CHUNK_SIZE);
  lx = x - (cx * CHUNK_SIZE);
  ly = y - (cy * CHUNK_SIZE);
  lz = z - (cz * CHUNK_SIZE);
}

uint8_t ChunkManager::GetBlock(int x, int y, int z) {
  int cx, cy, cz, lx, ly, lz;
  WorldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
  Chunk *chunk = GetChunk(cx, cy, cz);
  if (!chunk)
    return 0; // Air if unloaded
  return chunk->blocks[lx + lz * CHUNK_SIZE + ly * CHUNK_SIZE * CHUNK_SIZE];
}

void LightingUpdateBlockChanged(int wx, int wy, int wz, uint8_t old_id,
                                uint8_t new_id); // from ve_lighting

void ChunkManager::SetBlock(int x, int y, int z, uint8_t id) {
  int cx, cy, cz, lx, ly, lz;
  WorldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
  Chunk *chunk = GetChunk(cx, cy, cz);
  if (!chunk)
    return;

  uint8_t old_id =
      chunk->blocks[lx + lz * CHUNK_SIZE + ly * CHUNK_SIZE * CHUNK_SIZE];
  if (old_id == id)
    return;

  chunk->blocks[lx + lz * CHUNK_SIZE + ly * CHUNK_SIZE * CHUNK_SIZE] = id;
  if (id != 0) {
    if (chunk->is_empty) {
      chunk->is_empty = false;
      AllocateLightTextureSlot(chunk);
    }
  }

  // Mesh Rebuild ONLY for visual chunks touched
  QueueMeshRebuild(chunk);
  if (lx == 0) {
    Chunk *n = GetChunk(cx - 1, cy, cz);
    if (n)
      QueueMeshRebuild(n);
  }
  if (lx == CHUNK_SIZE - 1) {
    Chunk *n = GetChunk(cx + 1, cy, cz);
    if (n)
      QueueMeshRebuild(n);
  }
  if (ly == 0) {
    Chunk *n = GetChunk(cx, cy - 1, cz);
    if (n)
      QueueMeshRebuild(n);
  }
  if (ly == CHUNK_SIZE - 1) {
    Chunk *n = GetChunk(cx, cy + 1, cz);
    if (n)
      QueueMeshRebuild(n);
  }
  if (lz == 0) {
    Chunk *n = GetChunk(cx, cy, cz - 1);
    if (n)
      QueueMeshRebuild(n);
  }
  if (lz == CHUNK_SIZE - 1) {
    Chunk *n = GetChunk(cx, cy, cz + 1);
    if (n)
      QueueMeshRebuild(n);
  }

  LightingUpdateBlockChanged(x, y, z, old_id, id);
}

bool ChunkManager::IsSolid(int x, int y, int z) {
  uint8_t b = GetBlock(x, y, z);
  return b != 0 && g_block_defs[b].render_type != 1;
}

uint8_t ChunkManager::GetSunlight(int x, int y, int z) {
  int cx, cy, cz, lx, ly, lz;
  WorldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
  Chunk *chunk = GetChunk(cx, cy, cz);
  if (!chunk)
    return 15; // Assume light passes through unloaded
  return chunk->sun_light[lx + lz * CHUNK_SIZE + ly * CHUNK_SIZE * CHUNK_SIZE];
}

uint8_t ChunkManager::GetSourceLight(int x, int y, int z) {
  int cx, cy, cz, lx, ly, lz;
  WorldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
  Chunk *chunk = GetChunk(cx, cy, cz);
  if (!chunk)
    return 0;
  return chunk
      ->source_light[lx + lz * CHUNK_SIZE + ly * CHUNK_SIZE * CHUNK_SIZE];
}

void ChunkManager::SetSunlight(int x, int y, int z, uint8_t val) {
  int cx, cy, cz, lx, ly, lz;
  WorldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
  Chunk *chunk = GetChunk(cx, cy, cz);
  if (chunk)
    chunk->sun_light[lx + lz * CHUNK_SIZE + ly * CHUNK_SIZE * CHUNK_SIZE] = val;
}

void ChunkManager::SetSourceLight(int x, int y, int z, uint8_t val) {
  int cx, cy, cz, lx, ly, lz;
  WorldToChunk(x, y, z, cx, cy, cz, lx, ly, lz);
  Chunk *chunk = GetChunk(cx, cy, cz);
  if (chunk)
    chunk->source_light[lx + lz * CHUNK_SIZE + ly * CHUNK_SIZE * CHUNK_SIZE] =
        val;
}

void ChunkManager::QueueMeshRebuild(Chunk *chunk) {
  dmMutex::Lock(mutex);
  if (std::find(mesh_queue.begin(), mesh_queue.end(), chunk) ==
      mesh_queue.end()) {
    mesh_queue.push_back(chunk);
  }
  dmMutex::Unlock(mutex);
}

void ChunkManager::QueueLightingUpdate(Chunk *chunk) {
  dmMutex::Lock(mutex);
  if (std::find(lighting_queue.begin(), lighting_queue.end(), chunk) ==
      lighting_queue.end()) {
    lighting_queue.push_back(chunk);
  }
  // Lighting updates also trigger a mesh update, but to ensure accuracy,
  // we trigger mesh updates on neighbors too since lighting bleeds.
  dmMutex::Unlock(mutex);
}

void ChunkManager::ProcessQueues() {
  std::vector<Chunk *> to_light;
  dmMutex::Lock(mutex);
  to_light = lighting_queue;
  lighting_queue.clear();
  dmMutex::Unlock(mutex);

  for (Chunk *c : to_light) {
    perform_lighting_pass(c);
    QueueMeshRebuild(c);
  }

  std::vector<Chunk *> to_mesh;
  dmMutex::Lock(mutex);
  to_mesh = mesh_queue;
  mesh_queue.clear();
  dmMutex::Unlock(mutex);

  for (Chunk *c : to_mesh) {
    execute_mesh_generation_pipeline(c);
    stats_faces += c->opaque_faces + c->trans_faces;
    stats_vertices += c->opaque_verts + c->trans_verts;
    PushLuaUpdate(ChunkUpdateType::MESH, c->cx, c->cy, c->cz, c);
  }
}

void ChunkManager::PushLuaUpdate(ChunkUpdateType type, int cx, int cy, int cz,
                                 Chunk *chunk) {
  dmMutex::Lock(mutex);
  lua_pending_updates.push_back({type, cx, cy, cz, chunk});
  dmMutex::Unlock(mutex);
}

bool ChunkManager::PollUpdate(LuaUpdate &out_update) {
  dmMutex::Lock(mutex);
  if (!lua_pending_updates.empty()) {
    out_update = lua_pending_updates.front();
    lua_pending_updates.erase(lua_pending_updates.begin());
    dmMutex::Unlock(mutex);
    return true;
  }
  dmMutex::Unlock(mutex);
  return false;
}
