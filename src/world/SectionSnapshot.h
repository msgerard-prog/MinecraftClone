#pragma once

#include "world/World.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

namespace mc::world {

// Section coordinates like vanilla's SectionPos: x/z = chunk coords, y = blockY >> 4
// (-4..19 for the 1.21 overworld). Block origin = (x*16, y*16, z*16).
struct SectionPos {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    bool operator==(const SectionPos&) const = default;
};

// A section plus a one-block border from its 26 neighbours, decoded into a flat
// 18^3 array — everything the mesher (and later lighting/AO) needs, readable from
// a worker thread without touching the World. Unloaded neighbours read as air.
inline constexpr int kPadded = 18;
inline constexpr int kPaddedVolume = kPadded * kPadded * kPadded;

// x, y, z in -1..16 (section-local; -1 and 16 are the border).
constexpr int paddedIndex(int x, int y, int z) {
    return ((y + 1) * kPadded + (z + 1)) * kPadded + (x + 1);
}

void snapshotSection(const World& world, SectionPos pos, BlockStateId* out);

// Shared references to a section and its 26 neighbours (blocks and light), captured
// on the main thread in O(27) and read on a worker (sections are copy-on-write).
// Index: ((dy + 1) * 3 + (dz + 1)) * 3 + (dx + 1). Missing light = not computed.
struct SectionRefs {
    SectionPos pos;
    std::array<std::shared_ptr<const Section>, 27> blocks;
    std::array<std::shared_ptr<const SectionLight>, 27> light;
    std::shared_ptr<const ChunkBiomes> biomes; // the centre chunk's
    int minSection = -4;  // the world's bottom section Y
    int maxSection = 19;  // the world's top section Y (above it: open air)
    uint8_t openSky = 15; // sky light of open air (0 where the dimension has none)
};

// Main thread. False if any of the 9 chunks around is not loaded.
bool captureSection(const World& world, SectionPos pos, SectionRefs& out);

// Worker: the padded 18^3 arrays a mesher needs - block states, sky and block light.
// Outside the world, blocks are air; sky light is the open-air value above / 0 below.
void buildPadded(const SectionRefs& refs, BlockStateId* blocks, uint8_t* sky, uint8_t* blockLight);

} // namespace mc::world

template <> struct std::hash<mc::world::SectionPos> {
    size_t operator()(const mc::world::SectionPos& p) const noexcept {
        uint64_t h = static_cast<uint32_t>(p.x);
        h = h * 0x9E3779B97F4A7C15ull ^ static_cast<uint32_t>(p.y);
        h = h * 0x9E3779B97F4A7C15ull ^ static_cast<uint32_t>(p.z);
        return static_cast<size_t>(h);
    }
};
