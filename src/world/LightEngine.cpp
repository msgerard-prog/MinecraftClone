#include "world/LightEngine.h"

#include "world/Blocks.h"

#include <algorithm>
#include <vector>

namespace mc::world {

namespace {

// The region lit around the centre chunk: 15 blocks of margin on each side (light
// from further away can't reach the centre).
constexpr int kMargin = 15;
constexpr int kW = 16 + 2 * kMargin; // 46
constexpr int kLayer = kW * kW;

struct Region {
    int y0 = 0, y1 = 0; // lit y range [y0, y1)
    std::vector<uint8_t> opacity, sky, block;
    std::vector<uint32_t> queue;
    std::vector<int> height; // per column: first y below which sky light isn't 15
    int index(int ix, int y, int iz) const { return (y - y0) * kLayer + iz * kW + ix; }
};

} // namespace

const std::shared_ptr<const SectionLight>& sharedUniformLight(uint8_t sky) {
    static const auto make = [](uint8_t v) {
        auto l = std::make_shared<SectionLight>();
        l->sky.fill(v);
        return std::shared_ptr<const SectionLight>(std::move(l));
    };
    static const std::shared_ptr<const SectionLight> open = make(15), dark = make(0);
    return sky == 15 ? open : dark;
}

bool ChunkNeighbourhood::capture(const World& world, ChunkPos center, ChunkNeighbourhood& out) {
    out.center = center;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const Chunk* c = world.chunk({center.x + dx, center.z + dz});
            if (!c) return false;
            auto& dst = out.sections[(dz + 1) * 3 + (dx + 1)];
            for (int s = 0; s < kSectionsPerChunk; ++s)
                dst[s] = c->shareSection(s);
        }
    }
    return true;
}

