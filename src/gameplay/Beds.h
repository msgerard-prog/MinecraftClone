#pragma once

#include "world/Dimension.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <optional>

namespace mc {

// Beds (M17.4; wiki: Bed). Right-clicking a bed sets the respawn point and, at night
// with no monsters near, puts the player to sleep; after 100 ticks asleep the night
// is skipped. In the Nether and the End the bed explodes instead.
enum class BedUse {
    Sleep,       // set spawn and sleep
    NotNight,    // "You can sleep only at night" (spawn still set)
    Monsters,    // "You may not rest now; there are monsters nearby"
    Occupied,    // "This bed is occupied"
    Explodes,    // not the Overworld
    TooFar,      // "You can't rest now; the bed is too far away"
    Obstructed,  // "This bed is obstructed"
    NotABed,
};

// Night for sleeping (wiki: Bed): day time 12523..23477 in clear weather,
// 12002..23998 while it rains, any time in a thunderstorm.
bool canSleepAt(int64_t dayTime, bool raining = false, bool thundering = false);

// What happens when the player uses the bed block at `p` (either half).
// `creative`: monsters don't keep a creative player awake. `player`: the player's
// feet - too far (more than 3 blocks) or a solid block over the head stops it.
BedUse useBed(const world::World& world, const world::BlockPos& p, int64_t dayTime, world::Dimension dimension,
              bool creative = false, const glm::dvec3* player = nullptr, bool raining = false,
              bool thundering = false);

// The bed's head half (where the player lies and the spawn point is kept).
std::optional<world::BlockPos> bedHead(const world::World& world, const world::BlockPos& p);

// Where to stand up / respawn next to the bed: a spot around either half with solid
// ground and 2 free blocks (wiki: Bed › Respawn); nothing if it is gone or blocked.
std::optional<glm::dvec3> bedStandSpot(const world::World& world, const world::BlockPos& head);

// The night skipped by sleeping: the next morning (day time a multiple of 24000).
inline int64_t morningAfter(int64_t dayTime) { return dayTime + (24000 - dayTime % 24000); }

} // namespace mc
