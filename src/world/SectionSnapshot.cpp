#include "world/SectionSnapshot.h"

#include <array>
#include <cstring>

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

bool captureSection(const World& world, SectionPos pos, SectionRefs& out) {
    out.pos = pos;
    if (const Chunk* centre = world.chunk({pos.x, pos.z})) out.biomes = centre->biomes();
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const Chunk* c = world.chunk({pos.x + dx, pos.z + dz});
            if (!c) {
                out = {}; // keep no partial references
                return false;
            }
            for (int dy = -1; dy <= 1; ++dy) {
                const int i = ((dy + 1) * 3 + (dz + 1)) * 3 + (dx + 1);
                const int s = pos.y + dy - (kMinY >> 4);
                if (s < 0 || s >= kSectionsPerChunk) {
                    out.blocks[i].reset();
                    out.light[i].reset();
                } else {
                    out.blocks[i] = c->shareSection(s);
                    out.light[i] = c->light(s);
                }
            }
        }
    }
    return true;
}

void buildPadded(const SectionRefs& refs, BlockStateId* blocks, uint8_t* sky, uint8_t* blockLight) {
    static thread_local std::array<BlockStateId, Section::kVolume> decoded;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int i = ((dy + 1) * 3 + (dz + 1)) * 3 + (dx + 1);
                // The part of this neighbour that falls inside the padded cube.
                const int x0 = dx < 0 ? 15 : 0, x1 = dx > 0 ? 0 : 15;
                const int y0 = dy < 0 ? 15 : 0, y1 = dy > 0 ? 0 : 15;
                const int z0 = dz < 0 ? 15 : 0, z1 = dz > 0 ? 0 : 15;
                const Section* sec = refs.blocks[i].get();
                if (sec && sec->isEmpty()) sec = nullptr; // all air
                const SectionLight* light = refs.light[i].get();
                const bool aboveWorld = refs.pos.y + dy > (kMaxY >> 4);
                // Without light data: above the world is open sky, a missing section
                // dark, an existing unlit one (tests) full sky.
                const uint8_t defaultSky = aboveWorld ? 15 : (refs.blocks[i] ? 15 : 0);
                // Decode whole sections only for the centre and its 6 face neighbours
                // (>= 256 cells used); edges (16) and corners (1) read cells directly.
                const bool whole = sec && (dx != 0) + (dy != 0) + (dz != 0) <= 1;
                if (whole) sec->copyTo(decoded.data());
                for (int ly = y0; ly <= y1; ++ly) {
                    for (int lz = z0; lz <= z1; ++lz) {
                        const int row = paddedIndex(x0 + dx * 16, ly + dy * 16, lz + dz * 16);
                        const int srow = Section::index(x0, ly, lz);
                        const int n = x1 - x0 + 1;
                        for (int k = 0; k < n; ++k) {
                            blocks[row + k] = !sec    ? BlockStateId{0}
                                              : whole ? decoded[srow + k]
                                                      : sec->getIndex(srow + k);
                        }
                        if (light) {
                            for (int k = 0; k < n; ++k) {
                                sky[row + k] = light->sky.get(srow + k);
                                blockLight[row + k] = light->block.get(srow + k);
                            }
                        } else {
                            std::memset(sky + row, defaultSky, static_cast<size_t>(n));
                            std::memset(blockLight + row, 0, static_cast<size_t>(n));
                        }
                    }
                }
            }
        }
    }
}

} // namespace mc::world
