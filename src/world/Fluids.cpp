// Water and lava flow (M14; wiki: Fluid, Water, Lava). Part of BlockUpdates: fluids
// react to block updates by scheduling a fluid tick, and a fluid tick recomputes the
// block's level from its neighbours, then spreads.
//
// Levels (block state `level`): 0 = source, 1..7 = flowing (amount 8 - level), 8..15
// = falling (a full column under fluid above). Water loses 1 per block (reach 7),
// lava 2 in the Overworld/End (reach 3), 1 in the Nether.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

#include <algorithm>
#include <utility>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }
BlockPos rel(const BlockPos& p, Direction d) {
    const glm::ivec3 v = normal(d);
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}
Direction opposite(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }
constexpr Direction kSides[4] = {Direction::North, Direction::South, Direction::West, Direction::East};

int levelOf(BlockStateId s) { return R().get(s, level); }

} // namespace

bool BlockUpdates::isFluid(BlockId b) { return b == B::Water || b == B::Lava; }

int BlockUpdates::fluidAmount(BlockStateId s) {
    if (!isFluid(blockOf(s))) return 0;
    const int l = levelOf(s);
    return l == 0 || l >= 8 ? 8 : 8 - l;
}

BlockStateId BlockUpdates::fluidState(BlockId kind, int amount, bool falling) {
    const BlockStateId base = R().defaultState(kind);
    if (falling) return R().set(base, level, 8);
    return R().set(base, level, amount >= 8 ? 0 : 8 - amount);
}

bool BlockUpdates::breaksInFluid(BlockId b) {
    // Blocks a flowing fluid washes away, dropping them (wiki: Water - plants, torches,
    // redstone dust and components, snow...).
    switch (b) {
    case B::ShortGrass:
    case B::Fern:
    case B::Dandelion:
    case B::Poppy:
    case B::Cornflower:
    case B::AzureBluet:
    case B::OxeyeDaisy:
    case B::DeadBush:
    case B::Snow:
    case B::Torch:
    case B::RedstoneWire:
    case B::RedstoneTorch:
    case B::RedstoneWallTorch:
    case B::Repeater:
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
    case B::Fire: return true;
    default: return false;
    }
}

int BlockUpdates::fluidDelay(BlockId kind) const {
    // wiki: water 5 ticks; lava 30 (10 in the Nether, an ultrawarm dimension).
    return kind == B::Water ? 5 : m_world.isUltrawarm() ? 10 : 30;
}

int BlockUpdates::fluidDrop(BlockId kind) const {
    return kind == B::Lava && !m_world.isUltrawarm() ? 2 : 1;
}

int BlockUpdates::slopeFindDistance(BlockId kind) const {
    // How far a fluid looks for a drop to flow toward (wiki: Fluid).
    return kind == B::Lava && !m_world.isUltrawarm() ? 2 : 4;
}

BlockUpdates::FluidInto BlockUpdates::fluidInto(const BlockPos& p, BlockId kind) const {
    // An unloaded chunk is a wall, not a hole: flow waits for it (vanilla only ticks
    // fluids whose surroundings are loaded).
    if (!m_world.isInHeight(p.y)) return FluidInto::No;
    const BlockStateId target = at(p); // (sets m_cache to p's chunk)
    if (!m_cache) return FluidInto::No;
    if (target == 0) return FluidInto::Empty;
    const BlockId b = blockOf(target);
    if (b == kind) return FluidInto::Same;
    if (isFluid(b)) return FluidInto::No; // the other fluid: a reaction, not a flow
    return breaksInFluid(b) ? FluidInto::Breaks : FluidInto::No;
}

bool BlockUpdates::isHole(const BlockPos& p, BlockId kind) const {
    // A fluid there could keep falling: the block below takes fluid.
    const BlockPos below = rel(p, Direction::Down);
    return m_world.isInHeight(below.y) && fluidInto(below, kind) != FluidInto::No;
}

BlockStateId BlockUpdates::newFluidState(const BlockPos& p, BlockId kind) const {
    int maxAmount = 0, sources = 0;
    for (const Direction d : kSides) {
        const BlockStateId n = at(rel(p, d));
        if (blockOf(n) != kind) continue;
        if (levelOf(n) == 0) ++sources;
        maxAmount = std::max(maxAmount, fluidAmount(n));
    }
    // A new source between two sources, over a solid block or another source (wiki:
    // Water › Infinite water source; lava doesn't by default).
    if (kind == B::Water && sources >= 2) {
        const BlockStateId below = at(rel(p, Direction::Down));
        if (R().collides(below) || (blockOf(below) == kind && levelOf(below) == 0))
            return fluidState(kind, 8, false);
    }
    if (blockOf(at(rel(p, Direction::Up))) == kind) return fluidState(kind, 8, true); // falling
    const int amount = maxAmount - fluidDrop(kind);
    return amount > 0 ? fluidState(kind, amount, false) : BlockStateId{0};
}

bool BlockUpdates::lavaMeetsWater(const BlockPos& p, BlockStateId s) {
    // Lava touching water (not from below) hardens: a source into obsidian, flowing
    // lava into cobblestone (wiki: Lava › Water and lava).
    for (const Direction d : {Direction::Up, Direction::North, Direction::South, Direction::West, Direction::East})
        if (blockOf(at(rel(p, d))) == B::Water) {
            set(p, R().defaultState(levelOf(s) == 0 ? B::Obsidian : B::Cobblestone));
            return true;
        }
    return false;
}

