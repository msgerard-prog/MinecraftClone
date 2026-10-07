#pragma once

#include <cstdint>

// World coordinate conventions (vanilla 1.21). Never change without asking.
// Y is up. A chunk column is 16x16 blocks; it is split into 16^3 sections.
namespace mc::world {

inline constexpr int kChunkSize = 16;
inline constexpr int kSectionSize = 16;
inline constexpr int kMinY = -64;
inline constexpr int kMaxY = 319; // inclusive; build height is 384 blocks
inline constexpr int kHeight = kMaxY - kMinY + 1;
inline constexpr int kSectionsPerChunk = kHeight / kSectionSize; // 24

// Block -> chunk coordinate. Uses an arithmetic shift, so negative blocks
// floor correctly (block -1 is in chunk -1, not chunk 0).
constexpr int32_t blockToChunk(int32_t block) { return block >> 4; }

// Block -> position inside its chunk/section, always 0..15.
constexpr int32_t blockToLocal(int32_t block) { return block & 15; }

// World Y -> section index within the chunk column (0 = bottom section at y=-64).
constexpr int32_t sectionIndex(int32_t y) { return (y - kMinY) >> 4; }

constexpr bool isInBuildHeight(int32_t y) { return y >= kMinY && y <= kMaxY; }

} // namespace mc::world
