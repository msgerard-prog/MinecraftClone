#pragma once

#include "gameplay/ExperienceOrbs.h"
#include "gameplay/Mobs.h"
#include "world/Random.h"
#include "world/World.h"

#include <cstdint>
#include <vector>

namespace mc {

// The dragon fight in an "end2" End (M20.2; wiki: Ender Dragon, Exit Portal, Dragon
// Egg). The first time the End is entered the dragon appears above the exit portal,
// which stays shut while it lives. When it dies: experience (12,000 the first time,
// 500 after), the exit portal opens, and the first kill leaves the dragon egg on top
// of the portal's column. Kept in level.dat as vanilla's DragonFight.
struct DragonFight {
    bool killed = false;
    bool previouslyKilled = false;
    uint64_t uuidHi = 0, uuidLo = 0; // the living dragon (0: none spawned yet)
    std::vector<int32_t> gateways;   // (M20.3)
    int missingScans = 0;            // scans near the portal that didn't find it

    // One game tick while the player is in the End. `edits` gets changed blocks.
    void tick(world::World& world, const Mobs& mobs, const glm::dvec3& playerPos, ExperienceOrbs& orbs,
              world::Xoroshiro& rng, std::vector<world::BlockPos>& edits);
    // Fills the exit portal (and, with `egg`, puts the egg on its column).
    static void openExitPortal(world::World& world, bool egg, std::vector<world::BlockPos>& edits);
    // The egg flees a click: up to 15 blocks away sideways and 7 up or down, into an
    // air block (wiki: Dragon Egg). False if no spot was found.
    static bool teleportEgg(world::World& world, const world::BlockPos& egg, world::Xoroshiro& rng,
                            std::vector<world::BlockPos>& edits);

private:
    int m_scanClock = 0;
};

} // namespace mc
