// Lush cave blocks (M27.2; wiki: Cave Vines, Spore Blossom, Azalea, Rooted Dirt, Hanging
// Roots, Small Dripleaf, Big Dripleaf, Moss Block). Part of BlockUpdates: what holds each
// one up, glow berries, bone meal (azalea trees, dripleaves, moss patches) and big
// dripleaves tipping under whoever stands on them.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"
#include "world/TreeFeature.h"

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockPos rel(const BlockPos& p, Direction d) {
    const glm::ivec3 v = normal(d);
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}
BlockPos up(const BlockPos& p) { return {p.x, p.y + 1, p.z}; }
BlockPos down(const BlockPos& p) { return {p.x, p.y - 1, p.z}; }
bool vines(BlockId b) { return b == B::CaveVines || b == B::CaveVinesPlant; }
bool dripleafPart(BlockId b) { return b == B::BigDripleaf || b == B::BigDripleafStem; }

} // namespace

bool BlockUpdates::dripleafSoil(BlockStateId s) { return plantableSoil(s) || R().blockOf(s) == B::Clay; }

bool BlockUpdates::lushNeighbourChanged(const BlockPos& p, BlockStateId s) {
    const BlockId b = R().blockOf(s);
    switch (b) {
    case B::CaveVines:
    case B::CaveVinesPlant: {
        // Hang from a block's underside or more vine; the lowest piece is the growing tip.
        const BlockId above = R().blockOf(at(up(p)));
        if (!vines(above) && !R().collides(at(up(p)))) {
            pop(p);
            return true;
        }
        const bool below = vines(R().blockOf(at(down(p))));
        const int fruit = R().get(s, berries);
        if (b == B::CaveVines && below) set(p, R().set(R().defaultState(B::CaveVinesPlant), berries, fruit));
        else if (b == B::CaveVinesPlant && !below)
            set(p, R().set(R().set(R().defaultState(B::CaveVines), berries, fruit), age25, 25));
        return true;
    }
    case B::SporeBlossom:
    case B::HangingRoots:
        if (!R().collides(at(up(p)))) pop(p); // (on a block's underside)
        return true;
    case B::Azalea:
    case B::FloweringAzalea:
        if (!dripleafSoil(at(down(p)))) pop(p); // (dirt, moss or clay)
        return true;
    case B::BigDripleaf:
    case B::BigDripleafStem: {
        // A leaf on a stem (or the ground); a stem under a leaf or more stem.
        const BlockStateId below = at(down(p));
        const BlockId bb = R().blockOf(below);
        const bool held = bb == B::BigDripleafStem || dripleafSoil(below);
        if (!held) {
            pop(p);
            return true;
        }
        if (b == B::BigDripleafStem && !dripleafPart(R().blockOf(at(up(p))))) {
            // (its leaf was taken: the stem becomes the leaf)
            set(p, R().set(R().defaultState(B::BigDripleaf), facing, R().get(s, facing)));
        } else if (b == B::BigDripleaf && dripleafPart(R().blockOf(at(up(p))))) {
            set(p, R().set(R().defaultState(B::BigDripleafStem), facing, R().get(s, facing)));
        }
        return true;
    }
    default: return false;
    }
}

void BlockUpdates::tickCaveVines(const BlockPos& p, BlockStateId s) {
    // The tip grows a piece down 1 in 10 random ticks until age 25; the new piece bears
    // berries 11% of the time (wiki: Cave Vines).
    if (R().get(s, age25) >= 25 || m_random.nextInt(10) != 0) return;
    const BlockPos below = down(p);
    if (at(below) != 0) return;
    BlockStateId tip = R().set(R().defaultState(B::CaveVines), age25, R().get(s, age25) + 1);
    if (m_random.nextFloat() < 0.11f) tip = R().set(tip, berries, 0);
    set(below, tip);
}

void BlockUpdates::tiltDripleaf(const BlockPos& p) {
    // Stood on, a big dripleaf tips: unstable, then after 10 ticks partly, after 10 more
    // fully (nothing holds you), and springs back 100 ticks later (wiki: Big Dripleaf).
    const BlockStateId s = at(p);
    if (R().blockOf(s) != B::BigDripleaf || R().get(s, tilt) != 0) return;
    set(p, R().set(s, tilt, 1));
    schedule(p, B::BigDripleaf, 10, 0);
}

bool BlockUpdates::tickDripleaf(const BlockPos& p, BlockStateId s) {
    if (R().blockOf(s) != B::BigDripleaf) return false;
    const int t = R().get(s, tilt);
    if (t == 0) return true;
    set(p, R().set(s, tilt, (t + 1) % 4));
    if (t < 3) schedule(p, B::BigDripleaf, t == 2 ? 100 : 10, 0);
    return true;
}

