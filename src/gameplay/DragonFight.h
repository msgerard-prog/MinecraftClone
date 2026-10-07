#pragma once

#include "gameplay/ExperienceOrbs.h"
#include "gameplay/Mobs.h"
#include "world/NetherGenerator.h"
#include "world/Random.h"
#include "world/World.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace mc {

// The dragon fight in an "end2" End (M20.2; wiki: Ender Dragon, Exit Portal, Dragon
// Egg). The first time the End is entered the dragon appears above the exit portal,
// which stays shut while it lives. When it dies: experience (12,000 the first time,
// 500 after), the exit portal opens, and the first kill leaves the dragon egg on top
// of the portal's column. Kept in level.dat as vanilla's DragonFight.
//
// Gateways (M20.3; wiki: End Gateway): each kill opens one of 20 gateways on a ring 96
// blocks out at Y 75, in a random order. One leads out along its direction to the first
// outer island beyond 1024 blocks, where an exit gateway is built 10 above the ground;
// that one leads back to the main island. Respawning (wiki: Ender Dragon › Respawning):
// an end crystal on each of the four sides of the open exit portal rebuilds the pillars
// (with their crystals and cages), shuts the portal and brings a new dragon.
struct DragonFight {
    bool killed = false;
    bool previouslyKilled = false;
    uint64_t uuidHi = 0, uuidLo = 0; // the living dragon (0: none spawned yet)
    // Gateways still to open (vanilla's Gateways: the 20 indices shuffled when the
    // fight starts, taken from the front). `gatewaysReady`: the list was made.
    std::vector<int32_t> gateways;
    bool gatewaysReady = false;
    int missingScans = 0; // scans near the portal that didn't find it

    // One game tick while the player is in the End. `edits` gets changed blocks.
    void tick(world::World& world, const world::EndGenerator& gen, const Mobs& mobs, const glm::dvec3& playerPos,
              ExperienceOrbs& orbs, world::Xoroshiro& rng, std::vector<world::BlockPos>& edits);
    // Gateway number `i` (0..19) on the ring.
    static world::BlockPos gatewayPos(int i);
    // Where an end gateway at `gateway` sends the player (feet). Going out, it also
    // asks for an exit gateway at `exitGateway` (built once its chunk is loaded).
    std::optional<glm::dvec3> gatewayTarget(const world::EndGenerator& gen, const world::BlockPos& gateway);
    // Builds an end gateway with its bedrock caps (vanilla's look: a plus of bedrock
    // above and below).
    static void buildGateway(world::World& world, const world::BlockPos& at, std::vector<world::BlockPos>& edits);
    // Fills the exit portal (and, with `egg`, puts the egg on its column).
    static void openExitPortal(world::World& world, bool egg, std::vector<world::BlockPos>& edits);
    // The egg flees a click: up to 15 blocks away sideways and 7 up or down, into an
    // air block (wiki: Dragon Egg). False if no spot was found.
    static bool teleportEgg(world::World& world, const world::BlockPos& egg, world::Xoroshiro& rng,
                            std::vector<world::BlockPos>& edits);

private:
    int m_scanClock = 0;
    int m_respawnTicks = -1; // the respawn under way (not saved: it starts over)
    std::optional<world::BlockPos> m_pendingExit;
    void respawnStep(world::World& world, const world::EndGenerator& gen, world::Xoroshiro& rng,
                     std::vector<world::BlockPos>& edits);
};

} // namespace mc
