#include "rendering/ChunkMesher.h"

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

void meshSection(const world::BlockStateId* blocks, const uint8_t* sky, const uint8_t* bl,
                 const glm::ivec3& origin, const world::BlockRegistry& registry,
                 const BlockModels& models, SectionMesh& out) {
    out.clear();
    const int up = kNeighbour[static_cast<int>(Direction::Up)];
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                const int i = paddedIndex(x, y, z);
                const world::BlockStateId state = blocks[i];
                const BakedModel& model = models[state];
                if (!model.visible) continue;
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
                        }
                        emitQuad(dst, v, false);
                        emitReversed(dst, v);
                    }
                    continue;
                }
                if (model.boxCount > 0) {
                    // Non-cube model: box faces, lit by the block's own cell, no AO.
                    for (int b = 0; b < model.boxCount; ++b) {
                        const BakedBox& box = model.boxes[b];
                        for (int f = 0; f < world::kDirectionCount; ++f) {
                            const BakedBox::Face& face = box.faces[f];
                            if (!face.present) continue;
                            VertexAttribs v[4];
                            for (int c = 0; c < 4; ++c) {
                                const glm::ivec3& k = kCorners[f][c];
                                v[c].x16 = uint32_t(x * 16 + (k.x ? box.to[0] : box.from[0]));
                                v[c].y16 = uint32_t(y * 16 + (k.y ? box.to[1] : box.from[1]));
                                v[c].z16 = uint32_t(z * 16 + (k.z ? box.to[2] : box.from[2]));
                                v[c].face = uint32_t(f);
                                v[c].sprite = face.sprite;
                                v[c].u = kCornerU[c] ? face.uv[2] : face.uv[0];
                                v[c].v = kCornerV[c] ? face.uv[3] : face.uv[1];
                                v[c].sky4 = sky[i] * 4u;
                                v[c].block4 = bl[i] * 4u;
                            }
                            emitQuad(dst, v, false);
                        }
                    }
                    continue;
                }

                const BakedVariant& variant = model.variants[variantIndex(
                    origin.x + x, origin.y + y, origin.z + z, model.variantCount)];
                // Vanilla source fluids are 8/9 tall unless the same fluid is above.
                const bool lowerTop = model.fluid && registry.blockOf(blocks[i + up]) != block;
                const int upFace = static_cast<int>(Direction::Up);
                for (int f = 0; f < world::kDirectionCount; ++f) {
                    const int n = i + kNeighbour[f];
                    const world::BlockStateId neighbour = blocks[n];
                    // A lowered fluid surface stays visible under a solid block.
                    const bool keepLoweredTop = lowerTop && f == upFace;
                    if (registry.opaqueCube(neighbour) && !keepLoweredTop) continue; // hidden
                    if (model.cullSame && registry.blockOf(neighbour) == block) continue;
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
                        v[c].y16 = uint32_t((y + k.y) * 16);
                        v[c].z16 = uint32_t((z + k.z) * 16);
                        v[c].face = uint32_t(f);
                        v[c].sprite = face.sprite;
                        v[c].u = uint32_t(kCornerU[uvc]);
                        v[c].v = uint32_t(kCornerV[uvc]);
                        v[c].tint = face.tint;
                        v[c].fluidTop = lowerTop && k.y == 1;
                        if (model.fluid) {
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
                    emitQuad(dst, v, bright(0) + bright(2) < bright(1) + bright(3));
                    // Vanilla shows fluid top and side faces from both sides (the surface
                    // from underwater) but not the bottom from above.
                    if (model.fluid && f != static_cast<int>(Direction::Down)) emitReversed(dst, v);
                }
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
