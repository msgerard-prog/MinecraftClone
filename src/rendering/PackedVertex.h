#pragma once

#include <cstdint>

namespace mc::gfx {

// 12-byte block vertex (layout read by assets/shaders/block.vert):
//   w0: x:9 | y:9 | z:9 | face:3          position in 1/16 block inside the section
//                                         (0..256), face = world::Direction
//   w1: sprite:12 | u:5 | v:5 | tint:2 | (free):1 | ao:2
//       sprite = atlas grid index; u, v = texel coordinates 0..16 inside the sprite
//       (v 0 = top row); tint 0 none / 1 grass / 2 water / 3 foliage; bit 24 is free
//       (fluid surfaces are at their corner heights in y16 since M14); ao = occluding neighbours 0..3
//   w2: sky:6 | block:6 | biome:8         light = sum of 4 smooth-lighting samples
//                                         (0..60), i.e. average x 4; biome = tint
//                                         palette slot (world::Biome, or a fixed slot)
struct PackedVertex {
    uint32_t w0 = 0;
    uint32_t w1 = 0;
    uint32_t w2 = 0;
};
static_assert(sizeof(PackedVertex) == 12);

enum class Tint : uint8_t { None = 0, Grass = 1, Water = 2, Foliage = 3 };

struct VertexAttribs {
    uint32_t x16, y16, z16; // 1/16 block units, 0..256
    uint32_t face;
    uint32_t sprite;
    uint32_t u, v; // 0..16
    Tint tint = Tint::None;
    uint32_t ao = 0;     // 0..3
    uint32_t sky4 = 60;  // 0..60 (sum of 4 samples)
    uint32_t block4 = 0; // 0..60
    uint32_t biome = 0;  // 0..255
};

constexpr PackedVertex packVertex(const VertexAttribs& a) {
    return {a.x16 | (a.y16 << 9) | (a.z16 << 18) | (a.face << 27),
            a.sprite | (a.u << 12) | (a.v << 17) | (static_cast<uint32_t>(a.tint) << 22) |
                (a.ao << 25),
            a.sky4 | (a.block4 << 6) | (a.biome << 12)};
}

constexpr VertexAttribs unpackVertex(PackedVertex p) {
    VertexAttribs a{};
    a.x16 = p.w0 & 511u;
    a.y16 = (p.w0 >> 9) & 511u;
    a.z16 = (p.w0 >> 18) & 511u;
    a.face = (p.w0 >> 27) & 7u;
    a.sprite = p.w1 & 4095u;
    a.u = (p.w1 >> 12) & 31u;
    a.v = (p.w1 >> 17) & 31u;
    a.tint = static_cast<Tint>((p.w1 >> 22) & 3u);
    a.ao = (p.w1 >> 25) & 3u;
    a.sky4 = p.w2 & 63u;
    a.block4 = (p.w2 >> 6) & 63u;
    a.biome = (p.w2 >> 12) & 255u;
    return a;
}

inline constexpr uint32_t kMaxSprites = 1u << 12;

} // namespace mc::gfx
