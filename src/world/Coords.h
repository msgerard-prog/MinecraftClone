#pragma once

#include <cstdint>

// World coordinate conventions (vanilla 1.21). Never change without asking.
// Y is up. A chunk column is 16x16 blocks; it is split into 16^3 sections. How tall
// the column is depends on the dimension (vanilla: dimension type min_y / height).
namespace mc::world {

inline constexpr int kChunkSize = 16;
inline constexpr int kSectionSize = 16;

// A dimension's vertical extent, as vanilla's LevelHeightAccessor: the lowest Y and
// the height (both multiples of 16). Section indices count from the bottom (0);
// "section Y" is the world coordinate y >> 4 (vanilla SectionPos).
struct HeightRange {
    int32_t minY = -64;
    int32_t height = 384;
    constexpr int32_t maxY() const { return minY + height - 1; } // inclusive
    constexpr int sections() const { return height >> 4; }
    constexpr int minSection() const { return minY >> 4; }        // section Y of index 0
    constexpr int maxSection() const { return maxY() >> 4; }
    constexpr int sectionIndex(int32_t y) const { return (y - minY) >> 4; }
    constexpr bool contains(int32_t y) const { return y >= minY && y <= maxY(); }
    constexpr bool operator==(const HeightRange&) const = default;
};

// Vanilla 1.21 dimension heights (wiki: Dimension type).
inline constexpr HeightRange kOverworldHeight{-64, 384}; // Y -64..319, 24 sections
inline constexpr HeightRange kNetherHeight{0, 256};      // Y 0..255, 16 sections
inline constexpr HeightRange kEndHeight{0, 256};         // Y 0..255, 16 sections
// Storage capacity: the tallest dimension (per-chunk section arrays are this big).
inline constexpr int kMaxSections = 24;
inline constexpr int kMaxHeight = kMaxSections * kSectionSize;

// Block -> chunk coordinate. Uses an arithmetic shift, so negative blocks
// floor correctly (block -1 is in chunk -1, not chunk 0).
constexpr int32_t blockToChunk(int32_t block) { return block >> 4; }

// Block -> position inside its chunk/section, always 0..15.
constexpr int32_t blockToLocal(int32_t block) { return block & 15; }

} // namespace mc::world
