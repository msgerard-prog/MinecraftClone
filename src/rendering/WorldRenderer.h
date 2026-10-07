#pragma once

#include "world/Dimension.h"

#include "rendering/BlockModels.h"
#include "rendering/Camera.h"
#include "rendering/ChunkMeshTracker.h"
#include "rendering/ChunkRenderer.h"
#include "rendering/MeshWorkers.h"
#include "rendering/PackedVertex.h"
#include "rendering/ResourcePack.h"
#include "rendering/Shader.h"
#include "rendering/SkyRenderer.h"
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
    WorldRenderer() = default;
    ~WorldRenderer();
    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

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

    // Streaming: a chunk is meshed once it and all 8 neighbours are lit (its border
    // faces and smooth lighting depend on them), so loaded-area edges never show walls.
    void onChunksLit(const world::World& world, const std::vector<world::ChunkPos>& lit);
    // Sections whose light changed (relit after an edit): re-mesh them and their
    // neighbours (meshes read light across section borders).
    void onLightChanged(const std::vector<world::SectionPos>& sections);
    void onChunksUnloaded(const std::vector<world::ChunkPos>& unloaded);

    // Blocks edited by the player: re-mesh their sections, plus the neighbouring
    // section when a block sits on a section border (its faces there change too).
    void onBlocksChanged(const std::vector<world::BlockPos>& changed);

    // Uploads finished meshes, then snapshots dirty sections for the mesh workers,
    // nearest to `cameraPos` first, within a per-frame time budget and a cap on jobs
    // in flight. Call once per frame (main thread).
    void update(const world::World& world, const glm::dvec3& cameraPos);

    // Sections waiting to be meshed or uploaded (queued + in flight).
    int pendingMeshes() const { return m_inFlight + static_cast<int>(m_dirtyList.size()); }

    // Clears to the sky colour and draws the world from `camera`.
    void drawFrame(const Camera& camera, int framebufferWidth, int framebufferHeight);

    // Fog reaches the sky colour at the edge of the render distance (in chunks).
    // Also sizes the opaque vertex arena for it (~2700 quads per chunk column of
    // terrain measured at RD 12-16, rounded up to a power of two).
    void setRenderDistance(int chunks);
    // Time of day (world/DayTime.h): sky colour, sun/moon/stars and the sky light
    // lost at night. `partialTick` interpolates between ticks for smooth motion.
    void setDayTime(int64_t dayTime, float partialTick);
    // The Nether's fog colour (each Nether biome has its own; main eases it toward
    // the one at the camera). Default: Nether Wastes' #330808.
    void setNetherFog(const glm::vec3& rgb) { m_netherFog = rgb; }
    // Sky, fog and light differ per dimension (wiki: Dimension type, Fog).
    // Also drops every mesh, queued re-mesh and in-flight result of the previous
    // dimension (its chunks are all unloaded at a switch).
    void setDimension(world::Dimension d);
    float skyDarken() const { return m_skyDarken; }

    // Average GPU time of drawFrame (both passes) over the frames measured so far,
    // from GL timer queries read back a few frames late (no pipeline stall).
    double averageGpuMs() const { return m_gpuSamples ? m_gpuTotalMs / m_gpuSamples : 0.0; }
    double maxGpuMs() const { return m_gpuMaxMs; }
    // Start GPU statistics afresh (e.g. once loading is done, like the CPU stats).
    void resetGpuStats() {
        m_gpuTotalMs = m_gpuMaxMs = 0.0;
        m_gpuSamples = 0;
    }

    // For the GUI (item icons): the block atlas and baked models.
    const TextureAtlas& atlas() const { return m_atlas; }
    const BlockModels& models() const { return m_models; }
    // The pack stack's textures for other renderers (GUI), valid after init().
    const PackStack& packs() const { return *m_packs; }

    const ChunkRenderer::Stats& stats() const { return m_chunks.stats(); }
    const ChunkRenderer::Stats& translucentStats() const { return m_translucent.stats(); }

private:
    std::unique_ptr<PackStack> m_packs;
    Shader m_blockShader;
    TextureAtlas m_atlas;
    BlockModels m_models;
    ChunkRenderer m_chunks;      // opaque pass
    ChunkRenderer m_translucent; // blended pass (water...), drawn back to front
    int m_renderDistance = 12;
    world::Dimension m_dimension = world::Dimension::Overworld;
    int m_minSection = world::kOverworldHeight.minSection(); // -4
    int m_maxSection = world::kOverworldHeight.maxSection(); // 19
    float m_skyDarken = 0.0f;       // sky light levels lost to night, 0..11
    glm::vec3 m_skyColor{0.0f};     // clear and fog colour
    SkyState m_skyState;
    SkyRenderer m_sky;
    uint32_t m_tintPalette = 0; // SSBO binding 1: biome tint colours
    static constexpr int kQueryRing = 4;
    uint32_t m_queries[kQueryRing] = {};
    bool m_queryPending[kQueryRing] = {};
    int m_queryIndex = 0;
    double m_gpuTotalMs = 0.0;
    double m_gpuMaxMs = 0.0;
    int m_gpuSamples = 0;
    // Per-section scheduling state. Entries are erased once a section has no mesh
    // work pending. Known hard-rule-1 exception: node-based maps here and in
    // ChunkRenderer/World allocate when chunks stream in (ROADMAP › deferred perf:
    // dense grid around the camera).
    struct SectionState {
        uint32_t version = 0;  // latest submitted job; older results are stale
        uint16_t inFlight = 0; // jobs submitted, not yet returned
        bool dirty = false;    // in m_dirtyList
    };
    void markDirty(world::SectionPos pos);
    void markChunkSections(const world::World& world, world::ChunkPos pos);
    void eraseIfIdle(world::SectionPos pos);

    std::unique_ptr<MeshWorkers> m_workers;
    ChunkMeshTracker m_meshTracker;       // streaming: chunks given meshes
    bool m_streaming = false;             // false: static world (everything meshed up front)
    std::vector<world::ChunkPos> m_ready; // reused
    std::unordered_map<world::SectionPos, SectionState> m_states;
    std::vector<world::SectionPos> m_dirtyList; // sorted far -> near before dispatch
    bool m_dirtyUnsorted = false;
    size_t m_sortedDirty = 0;                      // m_dirtyList[0, n) is sorted
    std::vector<world::SectionPos> m_mergeScratch; // reused
    int m_inFlight = 0;
    int m_maxInFlight = 0;
    glm::vec3 m_netherFog{0x33 / 255.0f, 0x08 / 255.0f, 0x08 / 255.0f};
};

} // namespace mc::gfx
