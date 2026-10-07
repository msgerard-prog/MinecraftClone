#include "world/Rails.h"

#include "world/Blocks.h"

namespace mc::world {


bool isRail(BlockId b) {
    return b == blocks::Rail || b == blocks::PoweredRail || b == blocks::DetectorRail || b == blocks::ActivatorRail;
}

int railShapeOf(BlockStateId s) {
    const auto& r = blockRegistry();
    const BlockId b = r.blockOf(s);
    if (b == blocks::Rail) return r.get(s, properties::railShape);
    if (isRail(b)) return r.get(s, properties::straightRailShape);
    return -1;
}

BlockStateId withRailShape(BlockStateId s, int shape) {
    const auto& r = blockRegistry();
    if (r.blockOf(s) == blocks::Rail) return r.set(s, properties::railShape, shape);
    return shape < 6 ? r.set(s, properties::straightRailShape, shape) : s;
}

RailExits railExits(int shape) {
    using D = Direction;
    switch (shape) {
    case 1: return {D::East, D::West};
    case 2: return {D::East, D::West, true};
    case 3: return {D::West, D::East, true};
    case 4: return {D::North, D::South, true};
    case 5: return {D::South, D::North, true};
    case 6: return {D::South, D::East};
    case 7: return {D::South, D::West};
    case 8: return {D::North, D::West};
    case 9: return {D::North, D::East};
    default: return {D::North, D::South};
    }
}

namespace {

BlockPos step(const BlockPos& p, Direction d, int dy = 0) {
    const glm::ivec3 n = normal(d);
    return {p.x + n.x, p.y + n.y + dy, p.z + n.z};
}

// Is there a rail beside `p` toward `d` (level, one up or one down)? `up`: one up.
bool railToward(const World& world, const BlockPos& p, Direction d, bool& up) {
    const auto& r = blockRegistry();
    up = false;
    if (isRail(r.blockOf(world.getBlock(step(p, d))))) return true;
    if (isRail(r.blockOf(world.getBlock(step(p, d, 1))))) {
        up = true;
        return true;
    }
    return isRail(r.blockOf(world.getBlock(step(p, d, -1))));
}

} // namespace

int chooseRailShape(const World& world, const BlockPos& p, BlockStateId current) {
    const auto& r = blockRegistry();
    const bool plain = r.blockOf(current) == blocks::Rail;
    const int cur = railShapeOf(current);
    bool up = false;
    if (cur >= 0) { // both ends still lead somewhere: keep it
        const RailExits e = railExits(cur);
        bool ua = false, ub = false;
        if (railToward(world, p, e.a, ua) && railToward(world, p, e.b, ub) && ua == e.aUp && !ub) return cur;
    }
    bool n, s, e, w, nu, su, eu, wu;
    n = railToward(world, p, Direction::North, nu);
    s = railToward(world, p, Direction::South, su);
    e = railToward(world, p, Direction::East, eu);
    w = railToward(world, p, Direction::West, wu);
    (void)up;
    if ((n || s) && !(e || w)) return nu ? 4 : su ? 5 : 0;
    if ((e || w) && !(n || s)) return eu ? 2 : wu ? 3 : 1;
    if (n && s) return nu ? 4 : su ? 5 : 0;
    if (e && w) return eu ? 2 : wu ? 3 : 1;
    if (plain) { // one north/south and one east/west neighbour: a curve
        if (s && e) return 6;
        if (s && w) return 7;
        if (n && w) return 8;
        if (n && e) return 9;
    }
    if (n || s) return nu ? 4 : su ? 5 : 0;
    if (e || w) return eu ? 2 : wu ? 3 : 1;
    return cur >= 0 ? cur : 0;
}

} // namespace mc::world
