#pragma once

#include <cstdint>

namespace mc::gfx {

// 8-byte block vertex (layout read by assets/shaders/block.vert):
//   lo: x:5 | y:5 | z:5 | face:3 | uvCorner:2 | sprite:12
//       x/y/z are 0..16 inside the section; face = world::Direction; uvCorner 0..3 =
//       (u0,v0) (u0,v1) (u1,v1) (u1,v0); sprite = atlas grid index.
//   hi: tint:2 (0 none, 1 grass, 2 water) | fluidTop:1 (lower this vertex by 1/9:
//       vanilla source-fluid surface height 8/9) | reserved: sky/block light, AO (M5)
struct PackedVertex {
    uint32_t lo = 0;
    uint32_t hi = 0;
};
static_assert(sizeof(PackedVertex) == 8);

enum class Tint : uint8_t { None = 0, Grass = 1, Water = 2 };

constexpr PackedVertex packVertex(uint32_t x, uint32_t y, uint32_t z, uint32_t face,
                                  uint32_t uvCorner, uint32_t sprite, Tint tint,
                                  bool fluidTop = false) {
    return {x | (y << 5) | (z << 10) | (face << 15) | (uvCorner << 18) | (sprite << 20),
            static_cast<uint32_t>(tint) | (fluidTop ? 4u : 0u)};
}

struct UnpackedVertex {
    uint32_t x, y, z, face, uvCorner, sprite;
    Tint tint;
    bool fluidTop;
};

constexpr UnpackedVertex unpackVertex(PackedVertex v) {
    return {v.lo & 31u,        (v.lo >> 5) & 31u, (v.lo >> 10) & 31u,           (v.lo >> 15) & 7u,
            (v.lo >> 18) & 3u, v.lo >> 20,        static_cast<Tint>(v.hi & 3u), (v.hi & 4u) != 0};
}

inline constexpr uint32_t kMaxSprites = 1u << 12;

} // namespace mc::gfx
