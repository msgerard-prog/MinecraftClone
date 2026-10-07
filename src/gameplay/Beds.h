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
    NotABed,
};

// Night for sleeping: day time 12542..23459 in clear weather (wiki: Bed).
bool canSleepAt(int64_t dayTime);

// What happens when the player uses the bed block at `p` (either half).
BedUse useBed(const world::World& world, const world::BlockPos& p, int64_t dayTime, world::Dimension dimension);

// The bed's head half (where the player lies and the spawn point is kept).
std::optional<world::BlockPos> bedHead(const world::World& world, const world::BlockPos& p);

// Where to stand up / respawn next to the bed: a spot around either half with solid
// ground and 2 free blocks (wiki: Bed › Respawn); nothing if it is gone or blocked.
std::optional<glm::dvec3> bedStandSpot(const world::World& world, const world::BlockPos& head);

// The night skipped by sleeping: the next morning (day time a multiple of 24000).
inline int64_t morningAfter(int64_t dayTime) { return dayTime + (24000 - dayTime % 24000); }

} // namespace mc
