// Iron golems (M24.3; wiki: Iron Golem). Part of Mobs.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {
// Monsters a golem fights (wiki: Iron Golem › Behavior - every hostile mob but creepers).
bool golemTarget(const MobData& o) {
    if (o.health <= 0.0f || !mobInfo(o.type).hostile) return false;
    return o.type != MobType::Creeper && o.type != MobType::EnderDragon &&
           o.type != MobType::EndCrystal && o.type != MobType::Shulker && o.type != MobType::Ghast;
}
} // namespace

// The golem's goal: the nearest monster within 16 blocks (looked for about once a
// second, chased while within 32), else a stroll around where it came from (its
// village). Hits for 7.5 + 0-14 (7.5-21.5) and throws the target up (wiki). True: chasing.
bool Mobs::golemGoal(Context& ctx, MobData& g, double& speed) {
    MobData* t = g.targetUuid ? mobByUuid(ctx.world, g.pos, g.targetUuid) : nullptr;
    if (t && (!golemTarget(*t) || glm::length(t->pos - g.pos) > 32.0)) t = nullptr;
    if (!t) g.targetUuid = 0;
    if (!t && ctx.rng.nextInt(20) == 0) {
        double best = 16.0 * 16.0;
        const ChunkPos c{blockToChunk(int(std::floor(g.pos.x))),
                         blockToChunk(int(std::floor(g.pos.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (MobData& o : ch->mobs()) {
                        if (!golemTarget(o)) continue;
                        const double d = glm::dot(o.pos - g.pos, o.pos - g.pos);
                        if (d < best) best = d, t = &o;
                    }
        if (t) g.targetUuid = t->uuidHi;
    }
    if (t) {
        g.goal = t->pos;
        const double reach = mobInfo(g.type).width * 0.5 + mobInfo(t->type).width * 0.5 + 1.0;
        if (g.attackCooldown == 0 &&
            glm::length(glm::dvec2(t->pos.x - g.pos.x, t->pos.z - g.pos.z)) < reach &&
            std::abs(t->pos.y - g.pos.y) < 2.0) {
            g.attackCooldown = 20;
            t->health -= mobInfo(g.type).attackDamage + float(ctx.rng.nextInt(15));
            t->hurtTime = 10;
            t->vel.y += 0.4; // (thrown up)
            t->lastHurtByPlayer = false;
        }
        return true;
    }
    // Patrol around home (where it was made), slowly (wiki: golems wander at 0.6).
    speed *= 0.6;
    const glm::dvec3 home =
        g.home.y != kNoPoint ? glm::dvec3(g.home.x + 0.5, g.home.y, g.home.z + 0.5) : g.pos;
    if (++g.goalTicks > 240 ||
        glm::length(glm::dvec2(g.goal.x - g.pos.x, g.goal.z - g.pos.z)) < 0.8) {
        if (ctx.rng.nextInt(80) == 0) {
            g.goal = home + glm::dvec3(ctx.rng.nextDouble() * 32 - 16, 0.0,
                                       ctx.rng.nextDouble() * 32 - 16);
            g.goalTicks = 0;
        } else if (g.goalTicks > 240) {
            g.goal = g.pos;
        }
    }
    return false;
}

// Villagers call a golem when three or more are together and none is around (ours: a
// roll about every 30 s per villager with a bed; wiki: Iron Golem › Spawning - gossip
// and panic based). It appears on solid ground up to 8 blocks away.
void Mobs::villagersCallGolem(Context& ctx, MobData& v) {
    if (v.isBaby() || v.home.y == kNoPoint || ctx.rng.nextInt(600) != 0) return;
    int villagers = 0;
    const ChunkPos c{blockToChunk(int(std::floor(v.pos.x))),
                     blockToChunk(int(std::floor(v.pos.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& o : ch->mobs()) {
                    const double d = glm::dot(o.pos - v.pos, o.pos - v.pos);
                    if (o.type == MobType::IronGolem && o.health > 0.0f && d < 16.0 * 16.0) return;
                    if (o.type == MobType::Villager && o.health > 0.0f && d < 10.0 * 10.0)
                        ++villagers;
                }
    for (const MobData& b : m_births)
        if (b.type == MobType::IronGolem && glm::length(b.pos - v.pos) < 16.0) return;
    if (villagers < 3) return;
    for (int tries = 0; tries < 10; ++tries) {
        const int x = int(std::floor(v.pos.x)) + int(ctx.rng.nextInt(17)) - 8;
        const int z = int(std::floor(v.pos.z)) + int(ctx.rng.nextInt(17)) - 8;
        for (int y = int(std::floor(v.pos.y)) + 6; y >= int(std::floor(v.pos.y)) - 6; --y) {
            const auto& r = blockRegistry();
            if (!r.collides(ctx.world.getBlock({x, y - 1, z}))) continue;
            bool clear = true;
            for (int k = 0; k < 3 && clear; ++k)
                clear = !r.collides(ctx.world.getBlock({x, y + k, z}));
            if (!clear) break;
            MobData golem = make(MobType::IronGolem, {x + 0.5, double(y), z + 0.5}, ctx.rng);
            golem.home = {x, y, z};
            if (m_births.size() < m_births.capacity()) m_births.push_back(golem);
            return;
        }
    }
}

// Four iron blocks in a T with a carved pumpkin on top make an iron golem (wiki: Iron
// Golem › Creation): the blocks go, a golem stands where the base was.
bool Mobs::buildIronGolem(World& world, const BlockPos& pumpkin, Xoroshiro& rng) {
    const auto& r = blockRegistry();
    auto iron = [&](int x, int y, int z) {
        return r.blockOf(world.getBlock({x, y, z})) == blocks::IronBlock;
    };
    if (r.blockOf(world.getBlock(pumpkin)) != blocks::CarvedPumpkin) return false;
    const int x = pumpkin.x, y = pumpkin.y, z = pumpkin.z;
    if (!iron(x, y - 1, z) || !iron(x, y - 2, z)) return false;
    for (const auto& [ax, az] : {std::pair{1, 0}, std::pair{0, 1}}) {
        if (!iron(x + ax, y - 1, z + az) || !iron(x - ax, y - 1, z - az)) continue;
        for (const BlockPos& b : {pumpkin, BlockPos{x, y - 1, z}, BlockPos{x, y - 2, z},
                                  BlockPos{x + ax, y - 1, z + az}, BlockPos{x - ax, y - 1, z - az}})
            world.updateBlock(b, 0);
        MobData golem = make(MobType::IronGolem, {x + 0.5, double(y - 2), z + 0.5}, rng);
        golem.playerCreated = true; // (it never attacks the player, even when hit - wiki)
        golem.home = {x, y - 2, z};
        add(world, golem);
        return true;
    }
    return false;
}

} // namespace mc
