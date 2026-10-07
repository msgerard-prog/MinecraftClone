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

// Layout of DrawElementsIndirectCommand.
struct DrawCommand {
    uint32_t count, instanceCount, firstIndex;
    int32_t baseVertex;
    uint32_t baseInstance;
};

struct DrawSortItem {
    float distance2;
    uint32_t index;
};

// Reorders draws far -> near (blending order) by distance to each section centre
// (offset = section origin - camera). Writes to outCommands/outOffsets and renumbers
// baseInstance so each draw still finds its offset. GL-free; no allocation.
void sortDrawsBackToFront(std::span<const DrawCommand> commands, std::span<const glm::vec4> offsets,
                          std::span<DrawSortItem> scratch, std::span<DrawCommand> outCommands,
                          std::span<glm::vec4> outOffsets);

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
    void removeAll(); // every section (a dimension switch)

    // Issues the draw; the block shader, atlas and pass GL state must already be set.
    // backToFront: sort sections far -> near (translucent pass blending order).
    // maxDistance: skip sections whose centre is further (horizontally) than this.
    void draw(const Camera& camera, const glm::mat4& viewProjAtOrigin, bool backToFront = false,
              float maxDistance = 1e30f);

    struct Stats {
        int sections = 0;      // with a mesh
        int sectionsDrawn = 0; // after frustum culling
        uint64_t quadsDrawn = 0;
        uint64_t quadsTotal = 0; // all uploaded sections
        uint32_t arenaQuads = 0; // arena capacity
    };
    const Stats& stats() const { return m_stats; }

private:
    struct Entry {
        RangeAllocator::Range range; // in quads
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
    std::vector<DrawSortItem> m_sort; // reused (translucent ordering)
    std::vector<DrawCommand> m_sortedCommands;
    std::vector<glm::vec4> m_sortedOffsets;
    Stats m_stats;
    uint64_t m_quadsTotal = 0;
};

} // namespace mc::gfx