ChunkLight computeChunkLight(const ChunkNeighbourhood& n) {
    const auto& reg = blockRegistry();
    static thread_local Region r;
    static thread_local std::vector<BlockStateId> sectionStates(Section::kVolume);

    // 1. Vertical extent worth lighting: from 16 below the lowest surface/emitter in
    //    the region to just above the highest non-empty section. Everything above is
    //    open sky (15); everything below that range is dark.
    int topSection = -1;
    for (const auto& chunk : n.sections)
        for (int s = kSectionsPerChunk - 1; s > topSection; --s)
            if (!chunk[s]->isEmpty()) topSection = s;
    ChunkLight result;
    if (topSection < 0) { // nothing at all: fully lit sky
        result.fill(sharedUniformLight(15));
        return result;
    }
    // Up to 15 blocks above the highest non-empty section: block light from emitters
    // near its top reaches that far into the open air.
    r.y1 = std::min(kMaxY + 1, kMinY + (topSection + 1) * 16 + 15);
    // Sections that are solid opaque, non-emitting blocks in all 9 chunks hold no
    // light and pass none: start lighting above the highest run of them from the
    // bottom (usually just under the surface).
    int bottomSection = 0;
    auto solid = [&](BlockStateId st) { return reg.lightOpacity(st) >= 15 && !reg.lightEmission(st); };
    for (; bottomSection < topSection; ++bottomSection) {
        bool allSolid = true;
        for (const auto& chunk : n.sections)
            if (chunk[bottomSection]->isEmpty() || !chunk[bottomSection]->allPaletteStates(solid)) {
                allSolid = false;
                break;
            }
        if (!allSolid) break;
    }
    r.y0 = kMinY + bottomSection * 16;
    const int span = r.y1 - r.y0;
    r.opacity.assign(static_cast<size_t>(span) * kLayer, 0);
    r.sky.assign(r.opacity.size(), 0);
    r.block.assign(r.opacity.size(), 0);
    r.height.assign(kLayer, r.y0);
    r.queue.clear();

    // 2. Opacity and emitters from the 9 chunks (only the 46x46 region columns).
    for (int cz = 0; cz < 3; ++cz) {
        for (int cx = 0; cx < 3; ++cx) {
            const int ox = (cx - 1) * 16 + kMargin, oz = (cz - 1) * 16 + kMargin;
            const int lx0 = std::max(0, -ox), lx1 = std::min(16, kW - ox);
            const int lz0 = std::max(0, -oz), lz1 = std::min(16, kW - oz);
            if (lx0 >= lx1 || lz0 >= lz1) continue;
            for (int s = bottomSection; s <= topSection; ++s) {
                const Section& sec = *n.sections[cz * 3 + cx][s];
                if (sec.isEmpty()) continue; // opacity 0, no emitters
                sec.copyTo(sectionStates.data());
                const int baseY = kMinY + s * 16;
                for (int ly = 0; ly < 16; ++ly) {
                    const int y = baseY + ly;
                    if (y >= r.y1) break;
                    for (int lz = lz0; lz < lz1; ++lz) {
                        for (int lx = lx0; lx < lx1; ++lx) {
                            const BlockStateId st = sectionStates[Section::index(lx, ly, lz)];
                            const int i = r.index(ox + lx, y, oz + lz);
                            r.opacity[i] = reg.lightOpacity(st);
                            if (const uint8_t e = reg.lightEmission(st)) {
                                r.block[i] = e;
                                r.queue.push_back(static_cast<uint32_t>(i));
                            }
                        }
                    }
                }
            }
        }
    }

    auto spread = [&](std::vector<uint8_t>& light) {
        // Breadth-first: each cell passes light to its 6 neighbours, minus
        // max(1, opacity) - unless it's full sky light going straight down through a
        // transparent block (no loss).
        for (size_t q = 0; q < r.queue.size(); ++q) {
            const int i = static_cast<int>(r.queue[q]);
            const int level = light[i];
            if (level <= 1) continue;
            const int y = i / kLayer + r.y0, rem = i % kLayer, iz = rem / kW, ix = rem % kW;
            const int nbr[6][3] = {{ix - 1, y, iz}, {ix + 1, y, iz}, {ix, y, iz - 1},
                                   {ix, y, iz + 1}, {ix, y - 1, iz}, {ix, y + 1, iz}};
            for (int k = 0; k < 6; ++k) {
                const auto [nx, ny, nz] = nbr[k];
                if (nx < 0 || nx >= kW || nz < 0 || nz >= kW || ny < r.y0 || ny >= r.y1) continue;
                const int j = r.index(nx, ny, nz);
                const int o = r.opacity[j];
                if (o >= 15) continue;
                const bool straightDown = &light == &r.sky && k == 4 && level == 15 && o == 0;
                const int next = straightDown ? 15 : level - std::max(1, o);
                if (next > light[j]) {
                    light[j] = static_cast<uint8_t>(next);
                    r.queue.push_back(static_cast<uint32_t>(j));
                }
            }
        }
    };

    // 3. Block light from emitters.
    spread(r.block);

    // 4. Sky light: straight down each column from the top (15 until the first block
    //    with opacity; water etc. dim it 1 per block), then spread sideways/down.
    r.queue.clear();
    for (int iz = 0; iz < kW; ++iz) {
        for (int ix = 0; ix < kW; ++ix) {
            int level = 15;
            for (int y = r.y1 - 1; y >= r.y0 && level > 0; --y) {
                const int i = r.index(ix, y, iz);
                const int o = r.opacity[i];
                if (o >= 15) break;
                if (o > 0) {
                    // Only full sky light travels down for free: below a filtering
                    // block (water...) the reduced level spreads by BFS like any light.
                    r.sky[i] = static_cast<uint8_t>(std::max(0, level - std::max(1, o)));
                    break;
                }
                r.sky[i] = static_cast<uint8_t>(level);
            }
        }
    }
    // Seeds: lit cells next to a darker horizontal or lower neighbour.
    for (int y = r.y0; y < r.y1; ++y) {
        for (int iz = 0; iz < kW; ++iz) {
            for (int ix = 0; ix < kW; ++ix) {
                const int i = r.index(ix, y, iz);
                const int level = r.sky[i];
                if (level <= 1) continue;
                bool seed = false;
                if (ix > 0 && r.sky[i - 1] + 1 < level) seed = true;
                if (ix < kW - 1 && r.sky[i + 1] + 1 < level) seed = true;
                if (iz > 0 && r.sky[i - kW] + 1 < level) seed = true;
                if (iz < kW - 1 && r.sky[i + kW] + 1 < level) seed = true;
                if (y > r.y0 && r.sky[i - kLayer] + 1 < level) seed = true;
                if (seed) r.queue.push_back(static_cast<uint32_t>(i));
            }
        }
    }
    spread(r.sky);

    // 5. Copy the centre chunk out, section by section (uniform where possible;
    //    the common all-sky and all-dark sections share one immutable instance).
    for (int s = 0; s < kSectionsPerChunk; ++s) {
        const int baseY = kMinY + s * 16;
        if (baseY >= r.y1) {
            result[s] = sharedUniformLight(15);
            continue;
        }
        if (baseY < r.y0) {
            result[s] = sharedUniformLight(0);
            continue;
        }
        auto light = std::make_shared<SectionLight>();
        for (int ly = 0; ly < 16; ++ly) {
            const int y = baseY + ly;
            for (int lz = 0; lz < 16; ++lz) {
                for (int lx = 0; lx < 16; ++lx) {
                    const int si = Section::index(lx, ly, lz);
                    if (y >= r.y1) {
                        light->sky.set(si, 15);
                        continue;
                    }
                    const int i = r.index(lx + kMargin, y, lz + kMargin);
                    light->sky.set(si, r.sky[i]);
                    light->block.set(si, r.block[i]);
                }
            }
        }
        light->sky.compact();
        light->block.compact();
        if (light->block.isUniform() && light->block.uniformValue() == 0 && light->sky.isUniform() &&
            (light->sky.uniformValue() == 0 || light->sky.uniformValue() == 15))
            result[s] = sharedUniformLight(light->sky.uniformValue());
        else
            result[s] = std::move(light);
    }
    return result;
}

} // namespace mc::world
