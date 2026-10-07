#include "rendering/CubeMesher.h"

namespace mc::gfx {

namespace {

using world::Direction;

// Corners of each face as seen from outside: top-left, bottom-left, bottom-right,
// top-right (counter-clockwise). "Top" of the texture is +Y on side faces and
// north (-Z) on the up face, as in vanilla.
constexpr glm::ivec3 kCorners[world::kDirectionCount][4] = {
    {{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}, // down
    {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}, // up
    {{1, 1, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}}, // north
    {{0, 1, 1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}}, // south
    {{0, 1, 0}, {0, 0, 0}, {0, 0, 1}, {0, 1, 1}}, // west
    {{1, 1, 1}, {1, 0, 1}, {1, 0, 0}, {1, 1, 0}}, // east
};

uint32_t multiplyColor(uint32_t rgba, float shade) {
    uint32_t out = rgba & 0xFF000000u; // keep alpha
    for (int shift = 0; shift < 24; shift += 8) {
        const float c = static_cast<float>((rgba >> shift) & 0xFFu) * shade;
        out |= static_cast<uint32_t>(c + 0.5f) << shift;
    }
    return out;
}

} // namespace

float faceShade(Direction face) {
    switch (face) {
    case Direction::Up:
        return 1.0f;
    case Direction::Down:
        return 0.5f;
    case Direction::North:
    case Direction::South:
        return 0.8f;
    case Direction::West:
    case Direction::East:
        return 0.6f;
    }
    return 1.0f;
}

void appendCube(std::vector<BlockVertex>& out, const glm::ivec3& pos, const CubeFaces& faces,
                uint8_t faceMask) {
    for (int f = 0; f < world::kDirectionCount; ++f) {
        if (!(faceMask & (1u << f))) continue;
        const UvRect& uv = faces.uv[f];
        const uint32_t color = multiplyColor(faces.tint[f], faceShade(static_cast<Direction>(f)));
        const float us[4] = {uv.u0, uv.u0, uv.u1, uv.u1};
        const float vs[4] = {uv.v0, uv.v1, uv.v1, uv.v0};
        BlockVertex quad[4];
        for (int i = 0; i < 4; ++i) {
            const glm::ivec3 p = pos + kCorners[f][i];
            quad[i] = {float(p.x), float(p.y), float(p.z), us[i], vs[i], color};
        }
        // Two CCW triangles: 0-1-2 and 0-2-3.
        out.insert(out.end(), {quad[0], quad[1], quad[2], quad[0], quad[2], quad[3]});
    }
}

CubeFaces cubeAll(const TextureAtlas& atlas, const char* sprite) {
    CubeFaces faces;
    const UvRect uv = atlas.sprite(sprite);
    for (auto& f : faces.uv)
        f = uv;
    return faces;
}

CubeFaces cubeColumn(const TextureAtlas& atlas, const char* side, const char* end) {
    CubeFaces faces = cubeAll(atlas, side);
    faces.uv[int(Direction::Up)] = faces.uv[int(Direction::Down)] = atlas.sprite(end);
    return faces;
}

CubeFaces grassBlock(const TextureAtlas& atlas) {
    CubeFaces faces = cubeAll(atlas, "grass_block_side");
    faces.uv[int(Direction::Up)] = atlas.sprite("grass_block_top");
    faces.uv[int(Direction::Down)] = atlas.sprite("dirt");
    faces.tint[int(Direction::Up)] = kPlainsGrassTint; // greyscale top tinted by biome
    return faces;
}

} // namespace mc::gfx
