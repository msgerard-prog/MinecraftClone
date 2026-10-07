// Random ticks (M15; wiki: Tick › Random tick, Grass Block, Leaves, Sapling, Snow, Ice).
// Part of BlockUpdates. Every game tick, each 16^3 section of the chunks within the
// simulation distance gets `random_tick_speed` (3) random positions; the block there
// acts if it is one that random-ticks. Sections without such blocks are skipped
// (vanilla counts them per section too).
#include "world/BlockUpdates.h"

#include "world/Blocks.h"
#include "world/TreeFeature.h"

#include <algorithm>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }
BlockPos offset(const BlockPos& p, int dx, int dy, int dz) { return {p.x + dx, p.y + dy, p.z + dz}; }
BlockPos rel(const BlockPos& p, Direction d) {
    const glm::ivec3 v = normal(d);
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}

struct TreeBlocks {
    TreeKind kind;
    BlockId log, leaves;
};
TreeBlocks treeOf(BlockId sapling) {
    switch (sapling) {
    case B::BirchSapling: return {TreeKind::Birch, B::BirchLog, B::BirchLeaves};
    case B::SpruceSapling: return {TreeKind::Spruce, B::SpruceLog, B::SpruceLeaves};
    case B::AcaciaSapling: return {TreeKind::Acacia, B::AcaciaLog, B::AcaciaLeaves};
    default: return {TreeKind::Oak, B::OakLog, B::OakLeaves};
    }
}

bool isSapling(BlockId b) {
    return b == B::OakSapling || b == B::BirchSapling || b == B::SpruceSapling || b == B::AcaciaSapling;
}

// Blocks a growing tree's logs may replace (vanilla: air, leaves, plants, saplings).
bool treeReplaceable(BlockStateId s) {
    const BlockId b = blockOf(s);
    return s == 0 || BlockUpdates::isLeaves(b) || BlockUpdates::isLog(b) || isSapling(b) || b == B::ShortGrass ||
           b == B::Fern || b == B::Snow;
}

} // namespace

bool BlockUpdates::plantableSoil(BlockStateId s) {
    const BlockId b = blockOf(s);
    return b == B::Dirt || b == B::GrassBlock || b == B::CoarseDirt; // (+ podzol, moss... when added)
}

bool BlockUpdates::isLeaves(BlockId b) {
    return b == B::OakLeaves || b == B::BirchLeaves || b == B::SpruceLeaves || b == B::AcaciaLeaves;
}

bool BlockUpdates::isLog(BlockId b) {
    return b == B::OakLog || b == B::BirchLog || b == B::SpruceLog || b == B::AcaciaLog;
}

int BlockUpdates::blockLightAt(const BlockPos& p) const {
    const Chunk* c = chunkAt(p);
    if (!c || !c->lit() || !m_world.isInHeight(p.y)) return 0;
    return c->blockLight(blockToLocal(p.x), p.y, blockToLocal(p.z));
}

int BlockUpdates::rawBrightness(const BlockPos& p) const {
    // Vanilla's "raw brightness": the brighter of block light and sky light dimmed by
    // the time of day (so grass and saplings don't grow by night in the open).
    const Chunk* c = chunkAt(p);
    if (!c || !c->lit()) return 0;
    if (p.y > m_world.height().maxY()) return m_world.hasSkyLight() ? 15 - m_skyDarken : 0;
    if (!m_world.isInHeight(p.y)) return 0;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    const int sky = m_world.hasSkyLight() ? c->skyLight(x, p.y, z) - m_skyDarken : 0;
    return std::max<int>(c->blockLight(x, p.y, z), sky);
}

