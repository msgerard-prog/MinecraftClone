#pragma once

#include "gameplay/Commands.h"
#include "world/World.h"

#include <vector>

namespace mc {

// Command blocks (M29.7; wiki: Command Block). Runs the impulse and repeating command
// blocks that fired this tick (world::BlockUpdates::commandRuns), each followed by the
// chain command blocks its front points into: a chain block runs when it is powered or
// Always Active, and a conditional one only when the block before it succeeded. Each
// keeps its success count (its comparator signal) and last output. At most
// `maxChain` blocks per chain (vanilla game rule maxCommandChainLength, 65536). Clears
// `runs`.
void runCommandBlocks(world::World& world, std::vector<world::BlockPos>& runs, CommandContext& ctx,
                      int maxChain = 65536);

} // namespace mc
