#pragma once

#include "world/Items.h"
#include "world/Random.h"

#include <vector>

namespace mc {

// Which tool a block wants and what it takes to get drops (wiki: each block's
// "Tool" row and Breaking › Blocks by hardness / harvest level).
struct HarvestInfo {
    world::ToolType tool = world::ToolType::None; // preferred (faster); None = any
    int minLevel = -1; // -1: drops by hand; else a tool of `tool` with this level
};
HarvestInfo harvestInfo(world::BlockId block);

// True if breaking with `held` yields drops.
bool canHarvest(world::BlockStateId state, const world::ItemStack& held);

// Game ticks to break a block in survival (wiki: Breaking › Speed):
//   speed = tool speed if it is the right tool (1 otherwise or if it can't harvest);
//   damage per tick = speed / hardness / (harvestable ? 30 : 100);
//   /5 when not on the ground, /5 when the eyes are in water;
//   instant if damage per tick > 1; unbreakable (bedrock: hardness -1) -> -1.
// `haste`: the Haste level (M23.6; wiki: Haste - speed x (1 + 0.2 level)).
int breakTicks(world::BlockStateId state, const world::ItemStack& held, bool onGround,
               bool eyesInWater, int haste = 0);

// What a broken block drops (wiki: each block's "Drops"). Empty if it can't be
// harvested with `held`.
// Appends to `out` (no allocation when it has room).
// A durable item used `amount` times: each use is skipped with chance level/(level+1)
// under Unbreaking (wiki: Unbreaking); returns the stack, empty once it breaks.
world::ItemStack wearItem(world::ItemStack s, int amount, world::Xoroshiro& rng);

// Experience a mined block gives (wiki: Experience › ores): coal 0-2, diamond and
// emerald 3-7, lapis and quartz 2-5, redstone 1-5, nether gold 0-1; 0 otherwise.
int blockExperience(world::BlockStateId state, world::Xoroshiro& rng);
// `anyTool`: loot as if harvested correctly (explosions: the tool rule is the player's).
void blockDrops(world::BlockStateId state, const world::ItemStack& held, world::Xoroshiro& rng,
                std::vector<world::ItemStack>& out, bool anyTool = false);

} // namespace mc
