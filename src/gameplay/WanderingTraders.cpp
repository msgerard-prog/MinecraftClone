// Wandering traders (M24.4; wiki: Wandering Trader).
#include "gameplay/WanderingTraders.h"

#include "gameplay/Mobs.h"
#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

bool WanderingTraderSpawner::tick(World& world, const glm::dvec3& player, Xoroshiro& rng) {
    if (--delay > 0) return false;
    delay = 24000;
    if (int(rng.nextInt(100)) >= chance) {
        chance = std::min(75, chance + 25);
        return false;
    }
    if (rng.nextInt(10) != 0) return false; // (then 1 in 10: 2.5/5/7.5% a day - wiki)
    // Around a bell within 48 blocks if there is one (a quick look in the 3x3 chunks'
    // section palettes), else the player.
    glm::dvec3 centre = player;
    const ChunkPos pc{blockToChunk(int(std::floor(player.x))), blockToChunk(int(std::floor(player.z)))};
    bool found = false;
    for (int dz = -2; dz <= 2 && !found; ++dz)
        for (int dx = -2; dx <= 2 && !found; ++dx) {
            const Chunk* ch = world.chunk({pc.x + dx, pc.z + dz});
            if (!ch) continue;
            for (int si = 0; si < ch->sectionCount() && !found; ++si) {
                const Section& sec = ch->section(si);
                if (sec.isEmpty() || sec.allPaletteStates([](BlockStateId s) { return blockRegistry().blockOf(s) != blocks::Bell; }))
                    continue;
                for (int i = 0; i < 4096 && !found; ++i)
                    if (blockRegistry().blockOf(sec.getIndex(i)) == blocks::Bell) {
                        centre = {ch->pos().x * 16 + (i & 15) + 0.5, ch->height().minY + si * 16 + (i >> 8),
                                  ch->pos().z * 16 + ((i >> 4) & 15) + 0.5};
                        found = true;
                    }
            }
        }
    const auto& r = blockRegistry();
    for (int tries = 0; tries < 20; ++tries) {
        const int x = int(std::floor(centre.x)) + int(rng.nextInt(17)) - 8;
        const int z = int(std::floor(centre.z)) + int(rng.nextInt(17)) - 8;
        for (int y = int(std::floor(centre.y)) + 8; y >= int(std::floor(centre.y)) - 8; --y) {
            if (!r.collides(world.getBlock({x, y - 1, z}))) continue;
            if (r.collides(world.getBlock({x, y, z})) || r.collides(world.getBlock({x, y + 1, z}))) break;
            MobData trader = Mobs::make(MobType::WanderingTrader, {x + 0.5, double(y), z + 0.5}, rng);
            if (!Mobs::add(world, trader)) return false;
            // Two trader llamas come with it (wiki: Trader Llama; vanilla: on its leads).
            for (int i = 0; i < 2; ++i) {
                MobData llama = Mobs::make(MobType::TraderLlama, {x + 0.5 + (i ? 1.0 : -1.0), double(y), z + 0.5}, rng);
                llama.targetUuid = trader.uuidHi;
                llama.despawnDelay = trader.despawnDelay;
                Mobs::add(world, llama);
            }
            chance = 25;
            return true;
        }
    }
    return false;
}

} // namespace mc
