// Random ticks (M15; wiki: Tick › Random tick, Grass Block, Leaves, Sapling, Snow, Ice).
// Part of BlockUpdates. Every game tick, each 16^3 section of the chunks within the
// simulation distance gets `random_tick_speed` (3) random positions; the block there
// acts if it is one that random-ticks. Sections without such blocks are skipped
// (vanilla counts them per section too).
#include "world/BlockUpdates.h"

#include "world/BlockShapes.h"
#include "world/Blocks.h"
#include "world/TreeFeature.h"
#include "world/Weather.h"

#include <algorithm>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }
BlockPos offset(const BlockPos& p, int dx, int dy, int dz) {
    return {p.x + dx, p.y + dy, p.z + dz};
}
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
    case B::BirchSapling:
        return {TreeKind::Birch, B::BirchLog, B::BirchLeaves};
    case B::SpruceSapling:
        return {TreeKind::Spruce, B::SpruceLog, B::SpruceLeaves};
    case B::AcaciaSapling:
        return {TreeKind::Acacia, B::AcaciaLog, B::AcaciaLeaves};
    case B::JungleSapling:
        return {TreeKind::Jungle, B::JungleLog, B::JungleLeaves};
    case B::DarkOakSapling:
        return {TreeKind::DarkOak, B::DarkOakLog, B::DarkOakLeaves};
    case B::CherrySapling:
        return {TreeKind::Cherry, B::CherryLog, B::CherryLeaves};
    default:
        return {TreeKind::Oak, B::OakLog, B::OakLeaves};
    }
}

bool isSapling(BlockId b) {
    return b == B::OakSapling || b == B::BirchSapling || b == B::SpruceSapling ||
           b == B::AcaciaSapling || b == B::JungleSapling || b == B::DarkOakSapling ||
           b == B::CherrySapling;
}

// Blocks a growing tree's logs may replace (vanilla: air, leaves, plants, saplings).
bool treeReplaceable(BlockStateId s) {
    const BlockId b = blockOf(s);
    return s == 0 || BlockUpdates::isLeaves(b) || BlockUpdates::isLog(b) || isSapling(b) ||
           b == B::ShortGrass || b == B::Fern || b == B::Snow;
}

} // namespace

bool BlockUpdates::plantableSoil(BlockStateId s) {
    const BlockId b = blockOf(s);
    // vanilla #dirt: dirt, grass, coarse dirt, podzol, mycelium (+ moss, rooted dirt, mud later)
    return b == B::Dirt || b == B::GrassBlock || b == B::CoarseDirt || b == B::Podzol ||
           b == B::Mycelium;
}

bool BlockUpdates::isLeaves(BlockId b) {
    return b == B::OakLeaves || b == B::BirchLeaves || b == B::SpruceLeaves ||
           b == B::AcaciaLeaves || b == B::JungleLeaves || b == B::DarkOakLeaves ||
           b == B::CherryLeaves;
}

