#pragma once

#include "rendering/Mesh.h"
#include "rendering/TextureAtlas.h"
#include "world/Direction.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace mc::gfx {

inline constexpr uint32_t kNoTint = 0xFFFFFFFF;
// Vanilla plains biome grass colour (#91BD59), until biomes exist (M8).
inline constexpr uint32_t kPlainsGrassTint = 0xFF59BD91; // ABGR in memory = RGBA bytes

// Per-face sprite and tint for a full cube, indexed by world::Direction.
struct CubeFaces {
    UvRect uv[world::kDirectionCount];
    uint32_t tint[world::kDirectionCount] = {kNoTint, kNoTint, kNoTint, kNoTint, kNoTint, kNoTint};
};

// Vanilla's fixed directional face shading (no light engine needed):
// up 1.0, down 0.5, north/south 0.8, east/west 0.6.
float faceShade(world::Direction face);

// Appends the 6 faces (2 triangles each, CCW from outside) of a unit cube at `pos`.
// `faceMask` bit i (Direction i) set = emit that face (M2 culls hidden faces with it).
void appendCube(std::vector<BlockVertex>& out, const glm::ivec3& pos, const CubeFaces& faces,
                uint8_t faceMask = 0x3F);

// Model helpers mirroring vanilla's block/cube_all, block/cube_column and grass_block.
CubeFaces cubeAll(const TextureAtlas& atlas, const char* sprite);
CubeFaces cubeColumn(const TextureAtlas& atlas, const char* side, const char* end);
CubeFaces grassBlock(const TextureAtlas& atlas);

} // namespace mc::gfx
