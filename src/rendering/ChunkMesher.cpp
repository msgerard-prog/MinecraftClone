#include "rendering/ChunkMesher.h"

#include "world/Blocks.h"
#include "world/SectionSnapshot.h"

#include <array>

namespace mc::gfx {

namespace {

using world::Direction;
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
// Texel coordinates (u, v) of the 4 sprite corners in order TL, BL, BR, TR.
constexpr int kCornerU[4] = {0, 0, 16, 16};
constexpr int kCornerV[4] = {0, 16, 16, 0};

constexpr int offset(glm::ivec3 d) { return paddedIndex(d.x, d.y, d.z) - paddedIndex(0, 0, 0); }
constexpr int kNeighbour[world::kDirectionCount] = {
    offset({0, -1, 0}), offset({0, 1, 0}),  offset({0, 0, -1}),
    offset({0, 0, 1}),  offset({-1, 0, 0}), offset({1, 0, 0}),
};

struct CornerLight {
    uint32_t sky4, block4, ao;
};

// Smooth light + AO at one face corner. `n` = padded index of the cell in front of
// the face; corner (cx, cy, cz) in {0,1} picks which way the two sides lie.
CornerLight cornerLight(const world::BlockStateId* blocks, const uint8_t* sky, const uint8_t* bl,
                        const world::BlockRegistry& reg, int n, int face, glm::ivec3 corner) {
    const glm::ivec3 normal = world::kDirectionNormals[face];
    // The two in-plane directions toward this corner.
    glm::ivec3 t1(0), t2(0);
    int k = 0;
    for (int a = 0; a < 3; ++a) {
        if (normal[a] != 0) continue;
        glm::ivec3& t = k++ == 0 ? t1 : t2;
        t[a] = corner[a] == 1 ? 1 : -1;
    }
    const int s1 = n + offset(t1), s2 = n + offset(t2), c = n + offset(t1 + t2);
    const bool o1 = reg.opaqueCube(blocks[s1]), o2 = reg.opaqueCube(blocks[s2]);
    const bool oc = (o1 && o2) || reg.opaqueCube(blocks[c]);
    // Average light over the non-opaque cells (the cell in front always counts).
    uint32_t skySum = sky[n], blockSum = bl[n], count = 1;
    if (!o1) {
        skySum += sky[s1];
        blockSum += bl[s1];
        ++count;
    }
    if (!o2) {
        skySum += sky[s2];
        blockSum += bl[s2];
        ++count;
    }
    if (!oc) {
        skySum += sky[c];
        blockSum += bl[c];
        ++count;
    }
    return {(skySum * 4 + count / 2) / count, (blockSum * 4 + count / 2) / count,
            uint32_t(o1) + uint32_t(o2) + uint32_t(oc)};
}

void emitQuad(std::vector<PackedVertex>& dst, VertexAttribs v[4], bool flip) {
    // Flip the diagonal when 0-2 is darker than 1-3 (avoids AO/light streaks).
    if (flip) {
        dst.insert(dst.end(),
                   {packVertex(v[1]), packVertex(v[2]), packVertex(v[3]), packVertex(v[0])});
    } else {
        dst.insert(dst.end(),
                   {packVertex(v[0]), packVertex(v[1]), packVertex(v[2]), packVertex(v[3])});
    }
}

void emitReversed(std::vector<PackedVertex>& dst, VertexAttribs v[4]) {
    dst.insert(dst.end(), {packVertex(v[0]), packVertex(v[3]), packVertex(v[2]), packVertex(v[1])});
}

} // namespace

// A fluid cell's surface height (vanilla: amount / 9, so a source is 8/9; falling
// fluid and fluid with the same fluid above are full).
static float fluidHeight(const world::BlockRegistry& reg, world::BlockStateId s) {
    if (reg.waterlogged(s)) return 8.0f / 9.0f; // (a water source shares the cell)
    const int level = reg.get(s, world::properties::level);
    const int amount = level == 0 || level >= 8 ? 8 : 8 - level;
    return float(amount) / 9.0f;
}

// The fluid a cell holds for meshing: its own block, or water when waterlogged (M25.1).
static world::BlockId fluidBlockOf(const world::BlockRegistry& reg, world::BlockStateId s) {
    return reg.waterlogged(s) ? world::BlockId(world::blocks::Water) : reg.blockOf(s);
}

// Height (1/16 block) of a fluid surface corner: vanilla averages the 4 cells around
// it - the fluid's own heights (sources and near-full cells weigh 10x), open cells as
// 0, solid cells not at all; full if any of them has the same fluid above.
uint32_t fluidCornerHeight(const world::BlockStateId* blocks, const world::BlockRegistry& reg,
                           world::BlockId fluid, int x, int y, int z, int cx, int cz) {
    const int up = paddedIndex(0, 1, 0) - paddedIndex(0, 0, 0);
    float sum = 0.0f, weight = 0.0f;
    for (int dz = cz - 1; dz <= cz; ++dz)
        for (int dx = cx - 1; dx <= cx; ++dx) {
            const int c = paddedIndex(x + dx, y, z + dz);
            if (fluidBlockOf(reg, blocks[c + up]) == fluid) return 16u;
            const world::BlockStateId s = blocks[c];
            if (fluidBlockOf(reg, s) == fluid) {
                const float h = fluidHeight(reg, s);
                const float w = h >= 0.8f ? 10.0f : 1.0f;
                sum += h * w;
                weight += w;
            } else if (!reg.collides(s)) {
                weight += 1.0f;
            }
        }
    const float h = weight > 0.0f ? sum / weight : 8.0f / 9.0f;
    return uint32_t(h * 16.0f + 0.5f);
}


void meshSection(const world::BlockStateId* blocks, const uint8_t* sky, const uint8_t* bl,
                 const glm::ivec3& origin, const world::BlockRegistry& registry,
                 const BlockModels& models, SectionMesh& out, const world::Biome* biomes) {
    out.clear();
    const int up = kNeighbour[static_cast<int>(Direction::Up)];
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                const int i = paddedIndex(x, y, z);
                const world::BlockStateId state = blocks[i];
                const BakedModel& model = models[state];
                const bool wet = registry.waterlogged(state);
                if (!model.visible && !wet) continue;
                // Tint palette slot: the biome of this block's 4x4x4 cell (no blending), or the
                // model's fixed slot (birch/spruce leaves).
                const uint32_t cellBiome =
                    biomes ? static_cast<uint32_t>(biomes[((y >> 2) * 4 + (z >> 2)) * 4 + (x >> 2)]) : 0u;
                uint32_t biome = model.fixedTintSlot ? model.fixedTintSlot : cellBiome;

                // Full-cell faces (cubes and fluids), hidden against opaque neighbours.
                auto cube = [&](const BakedModel& cm, world::BlockId block) {
                    std::vector<PackedVertex>& cdst = cm.translucent ? out.translucent : out.opaque;
                const BakedVariant& variant = cm.variants[variantIndex(
                    origin.x + x, origin.y + y, origin.z + z, cm.variantCount)];
                // Fluid surfaces sit at their corner heights unless the same fluid is above.
                const bool lowerTop = cm.fluid && fluidBlockOf(registry, blocks[i + up]) != block;
                uint32_t cornerH[2][2] = {{16u, 16u}, {16u, 16u}};
                if (lowerTop)
                    for (int cz = 0; cz < 2; ++cz)
                        for (int cx = 0; cx < 2; ++cx)
                            cornerH[cx][cz] = fluidCornerHeight(blocks, registry, block, x, y, z, cx, cz);
                const int upFace = static_cast<int>(Direction::Up);
                for (int f = 0; f < world::kDirectionCount; ++f) {
                    const int n = i + kNeighbour[f];
                    const world::BlockStateId neighbour = blocks[n];
                    // A lowered fluid surface stays visible under a solid block.
                    const bool keepLoweredTop = lowerTop && f == upFace;
                    if (registry.opaqueCube(neighbour) && !keepLoweredTop) continue; // hidden
                    if (cm.cullSame && (cm.fluid ? fluidBlockOf(registry, neighbour) : registry.blockOf(neighbour)) == block)
                        continue;
                    const BakedFace& face = variant.faces[f];
                    VertexAttribs v[4];
                    for (int c = 0; c < 4; ++c) {
                        const glm::ivec3& k = kCorners[f][c];
                        // Rotation shifts which UV corner each geometric corner gets
                        // (+1 = texture turned 90 degrees clockwise); mirroring swaps
                        // left and right.
                        uint32_t uvc = uint32_t(c + face.rotation) & 3u;
                        if (face.mirror) uvc = 3u - uvc;
                        v[c].x16 = uint32_t((x + k.x) * 16);
                        v[c].y16 = uint32_t(y * 16) + (k.y ? cornerH[k.x][k.z] : 0u);
                        v[c].z16 = uint32_t((z + k.z) * 16);
                        v[c].face = uint32_t(f);
                        v[c].sprite = face.sprite;
                        v[c].u = uint32_t(kCornerU[uvc]);
                        v[c].v = uint32_t(kCornerV[uvc]);
                        v[c].tint = face.tint;
                        v[c].biome = biome;
                        if (cm.fluid) {
                            // Fluids: flat light, the brighter of the fluid's own cell and
                            // the one in front (a lowered top under a solid block faces an
                            // opaque cell whose stored light is 0).
                            v[c].sky4 = std::max(sky[n], sky[i]) * 4u;
                            v[c].block4 = std::max(bl[n], bl[i]) * 4u;
                        } else {
                            const CornerLight l = cornerLight(blocks, sky, bl, registry, n, f, k);
                            v[c].sky4 = l.sky4;
                            v[c].block4 = l.block4;
                            v[c].ao = l.ao;
                        }
                    }
                    const auto bright = [&](int c) {
                        return int(v[c].sky4 + v[c].block4) - int(v[c].ao) * 8;
                    };
                    emitQuad(cdst, v, bright(0) + bright(2) < bright(1) + bright(3));
                    // Vanilla shows fluid top and side faces from both sides (the surface
                    // from underwater) but not the bottom from above.
                    if (cm.fluid && f != static_cast<int>(Direction::Down)) emitReversed(cdst, v);
                }
                };
                if (wet) { // waterlogged (M25.1): the water first, then the block in it
                    biome = cellBiome;
                    const BakedModel& water = models[registry.defaultState(world::blocks::Water)];
                    cube(water, world::blocks::Water);
                    biome = model.fixedTintSlot ? model.fixedTintSlot : cellBiome;
                    if (!model.visible) continue;
                }
                std::vector<PackedVertex>& dst = model.translucent ? out.translucent : out.opaque;
                const world::BlockId block = registry.blockOf(state);

                if (model.cross) {
                    // Two diagonal planes, both sides, flat light from the plant's own
                    // cell; "Up" shading (no directional darkening).
                    static constexpr int kPlane[2][4][2] = {{{0, 0}, {0, 0}, {16, 16}, {16, 16}},
                                                            {{16, 0}, {16, 0}, {0, 16}, {0, 16}}};
                    for (const auto& plane : kPlane) {
                        VertexAttribs v[4];
                        for (int c = 0; c < 4; ++c) {
                            const bool top = c == 0 || c == 3;
                            v[c].x16 = uint32_t(x * 16 + plane[c][0]);
                            v[c].y16 = uint32_t(y * 16 + (top ? 16 : 0));
                            v[c].z16 = uint32_t(z * 16 + plane[c][1]);
                            v[c].face = uint32_t(Direction::Up);
                            v[c].sprite = model.crossSprite;
                            v[c].u = c < 2 ? 0u : 16u;
                            v[c].v = top ? 0u : 16u;
                            v[c].tint = model.crossTint;
                            v[c].sky4 = sky[i] * 4u;
                            v[c].block4 = bl[i] * 4u;
                            v[c].biome = biome;
                        }
                        emitQuad(dst, v, false);
                        emitReversed(dst, v);
                    }
                    if (model.boxCount == 0) continue; // (campfires: a cross over boxes)
                }
                if (model.boxCount > 0) {
                    // Non-cube model: box faces, lit by the block's own cell, no AO - except a
                    // face flush with the block's side (vanilla: lit from the cell it faces,
                    // and hidden against an opaque one; M29.5).
                    for (int b = 0; b < model.boxCount; ++b) {
                        const BakedBox& box = model.boxes[b];
                        for (int f = 0; f < world::kDirectionCount; ++f) {
                            const BakedBox::Face& face = box.faces[f];
                            if (!face.present) continue;
                            const auto dir = static_cast<Direction>(f);
                            const bool flush = dir == Direction::Down    ? box.from[1] == 0
                                               : dir == Direction::Up    ? box.to[1] == 16
                                               : dir == Direction::North ? box.from[2] == 0
                                               : dir == Direction::South ? box.to[2] == 16
                                               : dir == Direction::West  ? box.from[0] == 0
                                                                         : box.to[0] == 16;
                            int li = i;
                            if (flush) {
                                li = i + kNeighbour[f];
                                if (registry.opaqueCube(blocks[li])) continue;
                            }
                            VertexAttribs v[4];
                            for (int c = 0; c < 4; ++c) {
                                const glm::ivec3& k = kCorners[f][c];
                                v[c].x16 = uint32_t(x * 16 + (k.x ? box.to[0] : box.from[0]));
                                v[c].y16 = uint32_t(y * 16 + (k.y ? box.to[1] : box.from[1]));
                                v[c].z16 = uint32_t(z * 16 + (k.z ? box.to[2] : box.from[2]));
                                v[c].face = uint32_t(f);
                                v[c].sprite = face.sprite;
                                const uint32_t uvc = uint32_t(c + face.rotation) & 3u; // as cube faces
                                v[c].u = kCornerU[uvc] ? face.uv[2] : face.uv[0];
                                v[c].v = kCornerV[uvc] ? face.uv[3] : face.uv[1];
                                v[c].tint = face.tint;
                                v[c].sky4 = sky[li] * 4u;
                                v[c].block4 = bl[li] * 4u;
                                v[c].biome = biome;
                            }
                            emitQuad(dst, v, false);
                        }
                    }
                    continue;
                }

                cube(model, block);
            }
        }
    }
}

void meshSection(const world::BlockStateId* blocks, const glm::ivec3& origin,
                 const world::BlockRegistry& registry, const BlockModels& models,
                 SectionMesh& out) {
    static thread_local std::array<uint8_t, world::kPaddedVolume> fullSky = [] {
        std::array<uint8_t, world::kPaddedVolume> a{};
        a.fill(15);
        return a;
    }();
    static thread_local std::array<uint8_t, world::kPaddedVolume> dark{};
    meshSection(blocks, fullSky.data(), dark.data(), origin, registry, models, out);
}

} // namespace mc::gfx