bool BlockUpdates::isLog(BlockId b) {
    return b == B::OakLog || b == B::BirchLog || b == B::SpruceLog || b == B::AcaciaLog ||
           b == B::JungleLog || b == B::DarkOakLog || b == B::CherryLog;
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
    case B::GrassBlock:
    case B::Mycelium:
        tickGrass(p, b);
        break;
    case B::OakLeaves:
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
    case B::JungleLeaves:
    case B::DarkOakLeaves:
    case B::CherryLeaves:
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
        if (blockLightAt(p) > 10)
            set(p, m_world.isUltrawarm() ? BlockStateId{0} : R().defaultState(B::Water));
        break;
    case B::OakSapling:
    case B::BirchSapling:
    case B::SpruceSapling:
    case B::AcaciaSapling:
    case B::JungleSapling:
    case B::DarkOakSapling:
    case B::CherrySapling:
        // Light 9+ above, then a 1 in 7 chance to advance: stage 0 -> 1 -> a tree
        // (wiki: Sapling).
        if (rawBrightness(rel(p, Direction::Up)) >= 9 && m_random.nextInt(7) == 0) {
            if (R().get(s, stage) == 0)
                setRaw(p, R().set(s, stage, 1)); // (no updates, vanilla)
            else
                growTree(p, s);
        }
        break;
    case B::Farmland:
        tickFarmland(p, s);
        break;
    case B::NetherWart: // ages one step 1 random tick in 10, to 3 (wiki: Nether Wart)
        if (R().get(s, age3) < 3 && m_random.nextInt(10) == 0) set(p, R().set(s, age3, R().get(s, age3) + 1));
        break;
    case B::Cactus: {
        // As sugar cane: age +1 a random tick, a new piece on top at 15, 3 tall at most;
        // a piece that can't stand there breaks at once (wiki: Cactus).
        const BlockPos up{p.x, p.y + 1, p.z};
        if (!m_world.isInHeight(up.y) || at(up) != 0) break;
        int height = 1;
        while (height < 3 && blockOf(at({p.x, p.y - height, p.z})) == B::Cactus)
            ++height;
        if (height >= 3) break;
        const int a = R().get(s, age);
        if (a < 15) {
            setRaw(p, R().set(s, age, a + 1));
            break;
        }
        setRaw(p, R().set(s, age, 0));
        set(up, R().defaultState(B::Cactus));
        if (!cactusCanStay(m_world, up)) pop(up);
        break;
    }
    case B::BrownMushroom:
    case B::RedMushroom: {
        // 1 in 25: unless 5 of the same kind are within 9x3x9, wander 4 steps of +-1
        // and grow where it may stand (wiki: Mushroom › Spreading).
        if (m_random.nextInt(25) != 0) break;
        const BlockId kind = blockOf(s);
        int room = 5;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -4; dz <= 4; ++dz)
                for (int dx = -4; dx <= 4; ++dx)
                    if (blockOf(at({p.x + dx, p.y + dy, p.z + dz})) == kind && --room <= 0) return;
        auto step = [&](const BlockPos& from) {
            const int dx = int(m_random.nextInt(3)) - 1, dz = int(m_random.nextInt(3)) - 1;
            const int dy = int(m_random.nextInt(2)) - int(m_random.nextInt(2));
            return BlockPos{from.x + dx, from.y + dy, from.z + dz};
        };
        BlockPos from = p, to = step(p);
        for (int k = 0; k < 4; ++k) {
            if (m_world.isInHeight(to.y) && at(to) == 0 && mushroomCanStay(m_world, to)) from = to;
            to = step(from);
        }
        if (m_world.isInHeight(to.y) && at(to) == 0 && mushroomCanStay(m_world, to))
            set(to, R().defaultState(kind));
        break;
    }
    case B::SugarCane: {
        // Grows on the top piece: age +1 a random tick, a new piece at 15, 3 tall at
        // most (wiki: Sugar Cane).
        if (at({p.x, p.y + 1, p.z}) != 0) break;
        int height = 1;
        while (height < 3 && blockOf(at({p.x, p.y - height, p.z})) == B::SugarCane)
            ++height;
        if (height >= 3) break;
        const int a = R().get(s, age);
        if (a >= 15) {
            set({p.x, p.y + 1, p.z}, R().defaultState(B::SugarCane));
            setRaw(p, R().set(s, age, 0));
        } else {
            setRaw(p, R().set(s, age, a + 1));
        }
        break;
    }
    case B::Wheat:
    case B::Carrots:
    case B::Potatoes:
    case B::Beetroots:
        tickCrop(p, s);
        break;
    case B::Lava:
        lavaIgnites(p); // sources and flowing lava alike
        break;
    default:
        break;
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

void BlockUpdates::tickGrass(const BlockPos& p, BlockId kind) {
    // Mycelium spreads and dies exactly like grass (wiki: Mycelium › Spreading).
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
        if (above == B::Water || above == B::Lava)
            continue; // (the target's own light doesn't matter)
        const bool snowAbove = above == B::Snow;
        set(q, R().set(R().defaultState(kind), properties::snowy, snowAbove ? 0 : 1));
    }
}