bool BlockUpdates::lushBoneMeal(const BlockPos& p) {
    const BlockStateId s = at(p);
    switch (R().blockOf(s)) {
    case B::CaveVines:
    case B::CaveVinesPlant: // berries at once
        if (R().get(s, berries) == 0) return false;
        set(p, R().set(s, berries, 0));
        return true;
    case B::Azalea:
    case B::FloweringAzalea:
        if (m_random.nextFloat() < 0.45f) growAzaleaTree(p);
        return true;
    case B::RootedDirt: // hanging roots under it
        if (at(down(p)) != 0) return false;
        set(down(p), R().defaultState(B::HangingRoots));
        return true;
    case B::SmallDripleaf: { // a big dripleaf 2-5 tall
        const BlockPos base = R().get(s, doorHalf) == 0 ? down(p) : p;
        const int height = 2 + int(m_random.nextInt(4));
        int room = 0;
        for (int k = 2; k < height && (at({base.x, base.y + k, base.z}) == 0); ++k) room = k;
        const int top = std::max(1, room);
        const int f = R().get(s, facing);
        for (int k = 0; k <= top; ++k)
            setRaw({base.x, base.y + k, base.z},
                   R().set(R().defaultState(k == top ? B::BigDripleaf : B::BigDripleafStem), facing, f));
        return true;
    }
    case B::BigDripleaf:
    case B::BigDripleafStem: { // one taller, if there's room over the leaf
        BlockPos leaf = p;
        while (R().blockOf(at(leaf)) == B::BigDripleafStem) leaf = up(leaf);
        if (R().blockOf(at(leaf)) != B::BigDripleaf || at(up(leaf)) != 0) return false;
        const int f = R().get(at(leaf), facing);
        setRaw(up(leaf), R().set(R().defaultState(B::BigDripleaf), facing, f));
        setRaw(leaf, R().set(R().defaultState(B::BigDripleafStem), facing, f));
        return true;
    }
    case B::MossBlock: { // a moss patch: stone and dirt around turn to moss, plants on it
        for (int i = 0; i < 40; ++i) {
            const BlockPos q{p.x + int(m_random.nextInt(7)) - 3, p.y + int(m_random.nextInt(3)) - 1,
                             p.z + int(m_random.nextInt(7)) - 3};
            const BlockId g = R().blockOf(at(q));
            const bool base = g == B::Stone || g == B::Dirt || g == B::GrassBlock || g == B::Granite ||
                              g == B::Diorite || g == B::Andesite || g == B::Tuff || g == B::Deepslate;
            if (!base || at(up(q)) != 0) continue;
            set(q, R().defaultState(B::MossBlock));
            const uint32_t r = m_random.nextInt(100);
            if (r < 30) set(up(q), R().defaultState(B::MossCarpet));
            else if (r < 50) set(up(q), R().defaultState(B::ShortGrass));
            else if (r < 53) set(up(q), R().defaultState(r < 52 ? B::Azalea : B::FloweringAzalea));
        }
        return true;
    }
    default: return false;
    }
}

void BlockUpdates::growAzaleaTree(const BlockPos& p) {
    // An azalea grows into an azalea tree: an oak's shape with azalea leaves, a quarter of
    // them in flower, standing on rooted dirt (wiki: Azalea Tree).
    const BlockStateId log = R().defaultState(B::OakLog);
    const BlockStateId leaves = R().defaultState(B::AzaleaLeaves), flowering = R().defaultState(B::FloweringAzaleaLeaves);
    const int height = 4 + int(m_random.nextInt(2));
    for (int k = 1; k <= height + 1; ++k)
        if (at({p.x, p.y + k, p.z}) != 0) return; // (no room)
    Xoroshiro shape(m_random.nextLong());
    treeShape(TreeKind::Oak, p.x, p.y, p.z, height, shape, [&](int32_t x, int32_t y, int32_t z, int dist) {
        const BlockPos q{x, y, z};
        if (dist == 0) {
            setRaw(q, log);
            return;
        }
        if (at(q) != 0) return;
        const BlockStateId leaf = shape.nextInt(4) == 0 ? flowering : leaves;
        setRaw(q, R().set(leaf, properties::distance, dist - 1));
    });
    const BlockId g = R().blockOf(at(down(p)));
    if (g == B::Dirt || g == B::GrassBlock || g == B::MossBlock) setRaw(down(p), R().defaultState(B::RootedDirt));
}

} // namespace mc::world
