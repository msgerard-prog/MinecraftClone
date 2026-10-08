// Pillager patrols (M24.4; wiki: Patrol).
#include "gameplay/Patrols.h"

#include "gameplay/Mobs.h"
#include "world/Blocks.h"

#include <cmath>
#include <numbers>

namespace mc {

using namespace world;

int PatrolSpawner::tick(World& world, const glm::dvec3& player, int64_t dayTime, Xoroshiro& rng) {
    if (--delay > 0) return 0;
    delay = 12000 + int(rng.nextInt(1200));
    if (dayTime / 24000 < 5 || rng.nextInt(5) != 0) return 0;
    // A spot 24-48 blocks away on the surface (the highest block with room above).
    const double angle = rng.nextDouble() * 2.0 * std::numbers::pi,
                 dist = 24.0 + rng.nextDouble() * 24.0;
    const int cx = int(std::floor(player.x + std::cos(angle) * dist));
    const int cz = int(std::floor(player.z + std::sin(angle) * dist));
    const auto& r = blockRegistry();
    int spawned = 0;
    const int count = 1 + int(rng.nextInt(5));
    for (int i = 0; i < count; ++i) {
        const int x = cx + int(rng.nextInt(9)) - 4, z = cz + int(rng.nextInt(9)) - 4;
        if (!world.chunk(BlockPos{x, 0, z}.chunk())) continue;
        for (int y = world.height().maxY(); y > world.height().minY; --y) {
            const BlockStateId s = world.getBlock({x, y - 1, z});
            if (s == 0) continue;
            if (!r.collides(s) || r.blockOf(s) == blocks::Water || r.blockOf(s) == blocks::Lava)
                break;
            if (r.collides(world.getBlock({x, y + 1, z}))) break;
            MobData p = Mobs::make(MobType::Pillager, {x + 0.5, double(y), z + 0.5}, rng);
            p.captain = spawned == 0; // the first leads
            if (Mobs::add(world, p)) ++spawned;
            break;
        }
    }
    return spawned;
}

} // namespace mc
