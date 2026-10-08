// Pointed dripstone (M27.2b; wiki: Pointed Dripstone). Part of BlockUpdates.
//
// Stalactites hang (vertical_direction down) and stalagmites stand (up). A column's
// pieces take their thickness from their neighbours: the tip, the frustum above it, the
// middle and the base at the root (two tips meeting: tip_merge). A stalagmite without its
// support breaks; a stalactite without its support falls and skewers what it lands on.
// Under water or lava (above the block a stalactite hangs from) it drips: water fills a
// cauldron below a level at a time and dries mud into clay, lava fills one at once; with
// water above a dripstone block it slowly grows, and its drips build a stalagmite below.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockPos dy(const BlockPos& p, int d) { return {p.x, p.y + d, p.z}; }
// The direction a piece points: +1 up (a stalagmite), -1 down (a stalactite).
int pointing(BlockStateId s) { return R().get(s, verticalDirection) == 0 ? 1 : -1; }
bool dripstone(BlockStateId s) { return R().blockOf(s) == B::PointedDripstone; }

} // namespace

int BlockUpdates::dripstoneThickness(const BlockPos& p, BlockStateId s) const {
    // thickness values: 0 tip_merge, 1 tip, 2 frustum, 3 middle, 4 base.
    const int d = pointing(s);
    const BlockStateId next = at(dy(p, d));
    if (!dripstone(next) || pointing(next) != d) // the end of the column
        return dripstone(next) && pointing(next) == -d && R().get(next, thickness) <= 1 ? 0 : 1;
    if (R().get(next, thickness) <= 1) return 2;
    const BlockStateId prev = at(dy(p, -d));
    return dripstone(prev) && pointing(prev) == d ? 3 : 4;
}

bool BlockUpdates::dripstoneChanged(const BlockPos& p, BlockStateId s) {
    if (!dripstone(s)) return false;
    const int d = pointing(s);
    const BlockStateId support = at(dy(p, -d));
    const bool held = dripstone(support) ? pointing(support) == d : R().collides(support);
    if (!held) {
        if (d < 0 && p.y > m_world.height().minY) { // a stalactite falls
            m_falling.push_back({p, R().set(s, waterlogged, 1)});
            set(p, leftAfterBreaking(s));
        } else {
            pop(p);
        }
        return true;
    }
    const int t = dripstoneThickness(p, s);
    if (t != R().get(s, thickness)) set(p, R().set(s, thickness, t));
    return true;
}

void BlockUpdates::tickDripstone(const BlockPos& tip, BlockStateId s) {
    // Only a hanging tip drips or grows.
    if (pointing(s) != -1 || R().get(s, thickness) > 1) return;
    BlockPos root = tip;
    int length = 1;
    while (dripstone(at(dy(root, 1))) && pointing(at(dy(root, 1))) == -1) {
        root = dy(root, 1);
        ++length;
    }
    const BlockPos support = dy(root, 1);
    const BlockId fluid = R().blockOf(at(dy(support, 1)));
    const bool water = fluid == B::Water && R().get(at(dy(support, 1)), level) == 0;
    const bool lava = fluid == B::Lava && R().get(at(dy(support, 1)), level) == 0;
    if (!water && !lava) return;
    // Growth: 64 in 5625 with water over a dripstone block (wiki), up to 8 long; a tip
    // that can't grow down builds the stalagmite below instead.
    if (water && R().blockOf(at(support)) == B::DripstoneBlock && m_random.nextInt(5625) < 64) {
        const BlockStateId down = R().set(R().defaultState(B::PointedDripstone), verticalDirection, 1);
        if (length < 8 && at(dy(tip, -1)) == 0 && at(dy(tip, -2)) == 0) {
            set(dy(tip, -1), down);
        } else {
            for (int k = 1; k <= 10; ++k) {
                const BlockPos q = dy(tip, -k);
                const BlockStateId here = at(q);
                if (here == 0) continue;
                if (dripstone(here) && pointing(here) == 1 && at(dy(q, 1)) == 0) set(dy(q, 1), R().defaultState(B::PointedDripstone));
                else if (R().collides(here) && !dripstone(here) && at(dy(q, 1)) == 0)
                    set(dy(q, 1), R().defaultState(B::PointedDripstone));
                break;
            }
        }
    }
    // Dripping (wiki: water 45/256, lava 15/256 a random tick): mud above dries to clay;
    // the first block within 10 below, if a cauldron, fills.
    if (m_random.nextInt(256) >= (water ? 45u : 15u)) return;
    if (water && R().blockOf(at(support)) == B::Mud) {
        set(support, R().defaultState(B::Clay));
        return;
    }
    for (int k = 1; k <= 10; ++k) {
        const BlockPos q = dy(tip, -k);
        const BlockStateId here = at(q);
        if (here == 0) continue;
        const BlockId b = R().blockOf(here);
        if (b == B::Cauldron)
            set(q, water ? R().defaultState(B::WaterCauldron) : R().defaultState(B::LavaCauldron));
        else if (water && b == B::WaterCauldron && R().get(here, cauldronLevel) < 2)
            set(q, R().set(here, cauldronLevel, R().get(here, cauldronLevel) + 1));
        break;
    }
}

} // namespace mc::world
