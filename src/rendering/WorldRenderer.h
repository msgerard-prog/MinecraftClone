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
#include <vector>

namespace mc::gfx {

// Owns the GL state for drawing the world: block shader, atlas, baked models and the
// chunk renderer. main.cpp drives it; all GL calls for a frame happen in here
// (hard rule 7).
class WorldRenderer {
public:
    // Load time: GL state, shaders, atlas, models. Call after initOpenGl().
    bool init();

    // Queue sections for (re)meshing. Marking a chunk also marks the sections of its
    // four neighbours' facing borders, whose face culling depends on it.
    void markChunkDirty(const world::World& world, world::ChunkPos pos);
    void markAllDirty(const world::World& world);

    // Uploads finished meshes, then snapshots dirty sections for the mesh workers,
    // nearest to `cameraPos` first, within a per-frame time budget and a cap on jobs
    // in flight. Call once per frame (main thread).
    void update(const world::World& world, const glm::dvec3& cameraPos);

    // Sections waiting to be meshed or uploaded (queued + in flight).
    int pendingMeshes() const { return m_inFlight + static_cast<int>(m_dirtyList.size()); }

    // Clears to the sky colour and draws the world from `camera`.
    void drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight);

    const ChunkRenderer::Stats& stats() const { return m_chunks.stats(); }

private:
    Shader m_blockShader;
    TextureAtlas m_atlas;
    BlockModels m_models;
    ChunkRenderer m_chunks;
    // Per-section scheduling state. Entries are erased once a section has no mesh
    // work pending. (M3 replaces this map with a dense grid around the camera.)
    struct SectionState {
        uint32_t version = 0;  // latest submitted job; older results are stale
        uint16_t inFlight = 0; // jobs submitted, not yet returned
        bool dirty = false;    // in m_dirtyList
    };
    void markDirty(world::SectionPos pos);
    void eraseIfIdle(world::SectionPos pos);

    std::unique_ptr<MeshWorkers> m_workers;
    std::unordered_map<world::SectionPos, SectionState> m_states;
    std::vector<world::SectionPos> m_dirtyList; // sorted far -> near before dispatch
    bool m_dirtyUnsorted = false;
    int m_inFlight = 0;
    int m_maxInFlight = 0;
};

} // namespace mc::gfx
