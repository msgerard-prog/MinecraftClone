#include "rendering/ChunkMesher.h"

#include "world/SectionSnapshot.h"

#include <glm/glm.hpp>

namespace mc::gfx {

namespace {

using world::paddedIndex;

// Corners of each face as seen from outside: top-left, bottom-left, bottom-right,
// top-right (counter-clockwise). "Top" of the texture is +Y on side faces, north (-Z)
// on the up face and south (+Z) on the down face, as in vanilla.
constexpr glm::ivec3 kCorners[world::kDirectionCount][4] = {
    {{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}, // down
    {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}, // up
    {{1, 1, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}}, // north
    {{0, 1, 1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}}, // south
    {{0, 1, 0}, {0, 0, 0}, {0, 0, 1}, {0, 1, 1}}, // west
    {{1, 1, 1}, {1, 0, 1}, {1, 0, 0}, {1, 1, 0}}, // east
};

// Offset of the neighbour in the padded array, per face.
constexpr int neighbourOffset(int face) {
    const glm::ivec3 n = world::kDirectionNormals[face];
    return paddedIndex(n.x, n.y, n.z) - paddedIndex(0, 0, 0);
}
constexpr int kNeighbour[world::kDirectionCount] = {
    neighbourOffset(0), neighbourOffset(1), neighbourOffset(2),
    neighbourOffset(3), neighbourOffset(4), neighbourOffset(5),
};

} // namespace

void meshSection(const world::BlockStateId* padded, const glm::ivec3& origin,
                 const world::BlockRegistry& registry, const BlockModels& models,
                 std::vector<PackedVertex>& out) {
    out.clear();
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                const int i = paddedIndex(x, y, z);
                const BakedModel& model = models[padded[i]];
                if (!model.visible) continue;
                const BakedVariant& variant = model.variants[variantIndex(
                    origin.x + x, origin.y + y, origin.z + z, model.variantCount)];
                for (int f = 0; f < world::kDirectionCount; ++f) {
                    if (registry.opaqueCube(padded[i + kNeighbour[f]])) continue; // hidden
                    const BakedFace& face = variant.faces[f];
                    for (int c = 0; c < 4; ++c) {
                        const glm::ivec3& k = kCorners[f][c];
                        // Rotation shifts which UV corner each geometric corner gets
                        // (+1 = texture turned 90 degrees clockwise); mirroring swaps
                        // left and right UVs (corner 0<->3, 1<->2).
                        uint32_t uv = uint32_t(c + face.rotation) & 3u;
                        if (face.mirror) uv = 3u - uv;
                        out.push_back(packVertex(uint32_t(x + k.x), uint32_t(y + k.y),
                                                 uint32_t(z + k.z), uint32_t(f), uv, face.sprite,
                                                 face.tint));
                    }
                }
            }
        }
    }
}

} // namespace mc::gfx
