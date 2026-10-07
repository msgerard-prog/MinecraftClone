#pragma once

#include "rendering/BlockModels.h"
#include "world/Biome.h"
#include "rendering/PackedVertex.h"
#include "world/BlockRegistry.h"
#include "world/Direction.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace mc::gfx {

// Vanilla's fixed directional face shading, by world::Direction
// (down 0.5, up 1.0, north/south 0.8, west/east 0.6). Applied in block.vert from
// the vertex's face index; keep the shader's table in sync with this one.
inline constexpr float kFaceShade[world::kDirectionCount] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};

// One section's quads, split by render pass.
struct SectionMesh {
    std::vector<PackedVertex> opaque;      // solid + cutout (alpha-tested)
    std::vector<PackedVertex> translucent; // blended
    void clear() {
        opaque.clear();
        translucent.clear();
    }
};

// Builds the quads of one section from padded 18^3 arrays (world/SectionSnapshot.h):
// block states, sky light, block light. Full-cube faces are emitted only if the
// neighbour isn't an opaque cube (vanilla face culling; cullSame models also hide
// faces against the same block). Each vertex gets vanilla-style smooth lighting (the
// average light of the 4 cells around the corner on the face's side) and ambient
// occlusion (opaque cells among the two sides and the corner). Quads are two CCW
// triangles 0-1-2, 0-2-3; the diagonal flips to follow the darker corners.
// `origin` (block coordinates) picks per-position model variants. GL-free.
void meshSection(const world::BlockStateId* blocks, const uint8_t* sky, const uint8_t* blockLight,
                 const glm::ivec3& origin, const world::BlockRegistry& registry,
                 const BlockModels& models, SectionMesh& out,
                 const world::Biome* biomes = nullptr); // 64 cells of the section; null = plains

// Same with full sky light everywhere (tests, unlit previews).
void meshSection(const world::BlockStateId* blocks, const glm::ivec3& origin,
                 const world::BlockRegistry& registry, const BlockModels& models, SectionMesh& out);

// Upper bound of vertices for one section (every block, every face).
inline constexpr size_t kMaxSectionVertices = 4096 * 6 * 4;

} // namespace mc::gfx
