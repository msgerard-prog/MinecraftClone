#include "world/Chunk.h"

namespace mc::world {

uint8_t Chunk::skyLight(int x, int y, int z) const {
    if (y > kMaxY) return 15;
    if (y < kMinY) return 0;
    const auto& l = m_light[sectionIndex(y)];
    return l ? l->sky.get(Section::index(x, blockToLocal(y), z)) : 15;
}

uint8_t Chunk::blockLight(int x, int y, int z) const {
    if (!isInBuildHeight(y)) return 0;
    const auto& l = m_light[sectionIndex(y)];
    return l ? l->block.get(Section::index(x, blockToLocal(y), z)) : 0;
}

} // namespace mc::world