void BlockUpdates::runRandomTicks() {
    if (m_rtDistance < 0 || m_rtSpeed <= 0) return;
    const HeightRange& h = m_world.height();
    for (int32_t cz = m_rtCentre.z - m_rtDistance; cz <= m_rtCentre.z + m_rtDistance; ++cz)
        for (int32_t cx = m_rtCentre.x - m_rtDistance; cx <= m_rtCentre.x + m_rtDistance; ++cx) {
            const Chunk* c = chunkAt({cx * 16, h.minY, cz * 16});
            if (!c) continue;
            bool any = false;
            for (int s = 0; s < c->sectionCount() && !any; ++s)
                any = c->section(s).randomTickingCount() > 0;
            if (!any) continue;
            // Only chunks whose 8 neighbours are loaded tick (vanilla's entity-ticking
            // chunks): blocks next door must not read as air.
            bool surrounded = true;
            for (int dz = -1; dz <= 1 && surrounded; ++dz)
                for (int dx = -1; dx <= 1 && surrounded; ++dx)
                    surrounded = m_world.chunk({cx + dx, cz + dz}) != nullptr;
            if (!surrounded) continue;
            for (int s = 0; s < c->sectionCount(); ++s) {
                if (c->section(s).randomTickingCount() == 0) continue;
                for (int i = 0; i < m_rtSpeed; ++i) {
                    const uint64_t r = m_random.nextLong();
                    const int x = int(r & 15), y = int((r >> 4) & 15), z = int((r >> 8) & 15);
                    // (Re-read the section: an earlier tick may have replaced it.)
                    const Chunk* cc = chunkAt({cx * 16, h.minY, cz * 16});
                    if (!cc) break;
                    const BlockStateId state = cc->section(s).get(x, y, z);
                    if (R().randomTicks(state))
                        randomTick({cx * 16 + x, h.minY + s * 16 + y, cz * 16 + z}, state);
                }
            }
        }
}

void BlockUpdates::randomTick(const BlockPos& p, BlockStateId s) {
    const BlockId b = blockOf(s);
    switch (b) {
    case B::GrassBlock: tickGrass(p); break;
    case B::OakLeaves:
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
        // Leaves without a log within 6 blocks decay, dropping their loot (wiki: Leaves).
        // Only states with distance 7, not persistent, random-tick at all.
        m_drops.push_back({p, {}, s});
        set(p, 0);
        break;
    case B::Snow:
        // Snow layers melt in block light above 11 (wiki: Snow), dropping nothing.
        if (blockLightAt(p) > 11) set(p, 0);
        break;
    case B::Ice:
        // Ice melts when the block light next to it is above 11 (wiki: Ice): ice lets
        // light through with 1 lost, so above 10 at the ice. Into water, or nothing in
        // the Nether.
        if (blockLightAt(p) > 10) set(p, m_world.isUltrawarm() ? BlockStateId{0} : R().defaultState(B::Water));
        break;
    case B::OakSapling:
    case B::BirchSapling:
    case B::SpruceSapling:
    case B::AcaciaSapling:
        // Light 9+ above, then a 1 in 7 chance to advance: stage 0 -> 1 -> a tree
        // (wiki: Sapling).
        if (rawBrightness(rel(p, Direction::Up)) >= 9 && m_random.nextInt(7) == 0) {
            if (R().get(s, stage) == 0) setRaw(p, R().set(s, stage, 1)); // (no updates, vanilla)
            else growTree(p, s);
        }
        break;
    case B::Lava:
        lavaIgnites(p); // sources and flowing lava alike
        break;
    default: break;
    }
}

// --- Grass -------------------------------------------------------------------------

bool BlockUpdates::grassSurvives(const BlockPos& p) const {
    // Grass needs light through the block above: a thin snow layer is fine; water or
    // an opaque block (light opacity 15) turns it to dirt (wiki: Grass Block).
    const BlockStateId above = at(rel(p, Direction::Up));
    const BlockId a = blockOf(above);
    if (a == B::Snow) return R().get(above, layers) == 0; // thicker snow smothers it (wiki: Snow)
    if (a == B::Water && fluidAmount(above) == 8) return false;
    return R().lightOpacity(above) < 15;
}

