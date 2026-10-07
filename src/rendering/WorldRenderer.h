#pragma once

#include "rendering/BlockModels.h"
#include "rendering/Camera.h"
#include "rendering/ChunkRenderer.h"
#include "rendering/PackedVertex.h"
#include "rendering/Shader.h"
#include "rendering/TextureAtlas.h"
#include "world/SectionSnapshot.h"
#include "world/World.h"

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

    // Meshes queued sections (main thread, synchronous until M2.4) and uploads them.
    void update(const world::World& world);

    // Clears to the sky colour and draws the world from `camera`.
    void drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight);

    const ChunkRenderer::Stats& stats() const { return m_chunks.stats(); }

private:
    Shader m_blockShader;
    TextureAtlas m_atlas;
    BlockModels m_models;
    ChunkRenderer m_chunks;
    std::unordered_set<world::SectionPos> m_dirty;
    std::vector<world::BlockStateId> m_padded; // reused snapshot buffer
    std::vector<PackedVertex> m_vertices;      // reused mesh buffer
};

} // namespace mc::gfx
