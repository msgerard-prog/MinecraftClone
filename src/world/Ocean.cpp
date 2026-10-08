// Ocean blocks (M25.1; wiki: Kelp, Seagrass, Sea Pickle, Coral, Coral Block, Coral
// Fan). Part of BlockUpdates: their supports, kelp growing on random ticks, and living
// coral dying away from water.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

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

} // namespace mc::world
