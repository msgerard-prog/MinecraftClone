#include "world/Chunk.h"

namespace mc::world {

uint8_t Chunk::skyLight(int x, int y, int z) const {
    if (y > m_height.maxY()) { // above the world: like its top (open air: 15, or 0 without sky)
        const auto& top = m_light[size_t(m_height.sections() - 1)];
        return top ? top->sky.get(Section::index(x, 15, z)) : 15;
    }
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
