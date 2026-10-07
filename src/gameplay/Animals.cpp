// Farm animals (M16.3; wiki: Breeding, Sheep, Pig, Chicken, Cow). Part of Mobs.
#include "gameplay/Mobs.h"

#include "world/Blocks.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

ItemId itemId(const char* name) { return *itemRegistry().find(name); }

} // namespace

bool Mobs::isFood(MobType type, ItemId item) {
    // Breeding foods (wiki: Breeding): wheat for cows and sheep, carrots for pigs,
    // seeds for chickens (others not added yet).
    static const ItemId wheat = itemId("wheat"), carrot = itemId("carrot"), seeds = itemId("wheat_seeds");
    switch (type) {
    case MobType::Cow:
    case MobType::Sheep: return item == wheat;
    case MobType::Pig: return item == carrot;
    case MobType::Chicken: return item == seeds;
    default: return false;
    }
}

Mobs::Use Mobs::interact(MobData& m, ItemId held, Xoroshiro& rng, ItemEntities& items) {
    if (m.health <= 0.0f) return Use::None;
    static const ItemId shears = itemId("shears");
    if (held == shears && m.type == MobType::Sheep && !m.sheared && !m.isBaby()) {
        // Shearing drops 1-3 wool of its colour (wiki: Sheep › Shearing).
        m.sheared = true;
        const int n = 1 + static_cast<int>(rng.nextInt(3));
        items.spawn(m.pos + glm::dvec3(0, 1, 0),
                    {itemRegistry().blockItem(static_cast<BlockId>(blocks::WhiteWool + m.woolColour)), uint8_t(n)},
                    rng);
        return Use::Sheared;
    }
    if (!isFood(m.type, held)) return Use::None;
    if (m.isBaby()) { // feeding a baby speeds its growth by 10% of the time left
        m.age += -m.age / 10;
        return Use::Fed;
    }
    if (m.age == 0 && m.loveTicks == 0) {
        m.loveTicks = 600; // love mode for 30 s
        return Use::Fed;
    }
    return Use::None;
}

MobData* Mobs::findMob(World& world, const MobData& self, double range, bool wantLove, bool wantAdult) {
    // The nearest other mob of the same kind within `range` (3x3 chunks around).
    MobData* best = nullptr;
    double bestD = range * range;
    const ChunkPos c{blockToChunk(int(std::floor(self.pos.x))), blockToChunk(int(std::floor(self.pos.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            Chunk* ch = world.chunk({c.x + dx, c.z + dz});
            if (!ch) continue;
            for (MobData& o : ch->mobs()) {
                if (&o == &self || o.type != self.type || o.health <= 0.0f) continue;
                if (wantLove && (o.loveTicks == 0 || o.isBaby())) continue;
                if (wantAdult && o.isBaby()) continue;
                const double d = glm::dot(o.pos - self.pos, o.pos - self.pos);
                if (d < bestD) {
                    bestD = d;
                    best = &o;
                }
            }
        }
    return best;
}

void Mobs::animalUpkeep(Context& ctx, MobData& m) {
    if (m.age < 0) ++m.age;      // babies grow up in 20 minutes
    else if (m.age > 0) --m.age; // breeding cooldown (5 minutes)
    if (m.loveTicks > 0) --m.loveTicks;
    // Chickens lay an egg every 5-10 minutes (wiki: Chicken), adults only.
    if (m.type == MobType::Chicken && !m.isBaby() && --m.eggTicks <= 0) {
        static const ItemId egg = itemId("egg");
        ctx.items.spawn(m.pos + glm::dvec3(0, 0.3, 0), {egg, 1}, ctx.rng);
        m.eggTicks = 6000 + static_cast<int>(ctx.rng.nextInt(6000));
    }
    // Sheep graze (wiki: Sheep › Eating): now and then (adults 1 in 1000 ticks, lambs
    // 1 in 50) on short grass or a grass block; after a 40-tick animation the grass is
    // eaten (grass block -> dirt), the wool grows back and lambs grow 1 minute.
    if (m.type == MobType::Sheep) {
        const BlockPos feet{int(std::floor(m.pos.x)), int(std::floor(m.pos.y + 0.01)), int(std::floor(m.pos.z))};
        const BlockPos ground{feet.x, feet.y - 1, feet.z};
        const auto& reg = blockRegistry();
        if (m.eatTicks > 0) {
            if (--m.eatTicks == 4) {
                bool ate = false;
                if (reg.blockOf(ctx.world.getBlock(feet)) == blocks::ShortGrass) {
                    ctx.world.updateBlock(feet, 0);
                    if (ctx.edits) ctx.edits->push_back(feet);
                    ate = true;
                } else if (reg.blockOf(ctx.world.getBlock(ground)) == blocks::GrassBlock) {
                    ctx.world.updateBlock(ground, reg.defaultState(blocks::Dirt));
                    if (ctx.edits) ctx.edits->push_back(ground);
                    ate = true;
                }
                if (ate) {
                    m.sheared = false;
                    if (m.isBaby()) m.age = std::min(0, m.age + 1200);
                }
            }
        } else if (ctx.rng.nextInt(m.isBaby() ? 50 : 1000) == 0 &&
                   (reg.blockOf(ctx.world.getBlock(feet)) == blocks::ShortGrass ||
                    reg.blockOf(ctx.world.getBlock(ground)) == blocks::GrassBlock)) {
            m.eatTicks = 40;
        }
    }
}

bool Mobs::animalGoal(Context& ctx, MobData& m, double& speed) {
    if (m.eatTicks > 0) { // standing still while grazing
        m.goal = m.pos;
        return true;
    }
    // In love: walk to the nearest partner in love; after 3 s side by side, a baby.
    if (m.loveTicks > 0 && !m.isBaby()) {
        if (MobData* partner = findMob(ctx.world, m, 8.0, true, true)) {
            m.goal = partner->pos;
            if (glm::length(partner->pos - m.pos) < 3.0) {
                if (++m.breedTicks >= 60) {
                    MobData baby = make(m.type, (m.pos + partner->pos) * 0.5, ctx.rng);
                    baby.age = -24000;
                    baby.persistent = true;
                    if (m.type == MobType::Sheep) // a lamb takes a parent's colour (mixing: later)
                        baby.woolColour = ctx.rng.nextInt(2) ? m.woolColour : partner->woolColour;
                    m_births.push_back(baby);
                    for (MobData* parent : {&m, partner}) {
                        parent->loveTicks = 0;
                        parent->breedTicks = 0;
                        parent->age = 6000; // 5 minutes before breeding again
                    }
                }
            }
            return true;
        }
    }
    // Tempted: follow a player holding its food within 10 blocks, stopping 2.5 short.
    {
        const glm::dvec3 p = ctx.player.position();
        const double d = glm::length(p - m.pos);
        if (!ctx.playerDead && d < 10.0 && isFood(m.type, ctx.heldItem)) {
            m.goal = d > 2.5 ? p : m.pos;
            return true;
        }
    }
    // Babies keep near an adult of their kind (wiki: Breeding › Baby).
    if (m.isBaby()) {
        if (MobData* parent = findMob(ctx.world, m, 8.0, false, true)) {
            if (glm::length(parent->pos - m.pos) > 3.0) {
                m.goal = parent->pos;
                speed *= 1.1;
                return true;
            }
        }
    }
    return false;
}

} // namespace mc
