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
    out.hasSkyLight = world.hasSkyLight();
    out.height = world.height();
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const Chunk* c = world.chunk({center.x + dx, center.z + dz});
            if (!c) return false;
            auto& dst = out.sections[(dz + 1) * 3 + (dx + 1)];
            for (int s = 0; s < c->sectionCount(); ++s)
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
        for (int s = n.height.sections() - 1; s > topSection; --s)
            if (!chunk[s]->isEmpty()) topSection = s;
    ChunkLight result;
    const uint8_t open = n.hasSkyLight ? 15 : 0; // light of the open air above everything
    if (topSection < 0) { // nothing at all: fully lit sky
        result.fill(sharedUniformLight(open));
        return result;
    }
    // Up to 15 blocks above the highest non-empty section: block light from emitters
    // near its top reaches that far into the open air.
    const int minY = n.height.minY;
    r.y1 = std::min(n.height.maxY() + 1, minY + (topSection + 1) * 16 + 15);
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
    r.y0 = minY + bottomSection * 16;
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
                const int baseY = minY + s * 16;
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
    //    None at all in dimensions without sky light (r.sky stays 0).
    r.queue.clear();
    for (int iz = 0; iz < kW && n.hasSkyLight; ++iz) {
        for (int ix = 0; ix < kW; ++ix) {
            int level = open;
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
    for (int y = r.y0; y < r.y1 && n.hasSkyLight; ++y) {
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
    for (int s = 0; s < n.height.sections(); ++s) {
        const int baseY = minY + s * 16;
        if (baseY >= r.y1) {
            result[s] = sharedUniformLight(open);
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
                        light->sky.set(si, open);
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

void updateLightIncremental(const IncrementalLightInput& in, IncrementalLightOutput& out) {
    const auto& reg = blockRegistry();
    const HeightRange& h = in.blocks.height;
    const int sections = h.sections();
    const int ox = (in.blocks.center.x - 1) * 16, oz = (in.blocks.center.z - 1) * 16; // region corner
    // Working copies of the sections written to, made on first write (worker thread).
    static thread_local std::array<std::array<std::shared_ptr<SectionLight>, kMaxSections>, 9> copies;
    for (auto& c : copies) c.fill(nullptr);
    out.light = in.light;
    out.changed.fill(0);

    struct Cell {
        int chunk = -1, section = 0, index = 0;
    };
    auto locate = [&](int x, int y, int z) -> Cell {
        const int rx = x - ox, rz = z - oz;
        if (rx < 0 || rx >= 48 || rz < 0 || rz >= 48 || !h.contains(y)) return {};
        return {(rz >> 4) * 3 + (rx >> 4), h.sectionIndex(y), Section::index(rx & 15, blockToLocal(y), rz & 15)};
    };
    auto stateAt = [&](const Cell& c) -> BlockStateId {
        const auto& sec = in.blocks.sections[size_t(c.chunk)][size_t(c.section)];
        return sec ? sec->getIndex(c.index) : 0;
    };
    auto lightOf = [&](const Cell& c, bool sky) -> uint8_t {
        const SectionLight* l = copies[size_t(c.chunk)][size_t(c.section)].get();
        if (!l) l = in.light[size_t(c.chunk)][size_t(c.section)].get();
        if (!l) return 0;
        return sky ? l->sky.get(c.index) : l->block.get(c.index);
    };
    auto setLight = [&](const Cell& c, bool sky, uint8_t v) {
        auto& copy = copies[size_t(c.chunk)][size_t(c.section)];
        if (!copy) {
            const auto& orig = in.light[size_t(c.chunk)][size_t(c.section)];
            copy = orig ? std::make_shared<SectionLight>(*orig) : std::make_shared<SectionLight>();
        }
        (sky ? copy->sky : copy->block).set(c.index, v);
    };
    struct Entry {
        int x, y, z;
        uint8_t level;
    };
    static thread_local std::vector<Entry> decrease, increase;
    static constexpr int kDirs[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}};
    const int top = h.maxY();

    auto channel = [&](bool sky) {
        decrease.clear();
        increase.clear();
        // 1. At each edited block: take its light away (or pull the neighbours' in).
        for (const BlockPos& p : in.edits) {
            const Cell c = locate(p.x, p.y, p.z);
            if (c.chunk < 0) continue;
            const BlockStateId st = stateAt(c);
            const int emission = sky ? 0 : reg.lightEmission(st);
            const int level = lightOf(c, sky);
            if (emission < level) {
                setLight(c, sky, 0);
                decrease.push_back({p.x, p.y, p.z, uint8_t(level)});
            } else {
                decrease.push_back({p.x, p.y, p.z, 0}); // (pull the neighbours' light in)
            }
            if (emission > 0) {
                setLight(c, sky, uint8_t(emission));
                increase.push_back({p.x, p.y, p.z, uint8_t(emission)});
            }
        }
        // 2. Decrease: dimmer neighbours lit by a removed cell lose their light (and are
        //    followed); brighter ones are sources to fill back from. Sky light 15 going
        //    straight down came from above: it goes too.
        for (size_t q = 0; q < decrease.size(); ++q) {
            const Entry e = decrease[q];
            for (int k = 0; k < 6; ++k) {
                const int nx = e.x + kDirs[k][0], ny = e.y + kDirs[k][1], nz = e.z + kDirs[k][2];
                if (sky && ny > top) { // open sky above the world: a source
                    increase.push_back({nx, ny, nz, 15});
                    continue;
                }
                const Cell n = locate(nx, ny, nz);
                if (n.chunk < 0) continue;
                const int nl = lightOf(n, sky);
                if (nl == 0) continue;
                const bool fromHere = e.level > 0 && (nl < e.level || (sky && k == 4 && e.level == 15 && nl == 15));
                if (fromHere) {
                    const int emission = sky ? 0 : reg.lightEmission(stateAt(n));
                    setLight(n, sky, 0);
                    decrease.push_back({nx, ny, nz, uint8_t(nl)});
                    if (emission > 0) {
                        setLight(n, sky, uint8_t(emission));
                        increase.push_back({nx, ny, nz, uint8_t(emission)});
                    }
                } else {
                    increase.push_back({nx, ny, nz, uint8_t(nl)});
                }
            }
        }
        // 3. Increase: spread from the sources, max(1, opacity) lost a step (sky light 15
        //    straight down through clear blocks loses nothing).
        for (size_t q = 0; q < increase.size(); ++q) {
            const Entry e = increase[q];
            // From what the cell holds now (a source may have been dimmed since it was
            // queued); the open sky above the world is always 15.
            int level = 15;
            if (!(sky && e.y > top)) {
                const Cell here = locate(e.x, e.y, e.z);
                if (here.chunk < 0) continue;
                level = lightOf(here, sky);
            }
            if (level <= 1) continue;
            for (int k = 0; k < 6; ++k) {
                const int nx = e.x + kDirs[k][0], ny = e.y + kDirs[k][1], nz = e.z + kDirs[k][2];
                const Cell n = locate(nx, ny, nz);
                if (n.chunk < 0) continue;
                const int o = reg.lightOpacity(stateAt(n));
                if (o >= 15) continue;
                const int next = sky && k == 4 && level == 15 && o == 0 ? 15 : level - std::max(1, o);
                if (next > lightOf(n, sky)) {
                    setLight(n, sky, uint8_t(next));
                    increase.push_back({nx, ny, nz, uint8_t(next)});
                }
            }
        }
    };
    channel(false);
    if (in.blocks.hasSkyLight) channel(true);

    // 4. The changed sections, compacted (shared uniform instances where possible).
    for (int ci = 0; ci < 9; ++ci)
        for (int sct = 0; sct < sections; ++sct) {
            auto& copy = copies[size_t(ci)][size_t(sct)];
            if (!copy) continue;
            copy->sky.compact();
            copy->block.compact();
            const auto& orig = in.light[size_t(ci)][size_t(sct)];
            if (orig && *orig == *copy) {
                copy.reset();
                continue;
            }
            if (copy->block.isUniform() && copy->block.uniformValue() == 0 && copy->sky.isUniform() &&
                (copy->sky.uniformValue() == 0 || copy->sky.uniformValue() == 15))
                out.light[size_t(ci)][size_t(sct)] = sharedUniformLight(copy->sky.uniformValue());
            else
                out.light[size_t(ci)][size_t(sct)] = std::move(copy);
            copy.reset();
            out.changed[size_t(ci)] |= 1u << sct;
        }
}

} // namespace mc::world