void BlockUpdates::tickGrass(const BlockPos& p) {
    if (!grassSurvives(p)) {
        set(p, R().defaultState(B::Dirt));
        return;
    }
    // Light 9+ above: 4 tries to spread to dirt within x/z ±1, y -3..+1 (wiki: Grass
    // Block › Spread) that could keep grass alive (and isn't under water).
    if (rawBrightness(rel(p, Direction::Up)) < 9) return;
    for (int i = 0; i < 4; ++i) {
        const BlockPos q = offset(p, int(m_random.nextInt(3)) - 1, int(m_random.nextInt(5)) - 3,
                                  int(m_random.nextInt(3)) - 1);
        if (blockOf(at(q)) != B::Dirt || !grassSurvives(q)) continue;
        const BlockId above = blockOf(at(rel(q, Direction::Up)));
        if (above == B::Water || above == B::Lava) continue; // (the target's own light doesn't matter)
        const bool snowAbove = above == B::Snow;
        set(q, R().set(R().defaultState(B::GrassBlock), properties::snowy, snowAbove ? 0 : 1));
    }
}

// --- Leaves ------------------------------------------------------------------------

int BlockUpdates::leafDistance(const BlockPos& p) const {
    // Steps to the nearest log through leaves: 1 next to a log, up to 7 (wiki: Leaves ›
    // Block states).
    int best = 7;
    for (int d = 0; d < kDirectionCount; ++d) {
        const BlockPos q = rel(p, static_cast<Direction>(d));
        if (m_world.isInHeight(q.y) && !chunkAt(q)) return R().get(at(p), distance) + 1; // unknown: keep
        const BlockStateId n = at(q);
        const BlockId b = blockOf(n);
        if (isLog(b)) return 1;
        if (isLeaves(b)) best = std::min(best, R().get(n, distance) + 1 + 1); // value index 0 = "1"
    }
    return best;
}

void BlockUpdates::leavesChanged(const BlockPos& p, BlockStateId s) {
    // A neighbour changed: if the distance would change, update it next tick (vanilla
    // schedules a 1-tick update, so long chains of leaves update one step per tick).
    if (leafDistance(p) != R().get(s, distance) + 1) schedule(p, blockOf(s), 1, 0);
}

// --- Saplings ----------------------------------------------------------------------

bool BlockUpdates::growTree(const BlockPos& p, BlockStateId sapling) {
    const TreeBlocks t = treeOf(blockOf(sapling));
    const int height = treeHeight(t.kind, m_random);
    const uint64_t shapeSeed = m_random.nextLong();
    // Room to grow: every log position must be free (air, leaves, plants) and inside
    // the world (wiki: Sapling - otherwise it stays a sapling and tries again later).
    bool fits = p.y + height <= m_world.height().maxY();
    Xoroshiro check(shapeSeed);
    treeShape(t.kind, p.x, p.y, p.z, height, check, [&](int32_t x, int32_t y, int32_t z, int dist) {
        if (dist == 0 && fits && !((x == p.x && y == p.y && z == p.z) || treeReplaceable(at({x, y, z}))))
            fits = false;
    });
    if (!fits) return false;
    setRaw(p, 0);
    const BlockStateId log = R().defaultState(t.log); // axis y
    const BlockStateId leaf = R().set(R().defaultState(t.leaves), persistent, 1); // not persistent
    Xoroshiro shape(shapeSeed);
    treeShape(t.kind, p.x, p.y, p.z, height, shape, [&](int32_t x, int32_t y, int32_t z, int dist) {
        const BlockPos q{x, y, z};
        if (!m_world.isInHeight(y)) return;
        if (dist == 0) {
            if (treeReplaceable(at(q))) set(q, log);
        } else if (at(q) == 0 || blockOf(at(q)) == B::ShortGrass || blockOf(at(q)) == B::Fern) {
            set(q, R().set(leaf, distance, dist - 1));
        }
    });
    // The ground under the trunk becomes dirt (vanilla).
    const BlockPos ground = rel(p, Direction::Down);
    if (blockOf(at(ground)) == B::GrassBlock) set(ground, R().defaultState(B::Dirt));
    return true;
}

} // namespace mc::world
