#pragma once

#include "world/BlockEntity.h"
#include "world/Potions.h"
#include "world/World.h"

#include <array>

namespace mc {

// Beacons and conduits (M23.6; wiki: Beacon, Conduit).
//
// A beacon's pyramid: tier k (1..4) is a full (2k+1) x (2k+1) square of iron, gold,
// diamond, emerald or netherite blocks k blocks below it; tiers count from the top
// and stop at the first incomplete one. Its beam needs no opaque block above. Every
// 80 game ticks a lit beacon gives its effects to players within 10 + 10 x tiers
// blocks horizontally (and from that far below up to the sky) for 9 + 2 x tiers
// seconds: the primary at level I (level II when the secondary repeats it at tier
// 4), plus Regeneration I when that is the secondary.
int beaconTiers(const world::World& world, const world::BlockPos& p);
bool beaconSky(const world::World& world, const world::BlockPos& p);
// Whether an effect may be chosen as primary at these tiers (Speed, Haste: 1;
// Resistance, Jump Boost: 2; Strength: 3).
bool beaconPrimaryAllowed(world::Effect e, int tiers);
struct BeaconGift {
    world::Effect type = world::Effect::None;
    int amplifier = 0;
    int duration = 0; // ticks
};
// The effects a player standing at `feet` gets from this beacon now (0-2 entries).
int beaconGifts(const world::BeaconData& beacon, const world::BlockPos& p, const glm::dvec3& feet,
                std::array<BeaconGift, 2>& out);
// Payment items (wiki: Beacon › Usage).
bool isBeaconPayment(world::ItemId item);

// A conduit's frame: prismarine, prismarine bricks, dark prismarine and sea lanterns
// on the three 5x5 rings around it (42 places); it works with 16 or more and water in
// the 3x3x3 around it. Every 40 ticks it gives Conduit Power (13 s) to players in
// water or rain within 16 blocks per 7 frame blocks.
int conduitFrame(const world::World& world, const world::BlockPos& p);
bool conduitWet(const world::World& world, const world::BlockPos& p);
inline int conduitRange(int frame) { return frame / 7 * 16; }

} // namespace mc
