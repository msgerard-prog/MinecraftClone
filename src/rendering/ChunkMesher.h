#pragma once

#include "rendering/BlockModels.h"
#include "rendering/PackedVertex.h"
#include "world/BlockRegistry.h"
#include "world/Direction.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc::gfx {

// Vanilla's fixed directional face shading, by world::Direction
// (down 0.5, up 1.0, north/south 0.8, west/east 0.6). Applied in block.vert from
// the vertex's face index; keep the shader's table in sync with this one.
inline constexpr float kFaceShade[world::kDirectionCount] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};

// Builds the quads of one section from a padded 18^3 snapshot (world/SectionSnapshot.h).
// A face is emitted only if the neighbouring block is not an opaque full cube (vanilla
// face culling). Appends 4 vertices per quad to `out` (cleared first); quads are drawn
// as two CCW triangles with indices 0-1-2, 0-2-3. GL-free and thread-safe.
// `origin` is the section's block origin (picks per-position model variants).
void meshSection(const world::BlockStateId* padded, const glm::ivec3& origin,
                 const world::BlockRegistry& registry, const BlockModels& models,
                 std::vector<PackedVertex>& out);

// Upper bound of vertices for one section (every block, every face).
inline constexpr size_t kMaxSectionVertices = 4096 * 6 * 4;

} // namespace mc::gfx
