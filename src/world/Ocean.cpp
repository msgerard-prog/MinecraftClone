// Ocean blocks (M25.1; wiki: Kelp, Seagrass, Sea Pickle, Coral, Coral Block, Coral
// Fan). Part of BlockUpdates: their supports, kelp growing on random ticks, and living
// coral dying away from water.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

#include <array>
#include <string>
#include <vector>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockPos rel(const BlockPos& p, Direction d) {
    const glm::ivec3 v = normal(d);
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}
bool supports(BlockStateId s) { return R().collides(s) && !R().block(R().blockOf(s)).id.ends_with("_leaves"); }

// Each coral block, plant and fan, alive or dead, with the dead counterpart of the
// living ones (worked out once from the names).
struct CoralInfo {
    enum Kind : uint8_t { None, Block, Plant, Fan } kind = None;
    bool alive = false;
    BlockId dead = 0;
};
const std::vector<CoralInfo>& coralTable() {
    static const std::vector<CoralInfo> table = [] {
        std::vector<CoralInfo> t(R().blockCount());
        for (BlockId b = 0; b < R().blockCount(); ++b) {
            const std::string id = R().block(b).id.substr(10);
            CoralInfo c;
            if (id.ends_with("_coral_block")) c.kind = CoralInfo::Block;
            else if (id.ends_with("_coral")) c.kind = CoralInfo::Plant;
            else if (id.ends_with("_coral_fan")) c.kind = CoralInfo::Fan;
            else continue;
            c.alive = !id.starts_with("dead_");
            if (c.alive) c.dead = *R().findBlock("dead_" + id);
            t[b] = c;
        }
        return t;
    }();
    return table;
}

} // namespace

bool BlockUpdates::isOceanPlant(BlockId b) {
    return b == B::Kelp || b == B::KelpPlant || b == B::Seagrass || b == B::TallSeagrass || b == B::SeaPickle ||
           (b < coralTable().size() && coralTable()[b].kind != CoralInfo::None);
}

bool BlockUpdates::oceanSurvives(const BlockPos& p, BlockStateId s) const {
    const BlockId b = R().blockOf(s);
    const BlockStateId below = at(rel(p, Direction::Down));
    const BlockId bb = R().blockOf(below);
    switch (b) {
    case B::Kelp:
    case B::KelpPlant: // on kelp or a solid block, never on magma (wiki: Kelp)
        return bb == B::KelpPlant || bb == B::Kelp || (supports(below) && bb != B::MagmaBlock);
    case B::TallSeagrass: // its two halves together
        if (R().get(s, doorHalf) == 0) return bb == B::TallSeagrass;
        return supports(below) && R().blockOf(at(rel(p, Direction::Up))) == B::TallSeagrass;
    case B::Seagrass:
    case B::SeaPickle:
        return supports(below) && bb != B::MagmaBlock;
    default:
        if (b < coralTable().size() && coralTable()[b].kind != CoralInfo::None && coralTable()[b].kind != CoralInfo::Block)
            return supports(below);
        return true;
    }
}

bool BlockUpdates::oceanNeighbourChanged(const BlockPos& p, BlockStateId s) {
    const BlockId b = R().blockOf(s);
    if (!isOceanPlant(b)) return false;
    if (!oceanSurvives(p, s)) {
        pop(p);
        return true;
    }
    // A kelp tip with kelp put on it becomes a stem (wiki: Kelp).
    if (b == B::Kelp) {
        const BlockId above = R().blockOf(at(rel(p, Direction::Up)));
        if (above == B::Kelp || above == B::KelpPlant) set(p, R().defaultState(B::KelpPlant));
        return true;
    }
    // A kelp stem whose top was taken becomes the tip again (wiki: Kelp).
    if (b == B::KelpPlant) {
        const BlockId above = R().blockOf(at(rel(p, Direction::Up)));
        if (above != B::Kelp && above != B::KelpPlant)
            set(p, R().set(R().defaultState(B::Kelp), age25, int(m_random.nextInt(25))));
        return true;
    }
    // Living coral out of water dies after a few seconds (wiki: Coral - 60-99 ticks).
    const CoralInfo& c = coralTable()[b];
    if (c.alive && !coralWet(p, s) && !hasTick(p, b)) schedule(p, b, 60 + int(m_random.nextInt(40)), 0);
    return true;
}

bool BlockUpdates::coralWet(const BlockPos& p, BlockStateId s) const {
    const BlockId b = R().blockOf(s);
    if (coralTable()[b].kind != CoralInfo::Block) return R().waterlogged(s); // plants and fans: their own water
    for (int d = 0; d < kDirectionCount; ++d) { // blocks: water on any side
        const BlockStateId n = at(rel(p, static_cast<Direction>(d)));
        if (R().blockOf(n) == B::Water || R().waterlogged(n)) return true;
    }
    return false;
}

