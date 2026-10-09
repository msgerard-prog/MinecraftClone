#include "gameplay/CommandBlocks.h"

#include "world/Blocks.h"
#include "world/Direction.h"

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
    bool ok = false;
    if (!d->command.empty()) {
        const glm::dvec3 centre(p.x + 0.5, p.y + 0.5, p.z + 0.5);
        const glm::dvec3* before = ctx.origin;
        ctx.origin = &centre;
        const CommandResult r = runCommand(d->command, ctx);
        ctx.origin = before;
        ok = r.ok;
        d->lastOutput = r.message;
    }
    d->successCount = ok ? 1 : 0;
    c->markDirty();
    return ok;
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
        for (int n = 0; n < maxChain; ++n) { // the chain blocks its front points into
            const auto f = normal(static_cast<Direction>(reg.get(world.getBlock(p), properties::facing6)));
            p = {p.x + f.x, p.y + f.y, p.z + f.z};
            const BlockStateId s = world.getBlock(p);
            if (reg.blockOf(s) != blocks::ChainCommandBlock || p == start) break;
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
