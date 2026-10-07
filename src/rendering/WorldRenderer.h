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
    bool init();

    // Queue sections for (re)meshing. Marking a chunk also marks the sections of its
    // four neighbours' facing borders, whose face culling depends on it.
    void markChunkDirty(const world::World& world, world::ChunkPos pos);
    void markAllDirty(const world::World& world);

    // Snapshots dirty sections for the mesh workers (main thread) and uploads
    // finished meshes. Call once per frame.
    void update(const world::World& world);

    // Sections submitted to workers whose results haven't been uploaded yet.
    int pendingMeshes() const { return m_inFlight; }

    // Clears to the sky colour and draws the world from `camera`.
    void drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight);

    const ChunkRenderer::Stats& stats() const { return m_chunks.stats(); }

private:
    Shader m_blockShader;
    TextureAtlas m_atlas;
    BlockModels m_models;
    ChunkRenderer m_chunks;
    std::unique_ptr<MeshWorkers> m_workers;
    std::unordered_set<world::SectionPos> m_dirty;
    std::unordered_map<world::SectionPos, uint32_t> m_versions; // latest submitted
    int m_inFlight = 0;
};

} // namespace mc::gfx
