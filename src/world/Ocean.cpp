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
Direction opposite(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }
BlockPos rel(const BlockPos& p, Direction d) {
    const glm::ivec3 v = normal(d);
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}
bool supports(BlockStateId s) { return R().collides(s) && !R().block(R().blockOf(s)).id.ends_with("_leaves"); }

// Each coral block, plant and fan, alive or dead, with the dead counterpart of the
// living ones (worked out once from the names).
struct CoralInfo {
    enum Kind : uint8_t { None, Block, Plant, Fan, WallFan } kind = None;
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
            else if (id.ends_with("_coral_wall_fan")) c.kind = CoralInfo::WallFan; // (M29.4c)
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
        if (b < coralTable().size() && coralTable()[b].kind == CoralInfo::WallFan) // (M29.4c) on its wall
            return supports(at(rel(p, opposite(static_cast<Direction>(R().get(s, facing) + 2)))));
        if (b < coralTable().size() && coralTable()[b].kind != CoralInfo::None && coralTable()[b].kind != CoralInfo::Block)
            return supports(below);
        return true;
    }
}

bool BlockUpdates::oceanNeighbourChanged(const BlockPos& p, BlockStateId s) {
    const BlockId b = R().blockOf(s);
    if (b == B::Frogspawn) { // (M26.3c) gone with the water under it
        if (R().blockOf(at(rel(p, Direction::Down))) != B::Water) set(p, 0);
        return true;
    }
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
    if (coralTable()[b].kind != CoralInfo::Block && R().waterlogged(s)) return true; // plants and fans: their own water
    for (int d = 0; d < kDirectionCount; ++d) { // or water on any side (wiki: Coral)
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
        if (coralTable()[b].kind == CoralInfo::WallFan) dead = R().set(dead, facing, R().get(s, facing));
        set(p, dead);
    }
    return true;
}

void BlockUpdates::fillBubbleColumn(const BlockPos& base) {
    // (wiki: Bubble Column) every water source straight above, up to the first other block.
    const BlockId b = R().blockOf(at(base));
    if (b != B::SoulSand && b != B::MagmaBlock) return;
    const BlockStateId column = R().set(R().defaultState(B::BubbleColumn), drag, b == B::MagmaBlock ? 0 : 1);
    for (BlockPos q = rel(base, Direction::Up); m_world.isInHeight(q.y); q = rel(q, Direction::Up)) {
        const BlockStateId s = at(q);
        const bool source = R().blockOf(s) == B::Water && R().get(s, level) == 0;
        if (!source && R().blockOf(s) != B::BubbleColumn) break;
        if (s != column) set(q, column);
    }
}

void BlockUpdates::tickBubbleColumn(const BlockPos& p, BlockStateId s) {
    const BlockStateId below = at(rel(p, Direction::Down));
    const BlockId bb = R().blockOf(below);
    if (bb != B::BubbleColumn && bb != B::SoulSand && bb != B::MagmaBlock) { // its base is gone: water again
        set(p, R().defaultState(B::Water));
        return;
    }
    const int want = bb == B::BubbleColumn ? R().get(below, drag) : bb == B::MagmaBlock ? 0 : 1;
    if (R().get(s, drag) != want) set(p, R().set(s, drag, want));
    // A water source put on top joins the column.
    const BlockPos up = rel(p, Direction::Up);
    if (const BlockStateId a = at(up); R().blockOf(a) == B::Water && R().get(a, level) == 0)
        set(up, R().set(R().defaultState(B::BubbleColumn), drag, want));
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

void BlockUpdates::tickFrogspawn(const BlockPos& p) {
    // Frogspawn hatches 2-6 tadpoles after 3600-12000 ticks (wiki: Frogspawn); ours on a
    // random tick 1 time in 5 (about 6800 ticks on average).
    if (m_random.nextInt(5) != 0) return;
    m_hatched.push_back({p, 2 + int(m_random.nextInt(5)), MobType::Tadpole});
    set(p, 0);
}

void BlockUpdates::tickTurtleEgg(const BlockPos& p, BlockStateId s) {
    // On sand, eggs crack a stage on a random tick just before dawn (day time 21062-21904,
    // always) or otherwise 1 in 500 - about 4-5 nights to hatch;
    // past the second crack they hatch into baby turtles (wiki: Turtle Egg › Hatching).
    const BlockId below = R().blockOf(at(rel(p, Direction::Down)));
    if (below != B::Sand && below != B::RedSand) return;
    const int64_t t = ((m_dayTime % 24000) + 24000) % 24000;
    const bool dawn = t >= 21062 && t <= 21904;
    if (!dawn && m_random.nextInt(500) != 0) return;
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
