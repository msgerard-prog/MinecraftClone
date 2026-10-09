#include "gameplay/CommandBlocks.h"

#include "world/Blocks.h"
#include "world/Direction.h"

#include <algorithm>
#include <array>
#include <string>

namespace mc {

using namespace world;

namespace {

CommandBlockData* dataAt(World& world, const BlockPos& p, Chunk** chunk) {
    *chunk = world.chunk(p.chunk());
    return *chunk ? (*chunk)->commandBlock(blockToLocal(p.x), p.y, blockToLocal(p.z)) : nullptr;
}

// Runs one block's command; true when it succeeded.
bool execute(World& world, const BlockPos& p, CommandContext& ctx) {
    Chunk* c = nullptr;
    CommandBlockData* d = dataAt(world, p, &c);
    if (!d) return false;
    if (d->command.empty()) {
        d->successCount = 0;
        c->markDirty();
        return false;
    }
    const glm::dvec3 centre(p.x + 0.5, p.y + 0.5, p.z + 0.5);
    const glm::dvec3* before = ctx.origin;
    ctx.origin = &centre;
    // The command may edit this very chunk (/setblock on itself or a neighbour), which moves
    // or frees its block entities: run a copy and look the block up again afterwards (M29 review).
    static thread_local std::string command;
    command = d->command;
    const CommandResult r = runCommand(command, ctx);
    ctx.origin = before;
    d = dataAt(world, p, &c);
    if (!d) return r.ok; // (it replaced itself)
    if (d->lastOutput != r.message) d->lastOutput = r.message; // (no copy when it repeats)
    d->successCount = r.ok ? 1 : 0;
    c->markDirty();
    return r.ok;
}

} // namespace

void runCommandBlocks(World& world, std::vector<BlockPos>& runs, CommandContext& ctx, int maxChain) {
    const auto& reg = blockRegistry();
    for (const BlockPos& start : runs) {
        if (reg.blockOf(world.getBlock(start)) != blocks::CommandBlock &&
            reg.blockOf(world.getBlock(start)) != blocks::RepeatingCommandBlock)
            continue;
        // A conditional impulse/repeating block runs only if the block behind it succeeded.
        const BlockStateId ss = world.getBlock(start);
        const auto back = normal(static_cast<Direction>(reg.get(ss, properties::facing6) ^ 1));
        if (reg.get(ss, properties::conditional) == 0) {
            Chunk* bc = nullptr;
            const CommandBlockData* bd = dataAt(world, {start.x + back.x, start.y + back.y, start.z + back.z}, &bc);
            if (!bd || bd->successCount == 0) continue;
        }
        bool last = execute(world, start, ctx);
        BlockPos p = start;
        // A loop of chain blocks would run to the cap every tick (M29 review): stop when the
        // walk comes back to one of the last 64 blocks it passed.
        std::array<BlockPos, 64> recent{};
        int seen = 0;
        for (int n = 0; n < maxChain; ++n) { // the chain blocks its front points into
            const auto f = normal(static_cast<Direction>(reg.get(world.getBlock(p), properties::facing6)));
            p = {p.x + f.x, p.y + f.y, p.z + f.z};
            const BlockStateId s = world.getBlock(p);
            if (reg.blockOf(s) != blocks::ChainCommandBlock || p == start) break;
            if (std::find(recent.begin(), recent.begin() + std::min(seen, 64), p) != recent.begin() + std::min(seen, 64))
                break;
            recent[size_t(seen++ % 64)] = p;
            Chunk* c = nullptr;
            CommandBlockData* d = dataAt(world, p, &c);
            if (!d) break;
            if (!(d->powered || d->autoActive)) continue; // (skipped; the chain goes on)
            if (reg.get(s, properties::conditional) == 0 && !last) { // conditional: the one before failed
                d->successCount = 0;
                c->markDirty();
                last = false;
                continue;
            }
            last = execute(world, p, ctx);
        }
    }
    runs.clear();
}

} // namespace mc