bool BlockUpdates::tickOcean(const BlockPos& p, BlockStateId s) {
    const BlockId b = R().blockOf(s);
    if (b >= coralTable().size() || !coralTable()[b].alive) return false;
    if (!coralWet(p, s)) { // still dry: dead (keeping its waterlogged state - dry anyway)
        BlockStateId dead = R().defaultState(coralTable()[b].dead);
        if (coralTable()[b].kind != CoralInfo::Block) dead = R().set(dead, waterlogged, 1);
        set(p, dead);
    }
    return true;
}

void BlockUpdates::growKelp(const BlockPos& p, BlockStateId s) {
    // A kelp tip grows a block on 14% of its random ticks into the water above, until
    // its age reaches 25; the old tip becomes a stem (wiki: Kelp).
    const int a = R().get(s, age25);
    if (a >= 25 || m_random.nextInt(100) >= 14) return;
    const BlockPos up = rel(p, Direction::Up);
    if (!m_world.isInHeight(up.y)) return;
    const BlockStateId above = at(up);
    if (R().blockOf(above) != B::Water || R().get(above, level) != 0) return;
    set(up, R().set(s, age25, a + 1));
    set(p, R().defaultState(B::KelpPlant));
}

void BlockUpdates::tickTurtleEgg(const BlockPos& p, BlockStateId s) {
    // On sand, eggs crack a stage on a random tick at night (always) or by day (1 in 500);
    // past the second crack they hatch into baby turtles (wiki: Turtle Egg › Hatching).
    const BlockId below = R().blockOf(at(rel(p, Direction::Down)));
    if (below != B::Sand && below != B::RedSand) return;
    const bool night = m_skyDarken >= 4; // (the sky has darkened: night)
    if (!night && m_random.nextInt(500) != 0) return;
    const int h = R().get(s, hatch);
    if (h < 2) {
        set(p, R().set(s, hatch, h + 1));
        return;
    }
    m_hatched.push_back({p, R().get(s, eggs) + 1});
    set(p, 0);
}

bool BlockUpdates::spongeChanged(const BlockPos& p, BlockStateId s) {
    // A dry sponge next to water soaks up the water within 7 blocks (taxicab, through
    // water), up to 65 blocks, and turns wet; waterlogged plants in reach are washed
    // out with their drops (wiki: Sponge). A wet sponge put in the Nether dries at once.
    const BlockId b = R().blockOf(s);
    if (b == B::WetSponge) {
        if (m_world.isUltrawarm()) {
            set(p, R().defaultState(B::Sponge));
            fizz(p);
        }
        return true;
    }
    if (b != B::Sponge) return false;
    auto wet = [&](const BlockPos& q) {
        const BlockStateId t = at(q);
        return R().blockOf(t) == B::Water || R().waterlogged(t);
    };
    bool touching = false;
    for (int d = 0; d < kDirectionCount && !touching; ++d) touching = wet(rel(p, static_cast<Direction>(d)));
    if (!touching) return true;
    set(p, R().defaultState(B::WetSponge)); // (first: the updates below mustn't wake it again)
    struct Q {
        BlockPos pos;
        int dist;
    };
    std::array<Q, 512> queue{}; // (65 removed at most, a few hundred visited)
    int head = 0, tail = 0, removed = 0;
    queue[size_t(tail++)] = {p, 0};
    while (head < tail && removed < 65) {
        const Q q = queue[size_t(head++)];
        for (int d = 0; d < kDirectionCount && removed < 65; ++d) {
            const BlockPos n = rel(q.pos, static_cast<Direction>(d));
            if (!m_world.isInHeight(n.y) || !wet(n)) continue;
            const BlockStateId t = at(n);
            if (R().blockOf(t) == B::Water) set(n, 0);
            else if (R().blockOf(t) == B::Kelp || R().blockOf(t) == B::KelpPlant || R().blockOf(t) == B::Seagrass ||
                     R().blockOf(t) == B::TallSeagrass) {
                m_drops.push_back({n, {}, t}); // (washed out, dropping as if broken)
                set(n, 0);
            } else {
                set(n, R().set(t, waterlogged, 1)); // (drained)
            }
            ++removed;
            if (q.dist + 1 < 7 && tail < int(queue.size())) queue[size_t(tail++)] = {n, q.dist + 1};
        }
    }
    return true;
}

} // namespace mc::world
