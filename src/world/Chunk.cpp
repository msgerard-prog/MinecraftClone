#include "world/Chunk.h"

namespace mc::world {

uint8_t Chunk::skyLight(int x, int y, int z) const {
    if (y > m_height.maxY()) return 15;
    if (y < m_height.minY) return 0;
    const auto& l = m_light[size_t(m_height.sectionIndex(y))];
    return l ? l->sky.get(Section::index(x, blockToLocal(y), z)) : 15;
}

uint8_t Chunk::blockLight(int x, int y, int z) const {
    if (!m_height.contains(y)) return 0;
    const auto& l = m_light[size_t(m_height.sectionIndex(y))];
    return l ? l->block.get(Section::index(x, blockToLocal(y), z)) : 0;
}

} // namespace mc::world
