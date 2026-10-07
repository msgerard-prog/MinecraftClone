#include "world/SectionSnapshot.h"

#include <array>

namespace mc::world {

void snapshotSection(const World& world, SectionPos pos, BlockStateId* out) {
    // The 3x3 chunk neighbourhood, looked up once (not per block).
    std::array<const Chunk*, 9> chunks{};
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            chunks[(dz + 1) * 3 + (dx + 1)] = world.chunk({pos.x + dx, pos.z + dz});
        }
    }
    const Chunk* center = chunks[4];
    const int baseY = pos.y * 16;

    // Interior: decode the whole section at once.
    if (center && isInBuildHeight(baseY)) {
        static thread_local BlockStateId interior[Section::kVolume];
        center->section(sectionIndex(baseY)).copyTo(interior);
        for (int y = 0; y < 16; ++y)
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    out[paddedIndex(x, y, z)] = interior[Section::index(x, y, z)];
    } else {
        for (int y = 0; y < 16; ++y)
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    out[paddedIndex(x, y, z)] = 0;
    }

    // Border shell: per-block reads from the neighbouring chunks/sections.
    for (int y = -1; y <= 16; ++y) {
        for (int z = -1; z <= 16; ++z) {
            for (int x = -1; x <= 16; ++x) {
                const bool border = x < 0 || x > 15 || y < 0 || y > 15 || z < 0 || z > 15;
                if (!border) continue;
                const int cx = x < 0 ? 0 : (x > 15 ? 2 : 1);
                const int cz = z < 0 ? 0 : (z > 15 ? 2 : 1);
                const Chunk* c = chunks[cz * 3 + cx];
                out[paddedIndex(x, y, z)] =
                    c ? c->get(blockToLocal(x), baseY + y, blockToLocal(z)) : BlockStateId{0};
            }
        }
    }
}

} // namespace mc::world
