#pragma once

#include "gameplay/Aabb.h"
#include "world/Dimension.h"
#include "world/Direction.h"
#include "world/Items.h"
#include "world/World.h"

#include <optional>
#include <vector>

namespace mc {

// Nether and End portals (M12; wiki: Nether portal, End portal, End Portal Frame).
namespace portals {

// Lighting a portal (flint and steel used on `p`, an air block): if `p` lies inside a
// rectangular obsidian frame (inside 2..21 wide, 3..21 tall, corners optional), along
// x or z, it fills with nether_portal blocks. Returns the bottom corner of the inside.
std::optional<world::BlockPos> light(world::World& world, const world::BlockPos& p, std::vector<world::BlockPos>& changed);

// A known portal near `target` (horizontal distance <= radius): vanilla searches 128
// blocks in the Overworld and 16 in the Nether. Stale entries (no portal block there
// any more, in a loaded chunk) are removed.
struct Known {
    world::Dimension dimension;
    world::BlockPos pos;
};
std::optional<world::BlockPos> find(const world::World& world, std::vector<Known>& known, world::Dimension dim,
                                    const world::BlockPos& target, int radius);

// Builds a 4x5 obsidian portal (2x3 inside, along x) near `target`: on solid ground
// with room within 16 blocks if there is some, else at the target height on an
// obsidian platform with the space cleared (wiki: Nether portal › Portal search).
// Returns the bottom corner of the inside. `minY..maxY` bound where it may stand.
world::BlockPos build(world::World& world, const world::BlockPos& target, int minY, int maxY,
                      std::vector<world::BlockPos>& changed);

// Where a portal at `pos` in `from` leads in `to` (wiki: Nether portal - the
// Nether's coordinates are 1/8 of the Overworld's).
world::BlockPos destination(world::Dimension from, world::Dimension to, const world::BlockPos& pos);

// True if `box` overlaps a block of `block`.
bool touching(const world::World& world, const Aabb& box, world::BlockId block);

// The End's arrival platform: 5x5 obsidian at (98..102, 48, -2..2) with 3 blocks of
// air above (wiki: Obsidian platform); returns the arrival feet position.
glm::dvec3 endPlatform(world::World& world, std::vector<world::BlockPos>& changed);

// An eye placed in a frame: when all 12 frames around a 3x3 space have eyes, the
// space fills with end_portal (wiki: End Portal Frame). Returns true if it opened.
bool completeEndPortal(world::World& world, const world::BlockPos& frame, std::vector<world::BlockPos>& changed);

// Right-click with flint and steel or a fire charge (fire in front of the clicked face, or a portal when
// that fire would be inside an obsidian frame - not in the End) or an eye of ender
// (into an empty frame). `changed` gets edited positions. Returns true if the item was used
// (the caller wears the flint and steel or uses up the eye).
bool useItem(world::World& world, world::Dimension dimension, world::ItemId item, const world::BlockPos& block, world::Direction face,
             std::vector<world::BlockPos>& changed);

} // namespace portals

} // namespace mc
