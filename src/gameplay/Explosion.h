#pragma once

#include "gameplay/ItemEntities.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc {

// Explosions (M16.4; wiki: Explosion). Rays from the centre to the 1352 surface points
// of a 16x16x16 grid, each with intensity power x (0.7..1.3); every 0.3 blocks the
// intensity drops by 0.225, and by (blast resistance + 0.3) x 0.3 in a non-air block,
// which is destroyed while intensity stays above 0. Destroyed blocks drop their loot
// with a 1/power chance (mob explosions). Entities within 2 x power blocks take
// (impact^2 + impact)/2 x 7 x 2 x power + 1 damage and are pushed by impact, where
// impact = (1 - distance / (2 x power)) x exposure (the share of rays from their box
// to the centre that no block stops).
struct ExplosionTargets {
    Player* player = nullptr; // with its vitals, if it can be hurt (survival)
    Vitals* vitals = nullptr;
    bool damageMobs = true;
    // TNT (M21.1b): blasts light TNT blocks (a 10-30 tick fuse) and push primed TNT;
    // TNT's own blasts drop every block they break (1.21 game rule tnt_explosion_drop_decay
    // false), mob blasts 1 in power.
    class PrimedTnt* tnt = nullptr;
    bool dropAll = false;
    // Game rules (M28.1): mob_griefing off - a mob's blast breaks nothing; block_drops off -
    // broken blocks drop nothing.
    bool breakBlocks = true;
    bool blockDrops = true;
};

class Explosion {
public:
    Explosion() {
        m_hits.reserve(16384); // ~1352 rays x up to ~8 cells each in soft blocks
        m_loot.reserve(16);
    }
    // Destroys blocks (through World::updateBlock; positions into `changed`), spawns
    // their drops into `items`, damages and pushes the player and mobs. Returns the
    // number of blocks destroyed.
    int explode(world::World& world, const glm::dvec3& centre, float power, world::Xoroshiro& rng,
                ItemEntities& items, std::vector<world::BlockPos>& changed, const ExplosionTargets& targets);

    // The share (0..1) of sample rays from `box` to `centre` no block stops (public for tests).
    static double exposure(const world::World& world, const glm::dvec3& centre, const Aabb& box);

private:
    std::vector<world::BlockPos> m_hits;
    std::vector<world::ItemStack> m_loot;
};

} // namespace mc
