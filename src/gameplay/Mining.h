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
int breakTicks(world::BlockStateId state, const world::ItemStack& held, bool onGround,
               bool eyesInWater);

// What a broken block drops (wiki: each block's "Drops"). Empty if it can't be
// harvested with `held`.
std::vector<world::ItemStack> blockDrops(world::BlockStateId state, const world::ItemStack& held,
                                         world::Xoroshiro& rng);

} // namespace mc
