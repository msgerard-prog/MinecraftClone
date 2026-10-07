#pragma once

#include "rendering/BlockModels.h"
#include "rendering/Camera.h"
#include "rendering/ChunkRenderer.h"
#include "rendering/MeshWorkers.h"
#include "rendering/PackedVertex.h"
#include "rendering/Shader.h"
#include "rendering/TextureAtlas.h"
#include "world/SectionSnapshot.h"
#include "world/World.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mc::gfx {

// Owns the GL state for drawing the world: block shader, atlas, baked models and the
// chunk renderer. main.cpp drives it; all GL calls for a frame happen in here
// (hard rule 7).
class WorldRenderer {
public:
    // Load time: GL state, shaders, atlas, models. Call after initOpenGl().
    // Textures come from our built-in assets, overridden by every pack found in
    // `resourcePacksDir` (sorted by name, later wins).
    bool init(const std::string& resourcePacksDir);

    // Once per game tick: texture animations.
    void tick() { m_atlas.tick(); }

    // Queue sections for (re)meshing. Marking a chunk also marks the sections of its
    // four neighbours' facing borders, whose face culling depends on it.
    void markChunkDirty(const world::World& world, world::ChunkPos pos);
    void markAllDirty(const world::World& world);

    // Streaming: a chunk is meshed once it and all 8 neighbours are loaded (its border
    // faces depend on them), so the edge of the loaded area never shows walls.
    void onChunksLoaded(const world::World& world, const std::vector<world::ChunkPos>& loaded);
    void onChunksUnloaded(const std::vector<world::ChunkPos>& unloaded);

    // Uploads finished meshes, then snapshots dirty sections for the mesh workers,
    // nearest to `cameraPos` first, within a per-frame time budget and a cap on jobs
    // in flight. Call once per frame (main thread).
    void update(const world::World& world, const glm::dvec3& cameraPos);

    // Sections waiting to be meshed or uploaded (queued + in flight).
    int pendingMeshes() const { return m_inFlight + static_cast<int>(m_dirtyList.size()); }

    // Clears to the sky colour and draws the world from `camera`.
    void drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight);

    // Fog reaches the sky colour at the edge of the render distance (in chunks).
    void setRenderDistance(int chunks) { m_renderDistance = chunks; }

    const ChunkRenderer::Stats& stats() const { return m_chunks.stats(); }
    const ChunkRenderer::Stats& translucentStats() const { return m_translucent.stats(); }

private:
    Shader m_blockShader;
    TextureAtlas m_atlas;
    BlockModels m_models;
    ChunkRenderer m_chunks;      // opaque pass
    ChunkRenderer m_translucent; // blended pass (water...), drawn back to front
    int m_renderDistance = 12;
    // Per-section scheduling state. Entries are erased once a section has no mesh
    // work pending. (M3 replaces this map with a dense grid around the camera.)
    struct SectionState {
        uint32_t version = 0;  // latest submitted job; older results are stale
        uint16_t inFlight = 0; // jobs submitted, not yet returned
        bool dirty = false;    // in m_dirtyList
    };
    void markDirty(world::SectionPos pos);
    void markChunkSections(world::ChunkPos pos);
    void eraseIfIdle(world::SectionPos pos);

    std::unique_ptr<MeshWorkers> m_workers;
    std::unordered_set<world::ChunkPos> m_meshedChunks; // streaming: chunks given meshes
    std::unordered_map<world::SectionPos, SectionState> m_states;
    std::vector<world::SectionPos> m_dirtyList; // sorted far -> near before dispatch
    bool m_dirtyUnsorted = false;
    int m_inFlight = 0;
    int m_maxInFlight = 0;
};

} // namespace mc::gfx
