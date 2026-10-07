#include "world/Weather.h"

#include "world/Biome.h"
#include "world/Blocks.h"

#include <algorithm>

namespace mc::world {

void Weather::tick(Xoroshiro& rng) {
    if (clearTime > 0) { // /weather clear: no changes until it runs out
        --clearTime;
        raining = thundering = false;
        rainTime = thunderTime = 0;
    } else {
        // Each countdown flips its state when it reaches 0, then draws the next length.
        if (thunderTime > 0) {
            if (--thunderTime == 0) thundering = !thundering;
        } else {
            thunderTime = thundering ? 3600 + int(rng.nextInt(12000)) : 12000 + int(rng.nextInt(168000));
        }
        if (rainTime > 0) {
            if (--rainTime == 0) raining = !raining;
        } else {
            rainTime = raining ? 12000 + int(rng.nextInt(12000)) : 12000 + int(rng.nextInt(168000));
        }
    }
    prevRain = rain;
    prevThunder = thunder;
    rain = std::clamp(rain + (raining ? 0.01f : -0.01f), 0.0f, 1.0f);
    thunder = std::clamp(thunder + (raining && thundering ? 0.01f : -0.01f), 0.0f, 1.0f);
}

void Weather::set(Kind kind, int duration) {
    clearTime = kind == Kind::Clear ? duration : 0;
    raining = kind != Kind::Clear;
    thundering = kind == Kind::Thunder;
    rainTime = kind == Kind::Clear ? 0 : duration;
    thunderTime = kind == Kind::Clear ? 0 : duration;
}

Precipitation precipitationAt(const World& world, const BlockPos& p) {
    const Chunk* c = world.chunk(p.chunk());
    if (!c || !c->biomes() || !world.hasSkyLight() || world.isUltrawarm()) return Precipitation::None;
    const Biome b = c->biomes()->at(blockToLocal(p.x), std::clamp(p.y, world.height().minY, world.height().maxY()),
                                    blockToLocal(p.z), world.height());
    if (b == Biome::TheEnd || (b >= Biome::EndHighlands && b <= Biome::EndBarrens)) return Precipitation::None;
    if (biomeInfo(b).temperature >= 1.5f) return Precipitation::None;
    const float t = biomeInfo(b).temperature - 0.00125f * float(std::max(0, p.y - 80));
    return t < 0.15f ? Precipitation::Snow : Precipitation::Rain;
}

int rainHeight(const World& world, int32_t x, int32_t z) {
    const Chunk* c = world.chunk({blockToChunk(x), blockToChunk(z)});
    const HeightRange h = world.height();
    if (!c) return h.minY;
    const auto& r = blockRegistry();
    const int lx = blockToLocal(x), lz = blockToLocal(z);
    // Top-down by section, skipping empty ones (drawn for ~400 columns a frame).
    for (int si = c->sectionCount() - 1; si >= 0; --si) {
        const Section& sec = c->section(si);
        if (sec.nonAirCount() == 0) continue;
        for (int ly = 15; ly >= 0; --ly) {
            const BlockStateId s = sec.get(lx, ly, lz);
            if (s == 0) continue;
            const BlockId b = r.blockOf(s);
            if (r.collides(s) || b == blocks::Water || b == blocks::Lava) return c->height().minY + si * 16 + ly + 1;
        }
    }
    return h.minY;
}

bool rainingAt(const World& world, const Weather& weather, const BlockPos& p) {
    if (!weather.raining) return false;
    if (rainHeight(world, p.x, p.z) > p.y) return false;
    return precipitationAt(world, p) == Precipitation::Rain;
}

} // namespace mc::world