void BlockUpdates::fluidNeighbourChanged(const BlockPos& p, BlockStateId s) {
    const BlockId kind = blockOf(s);
    if (kind == B::Lava && lavaMeetsWater(p, s)) return;
    schedule(p, kind, fluidDelay(kind), 0); // (ignored if one is pending)
}

void BlockUpdates::placeFluid(const BlockPos& p, BlockStateId state) {
    const BlockStateId old = at(p);
    if (old == state) return;
    // Water washes blocks away with their drops; lava burns them (no drop; wiki: Lava).
    if (breaksInFluid(blockOf(old)) && blockOf(state) == B::Water)
        if (const ItemId item = itemRegistry().blockItem(blockOf(old))) m_drops.push_back({p, {item, 1}});
    set(p, state);
    if (state == 0) return;
    // Flowing lava arriving next to water hardens at once (its neighbours' updates
    // don't reach it: water beside it doesn't change).
    if (blockOf(state) == B::Lava && lavaMeetsWater(p, state)) return;
    // The new fluid block schedules its own tick (vanilla: on placement).
    schedule(p, blockOf(state), fluidDelay(blockOf(state)), 0);
}

void BlockUpdates::tickFluid(const BlockPos& p, BlockStateId s) {
    const BlockId kind = blockOf(s);
    // The slope search reaches 5 blocks: wait until those chunks are loaded.
    for (const auto& [dx, dz] : {std::pair{-5, 0}, {5, 0}, {0, -5}, {0, 5}})
        if (!chunkAt({p.x + dx, p.y, p.z + dz})) {
            schedule(p, kind, fluidDelay(kind), 0);
            return;
        }
    if (levelOf(s) != 0) {
        const BlockStateId next = newFluidState(p, kind);
        if (next != s) {
            set(p, next);
            if (next == 0) return; // dried up
            s = next;
            schedule(p, kind, fluidDelay(kind), 0); // keep settling
        }
    }
    // Down first; a source (or anything that can't fall) also spreads sideways.
    const BlockPos below = rel(p, Direction::Down);
    if (m_world.isInHeight(below.y)) {
        const BlockStateId bs = at(below);
        if (kind == B::Lava && blockOf(bs) == B::Water) { // lava falling onto water: stone
            set(below, R().defaultState(B::Stone));
            return;
        }
        const FluidInto into = fluidInto(below, kind);
        // Below already this fluid: it turns itself into a falling column (its own
        // update), and a source here still spreads sideways (wiki: Fluid - a midair
        // source flows down, then to its four sides).
        if (into != FluidInto::No && into != FluidInto::Same) {
            placeFluid(below, fluidState(kind, 8, true));
            int sources = 0;
            for (const Direction d : kSides)
                sources += blockOf(at(rel(p, d))) == kind && levelOf(at(rel(p, d))) == 0;
            if (levelOf(s) == 0 && sources >= 3) spreadSideways(p, s);
            return;
        }
    }
    if (levelOf(s) == 0 || !isHole(p, kind)) spreadSideways(p, s);
}

int BlockUpdates::slopeDistance(const BlockPos& p, int depth, Direction from, BlockId kind) const {
    // Steps to the nearest place where the fluid could fall, up to the search distance;
    // 1000 if none (vanilla's slope finding).
    int best = 1000;
    for (const Direction d : kSides) {
        if (d == from) continue;
        const BlockPos n = rel(p, d);
        const FluidInto into = fluidInto(n, kind);
        if (into == FluidInto::No || (into == FluidInto::Same && levelOf(at(n)) == 0)) continue;
        if (isHole(n, kind)) return depth;
        if (depth < slopeFindDistance(kind)) best = std::min(best, slopeDistance(n, depth + 1, opposite(d), kind));
    }
    return best;
}

void BlockUpdates::spreadSideways(const BlockPos& p, BlockStateId s) {
    const BlockId kind = blockOf(s);
    const int amount = (levelOf(s) >= 8 ? 8 : fluidAmount(s)) - fluidDrop(kind);
    if (amount <= 0) return;
    // Toward the nearest drop: every direction tied for the shortest way down; all
    // open directions if there is none within reach.
    int dist[4];
    int best = 1000;
    for (int i = 0; i < 4; ++i) {
        dist[i] = -1;
        const BlockPos n = rel(p, kSides[i]);
        const FluidInto into = fluidInto(n, kind);
        if (into == FluidInto::No || (into == FluidInto::Same && levelOf(at(n)) == 0)) continue;
        dist[i] = isHole(n, kind) ? 0 : slopeDistance(n, 1, opposite(kSides[i]), kind);
        best = std::min(best, dist[i]);
    }
    const BlockStateId flowing = fluidState(kind, amount, false);
    for (int i = 0; i < 4; ++i) {
        if (dist[i] < 0 || dist[i] != best) continue;
        const BlockPos n = rel(p, kSides[i]);
        const BlockStateId ns = at(n);
        if (blockOf(ns) == kind && fluidAmount(ns) >= amount) continue; // already as full
        if (kind == B::Lava && blockOf(ns) == B::Water) continue;      // (the reaction handles it)
        placeFluid(n, flowing);
    }
}

} // namespace mc::world
