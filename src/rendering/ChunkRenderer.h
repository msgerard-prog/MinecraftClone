#pragma once

#include "core/RangeAllocator.h"
#include "rendering/Camera.h"
#include "rendering/PackedVertex.h"
#include "world/SectionSnapshot.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace mc::gfx {

// Draws all section meshes with one glMultiDrawElementsIndirect per frame.
//  - One vertex arena buffer holds every section's quads, sub-allocated in quads.
//  - One shared index buffer (0,1,2, 0,2,3 per quad) serves all sections via
//    baseVertex.
//  - Per draw, the section's offset from the camera (computed in double) goes to an
//    SSBO indexed by gl_BaseInstance, so vertices stay small and precise.
//  - Sections outside the view frustum are skipped on the CPU.
class ChunkRenderer {
public:
    ChunkRenderer() = default;
    ~ChunkRenderer();
    ChunkRenderer(const ChunkRenderer&) = delete;
    ChunkRenderer& operator=(const ChunkRenderer&) = delete;

    bool init();

    // Replaces a section's mesh (4 vertices per quad). Empty = remove. Main thread.
    void uploadSection(world::SectionPos pos, std::span<const PackedVertex> vertices);
    void removeSection(world::SectionPos pos);

    // Issues the draw; the block shader and atlas must already be bound.
    void draw(const Camera& camera, const glm::mat4& viewProjAtOrigin);

    struct Stats {
        int sections = 0;      // with a mesh
        int sectionsDrawn = 0; // after frustum culling
        uint64_t quadsDrawn = 0;
        uint32_t arenaQuads = 0; // arena capacity
    };
    const Stats& stats() const { return m_stats; }

private:
    struct Entry {
        RangeAllocator::Range range; // in quads
    };
    struct DrawCommand { // layout of DrawElementsIndirectCommand
        uint32_t count, instanceCount, firstIndex;
        int32_t baseVertex;
        uint32_t baseInstance;
    };

    void growArena(uint32_t minQuads);
    void ensureDrawCapacity(size_t sections);

    uint32_t m_vao = 0;
    uint32_t m_indexBuffer = 0;
    uint32_t m_arena = 0;
    uint32_t m_commandBuffer = 0;
    uint32_t m_offsetBuffer = 0;
    size_t m_drawCapacity = 0;
    RangeAllocator m_arenaAlloc;
    std::unordered_map<world::SectionPos, Entry> m_sections;
    std::vector<DrawCommand> m_commands; // reused every frame (no per-frame allocation)
    std::vector<glm::vec4> m_offsets;
    Stats m_stats;
};

} // namespace mc::gfx