// --- Leaves ------------------------------------------------------------------------

int BlockUpdates::leafDistance(const BlockPos& p) const {
    // Steps to the nearest log through leaves: 1 next to a log, up to 7 (wiki: Leaves ›
    // Block states).
    int best = 7;
    for (int d = 0; d < kDirectionCount; ++d) {
        const BlockPos q = rel(p, static_cast<Direction>(d));
        if (m_world.isInHeight(q.y) && !chunkAt(q))
            return R().get(at(p), distance) + 1; // unknown: keep
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

bool BlockUpdates::growTree(const BlockPos& sapPos, BlockStateId sapling) {
    TreeBlocks t = treeOf(blockOf(sapling));
    // Jungle and dark oak saplings in a 2x2 square grow one big tree from its corner;
    // a lone dark oak sapling never grows (wiki: Sapling › Growth).
    BlockPos p = sapPos;
    bool square = false;
    if (t.kind == TreeKind::Jungle || t.kind == TreeKind::DarkOak) {
        const BlockId kind = blockOf(sapling);
        for (const auto& c : {std::array{0, 0}, std::array{-1, 0}, std::array{0, -1}, std::array{-1, -1}}) {
            const BlockPos corner{sapPos.x + c[0], sapPos.y, sapPos.z + c[1]};
            bool all = true;
            for (int k = 0; k < 4 && all; ++k)
                all = blockOf(at({corner.x + (k & 1), corner.y, corner.z + (k >> 1)})) == kind;
            if (all) {
                p = corner;
                square = true;
                break;
            }
        }
        if (t.kind == TreeKind::Jungle && square) t.kind = TreeKind::MegaJungle;
        if (t.kind == TreeKind::DarkOak && !square) return false;
    }
    const int height = treeHeight(t.kind, m_random);
    const uint64_t shapeSeed = m_random.nextLong();
    // Room to grow: every log position must be free (air, leaves, plants, its own
    // saplings) and the tree inside the world (wiki: Sapling - otherwise it stays a
    // sapling and tries again later).
    bool fits = p.y + height + 2 <= m_world.height().maxY();
    Xoroshiro check(shapeSeed);
    treeShape(t.kind, p.x, p.y, p.z, height, check, [&](int32_t x, int32_t y, int32_t z, int dist) {
        if (dist == 0 && fits && !treeReplaceable(at({x, y, z}))) fits = false;
    });
    if (!fits) return false;
    for (int k = 0; k < (square ? 4 : 1); ++k)
        setRaw({p.x + (k & 1), p.y, p.z + (k >> 1)}, 0);
    const BlockStateId log = R().defaultState(t.log);                             // axis y
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
    for (int k = 0; k < (square ? 4 : 1); ++k) {
        const BlockPos ground{p.x + (k & 1), p.y - 1, p.z + (k >> 1)};
        if (blockOf(at(ground)) == B::GrassBlock) set(ground, R().defaultState(B::Dirt));
    }
    return true;
}

} // namespace mc::world

namespace mc::world {

bool BlockUpdates::rainingNear(const BlockPos& p) const {
    if (!m_weather || !m_weather->raining) return false;
    return rainingAt(m_world, *m_weather, p) || rainingAt(m_world, *m_weather, rel(p, Direction::West)) ||
           rainingAt(m_world, *m_weather, rel(p, Direction::East)) ||
           rainingAt(m_world, *m_weather, rel(p, Direction::North)) ||
           rainingAt(m_world, *m_weather, rel(p, Direction::South));
}

void BlockUpdates::strikeLightning(const BlockPos& p) {
    if (m_lightning.size() < m_lightning.capacity()) m_lightning.push_back(p);
    // Fire where it lands and up to 4 more within a block (Normal difficulty; wiki:
    // Lightning › Fire), where fire could stay.
    auto ignite = [&](const BlockPos& q) {
        if (m_world.isInHeight(q.y) && at(q) == 0 && fireCanStay(m_world, q)) set(q, fireState(0));
    };
    ignite(p);
    for (int i = 0; i < 4; ++i)
        ignite({p.x + int(m_random.nextInt(3)) - 1, p.y + int(m_random.nextInt(3)) - 1, p.z + int(m_random.nextInt(3)) - 1});
}

// Vanilla's per-chunk weather work (ServerLevel tickChunk): in a thunderstorm 1 in
// 100,000 ticks a bolt strikes the top of a random column where it rains; 1 in 16
// ticks the top of a random column is checked - still water under the open sky in a
// cold biome freezes (if it borders something other than water), and while it snows
// a snow layer settles on air over a solid top (block light below 10).
void BlockUpdates::runWeatherTicks() {
    if (!m_weather || m_rtDistance < 0 || !m_world.hasSkyLight()) return;
    const bool storm = m_weather->raining && m_weather->thunder > 0.9f;
    for (int32_t cz = m_rtCentre.z - m_rtDistance; cz <= m_rtCentre.z + m_rtDistance; ++cz)
        for (int32_t cx = m_rtCentre.x - m_rtDistance; cx <= m_rtCentre.x + m_rtDistance; ++cx) {
            if (!m_world.chunk({cx, cz})) continue;
            if (storm && m_random.nextInt(100000) == 0) {
                const int32_t x = cx * 16 + int(m_random.nextInt(16)), z = cz * 16 + int(m_random.nextInt(16));
                const BlockPos top{x, rainHeight(m_world, x, z), z};
                if (rainFallsOn(m_world, *m_weather, top)) strikeLightning(top);
            }
            // Vanilla: random_tick_speed tries of 1 in 48 (1 in 16 a tick at the default 3).
            bool picked = false;
            for (int i = 0; i < m_rtSpeed && !picked; ++i)
                picked = m_random.nextInt(48) == 0;
            if (!picked) continue;
            const int32_t x = cx * 16 + int(m_random.nextInt(16)), z = cz * 16 + int(m_random.nextInt(16));
            const BlockPos top{x, rainHeight(m_world, x, z), z};
            const BlockPos below{x, top.y - 1, z};
            if (!m_world.isInHeight(below.y) || precipitationAt(m_world, below) != Precipitation::Snow) continue;
            const Chunk* bc = chunkAt(below);
            if (!bc) continue;
            const BlockStateId ws = at(below);
            if (blockOf(ws) == B::Water && R().get(ws, properties::level) == 0 &&
                bc->blockLight(blockToLocal(x), below.y, blockToLocal(z)) < 10) {
                bool edge = false;
                for (const Direction d : {Direction::West, Direction::East, Direction::North, Direction::South})
                    edge = edge || blockOf(at(rel(below, d))) != B::Water;
                if (edge) set(below, R().defaultState(B::Ice));
            }
            if (m_weather->raining && m_world.isInHeight(top.y) && at(top) == 0 &&
                bc->blockLight(blockToLocal(x), std::min(top.y, m_world.height().maxY()), blockToLocal(z)) < 10) {
                const BlockStateId under = at(below);
                const BlockId ub = blockOf(under);
                // On a full solid top (glass too) or leaves, not on ice (wiki: Snow).
                const BlockShape& shape = collisionShape(under);
                const bool fullTop = R().collides(under) &&
                                     (shape.count == 0 ? true
                                                       : shape.count == 1 && shape.boxes[0].to[1] == 16 &&
                                                             shape.boxes[0].from[0] == 0 && shape.boxes[0].to[0] == 16 &&
                                                             shape.boxes[0].from[2] == 0 && shape.boxes[0].to[2] == 16);
                if ((fullTop || isLeaves(ub)) && ub != B::Ice && ub != B::PackedIce)
                    set(top, R().defaultState(B::Snow));
            }
        }
}

} // namespace mc::world
