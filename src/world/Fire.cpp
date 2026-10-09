// Fire (M15; wiki: Fire, Lava). Part of BlockUpdates.
//
// A fire block ticks every 30-40 game ticks (scheduled, not random). Each tick it may
// age (1 in 3), goes out if it can't stay, burns away its flammable neighbours and
// spreads to air around it that touches something flammable: 3x3 around, from 1 below
// to 4 above, less likely the higher and the older the fire. Netherrack, magma
// blocks and soul sand under it keep it burning forever (infiniburn).
//
// Lava starts fires on its random ticks: in air 1 or 2 blocks above it (3x3, then
// 5x5) next to something flammable, or on top of a flammable block beside it.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

#include <algorithm>

#include <vector>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }
bool netherWood(BlockId b) { // (and copper: copper doors behave like wooden ones but never burn)
    const std::string_view id = R().block(b).id;
    return id.starts_with("minecraft:crimson_") || id.starts_with("minecraft:warped_") ||
           id.find("copper") != std::string_view::npos;
}
BlockPos rel(const BlockPos& p, Direction d) {
    const glm::ivec3 v = normal(d);
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}

// Vanilla's default difficulty (Normal) until difficulty exists: it speeds up spread.
constexpr int kDifficulty = 2;

// Blocks fire burns on forever (wiki: Fire › Eternal fire); bedrock too in the End
// (the dimension with neither sky light nor ultrawarm).
bool infiniburn(const World& w, BlockId b) {
    return b == B::Netherrack || b == B::MagmaBlock || b == B::SoulSand ||
           (b == B::Bedrock && !w.hasSkyLight() && !w.isUltrawarm());
}

// Blocks lava can set alight (wiki: Fire, "can catch fire from lava"): wood and leaves,
// also wood-built blocks that don't burn (crafting table); not 1-block flowers.
bool ignitedByLava(BlockId b) {
    switch (b) {
    case B::Dandelion:
    case B::Poppy:
    case B::Cornflower:
    case B::AzureBluet:
    case B::OxeyeDaisy:
        return false;
    case B::CraftingTable:
        return true;
    default:
        return BlockUpdates::igniteOdds(b) > 0;
    }
}

} // namespace

namespace {

int igniteOddsSlow(BlockId b) {
    // How readily fire spreads next to the block (wiki: Fire › Flammable blocks, the
    // "ignite odds" column). Wood sets burn like their oak versions (M23.3), but
    // crimson and warped wood never burns.
    if (netherWood(b)) return 0;
    switch (R().likeOf(b)) {
    case B::OakPlanks:
    case B::BirchPlanks:
    case B::SprucePlanks:
    case B::AcaciaPlanks:
    case B::JunglePlanks:
    case B::DarkOakPlanks:
    case B::CherryPlanks:
    case B::OakLog:
    case B::BirchLog:
    case B::SpruceLog:
    case B::AcaciaLog:
    case B::JungleLog:
    case B::DarkOakLog:
    case B::CherryLog:
        return 5;
    case B::OakLeaves:
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
    case B::JungleLeaves:
    case B::DarkOakLeaves:
    case B::CherryLeaves:
    case B::RedPoplarLeaves: // (M33 review: 26.3)
    case B::OrangePoplarLeaves:
    case B::YellowPoplarLeaves:
        return 30;
    case B::ShortGrass:
    case B::Fern:
    case B::Dandelion:
    case B::Poppy:
    case B::Cornflower:
    case B::AzureBluet:
    case B::OxeyeDaisy:
    case B::DeadBush:
        return 60;
    case B::Tnt: // (wiki: TNT - encouragement 15, flammability 100)
        return 15;
    default: {
        if (b >= B::WhiteWool && b <= B::BlackWool) return 30; // wool: 30 / 60 like leaves (wiki)
        // M23: every wood's logs, planks, leaves; wooden slabs and stairs like their
        // planks; carpets like wool's plants (60).
        const BlockSettings& st = R().block(b).settings;
        if (st.kind == BlockKind::Carpet) return 60;
        if (st.kind != BlockKind::Plain && st.base != 0) return igniteOddsSlow(st.base);
        if (BlockUpdates::isLeaves(b)) return 30;
        if (BlockUpdates::isLog(b) || R().block(b).id.ends_with("_planks") || b == B::BambooMosaic) return 5;
        return 0;
    }
    }
}

int burnOddsSlow(BlockId b) {
    // How quickly fire destroys the block (wiki: Fire, the "burn odds" column).
    if (netherWood(b)) return 0;
    switch (R().likeOf(b)) {
    case B::OakPlanks:
    case B::BirchPlanks:
    case B::SprucePlanks:
    case B::AcaciaPlanks:
    case B::JunglePlanks:
    case B::DarkOakPlanks:
    case B::CherryPlanks:
        return 20;
    case B::OakLog:
    case B::BirchLog:
    case B::SpruceLog:
    case B::AcaciaLog:
    case B::JungleLog:
    case B::DarkOakLog:
    case B::CherryLog:
        return 5;
    case B::OakLeaves:
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
    case B::JungleLeaves:
    case B::DarkOakLeaves:
    case B::CherryLeaves:
    case B::RedPoplarLeaves: // (M33 review: 26.3)
    case B::OrangePoplarLeaves:
    case B::YellowPoplarLeaves:
        return 60;
    case B::ShortGrass:
    case B::Fern:
    case B::Dandelion:
    case B::Poppy:
    case B::Cornflower:
    case B::AzureBluet:
    case B::OxeyeDaisy:
    case B::DeadBush:
    case B::Tnt:
        return 100;
    default: {
        if (b >= B::WhiteWool && b <= B::BlackWool) return 60;
        const BlockSettings& st = R().block(b).settings;
        if (st.kind == BlockKind::Carpet) return 20;
        if (st.kind != BlockKind::Plain && st.base != 0) return burnOddsSlow(st.base);
        if (BlockUpdates::isLeaves(b)) return 60;
        if (BlockUpdates::isLog(b)) return 5;
        if (R().block(b).id.ends_with("_planks") || b == B::BambooMosaic) return 20;
        return 0;
    }
    }
}

// Both odds per block, worked out once (M23 perf review: the rules above read names).
struct FireOdds {
    std::vector<uint8_t> ignite, burn;
    FireOdds() {
        ignite.resize(R().blockCount());
        burn.resize(R().blockCount());
        for (BlockId b = 0; b < R().blockCount(); ++b) {
            ignite[b] = static_cast<uint8_t>(igniteOddsSlow(b));
            burn[b] = static_cast<uint8_t>(burnOddsSlow(b));
        }
    }
};
const FireOdds& fireOdds() {
    static const FireOdds t;
    return t;
}

} // namespace

int BlockUpdates::igniteOdds(BlockId b) { return fireOdds().ignite[b]; }
int BlockUpdates::burnOdds(BlockId b) { return fireOdds().burn[b]; }

bool BlockUpdates::nextToFlammable(const BlockPos& p) const {
    for (int d = 0; d < kDirectionCount; ++d)
        if (igniteOdds(blockOf(at(rel(p, static_cast<Direction>(d))))) > 0) return true;
    return false;
}

bool BlockUpdates::fireCanStay(const World& world, const BlockPos& p) {
    // On a solid block, or clinging to something flammable (wiki: Fire › Placement).
    if (R().collides(world.getBlock(rel(p, Direction::Down)))) return true;
    for (int d = 0; d < kDirectionCount; ++d)
        if (igniteOdds(blockOf(world.getBlock(rel(p, static_cast<Direction>(d))))) > 0) return true;
    return false;
}

bool BlockUpdates::fireSurvives(const BlockPos& p) const {
    return R().collides(at(rel(p, Direction::Down))) || nextToFlammable(p);
}

bool BlockUpdates::nearPlayer(const BlockPos& p) const {
    // 1.21.11: fire only burns, spreads or goes out within 128 blocks of a player
    // (game rule fire_spread_radius_around_player; wiki: Fire › Ticking).
    if (!m_player) return true;
    const double dx = p.x + 0.5 - m_player->x, dz = p.z + 0.5 - m_player->z;
    return dx * dx + dz * dz <= double(kFireRadius) * kFireRadius;
}

BlockStateId BlockUpdates::fireState(int fireAge) {
    return R().set(R().defaultState(B::Fire), age, std::min(fireAge, 15));
}

void BlockUpdates::placeFire(const BlockPos& p, int fireAge) {
    // (M29.4c; wiki: Soul Fire) fire lit on soul sand or soul soil is soul fire: it never
    // spreads or burns out, so it has no ticks; it goes when its soul block does.
    if (const BlockId below = R().blockOf(at(rel(p, Direction::Down))); below == B::SoulSand || below == B::SoulSoil) {
        set(p, R().defaultState(B::SoulFire));
        return;
    }
    set(p, fireState(fireAge));
    schedule(p, B::Fire, 30 + static_cast<int>(m_random.nextInt(10)), 0); // its own first tick
}

void BlockUpdates::fireNeighbourChanged(const BlockPos& p) {
    if (!fireSurvives(p)) {
        set(p, 0);
        return;
    }
    schedule(p, B::Fire, 30 + static_cast<int>(m_random.nextInt(10)), 0); // (if none pending)
}

void BlockUpdates::burnNeighbour(const BlockPos& q, int bound, int fireAge) {
    // The neighbour burns away with a chance of its burn odds out of `bound` (300 at the
    // sides, 250 above and below: public write-ups; the wiki gives the odds only); a
    // young fire often takes its place.
    const int odds = burnOdds(blockOf(at(q)));
    if (odds == 0 || static_cast<int>(m_random.nextInt(uint32_t(bound))) >= odds) return;
    if (blockOf(at(q)) == B::Tnt) { // burning TNT is lit (wiki: TNT)
        primeTnt(q);
        return;
    }
    if (static_cast<int>(m_random.nextInt(uint32_t(fireAge + 10))) < 5)
        placeFire(q, fireAge + static_cast<int>(m_random.nextInt(5)) / 4);
    else
        set(q, 0); // burnt: no drop
}

void BlockUpdates::tickFire(const BlockPos& p, BlockStateId s) {
    schedule(p, B::Fire, 30 + static_cast<int>(m_random.nextInt(10)), 0); // 1.5-2 s (wiki)
    if (!nearPlayer(p)) return; // frozen far from players (still rescheduled)
    if (!fireSurvives(p)) {
        set(p, 0);
        return;
    }
    const BlockId below = blockOf(at(rel(p, Direction::Down)));
    int a = R().get(s, age);
    // Rain on it or beside it puts it out: 20% + 3% per age (wiki: Fire › Rain).
    if (!infiniburn(m_world, below) && rainingNear(p) && m_random.nextFloat() < 0.2f + 0.03f * float(a)) {
        set(p, 0);
        return;
    }
    if (a < 15 && m_random.nextInt(3) == 0) { // ages 1 in 3 ticks (wiki)
        m_world.setBlock(p, fireState(++a));  // the model ignores age: no re-mesh, no updates
        if (Chunk* c = chunkAt(p)) c->markDirty();
    }
    if (!infiniburn(m_world, below)) {
        // Nothing flammable around: a fire older than 3 (or not on a solid block) dies.
        if (!nextToFlammable(p)) {
            if (!R().collides(at(rel(p, Direction::Down))) || a > 3) set(p, 0);
            return;
        }
        // An old fire (age 15) over a non-flammable block: 1 in 4 to go out (wiki).
        if (a == 15 && igniteOdds(below) == 0 && m_random.nextInt(4) == 0) {
            set(p, 0);
            return;
        }
    }
    burnNeighbour(rel(p, Direction::East), 300, a);
    burnNeighbour(rel(p, Direction::West), 300, a);
    burnNeighbour(rel(p, Direction::Down), 250, a);
    burnNeighbour(rel(p, Direction::Up), 250, a);
    burnNeighbour(rel(p, Direction::North), 300, a);
    burnNeighbour(rel(p, Direction::South), 300, a);
    // (Spreading happens even if burning its support just put this fire out.)
    // Spread: every air block in 3x3, 1 below to 4 above, next to something
    // flammable. Degree = (best ignite odds + 40 + 7 x difficulty) / (age + 30);
    // chance degree / base, base 100 up to 1 above, +100 per block higher (wiki:
    // Fire › Spread).
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 4; ++dy) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                const BlockPos q{p.x + dx, p.y + dy, p.z + dz};
                if (!m_world.isInHeight(q.y) || at(q) != 0 || !chunkAt(q)) continue;
                int best = 0;
                for (int d = 0; d < kDirectionCount; ++d)
                    best =
                        std::max(best, igniteOdds(blockOf(at(rel(q, static_cast<Direction>(d))))));
                if (best == 0) continue;
                const int base = dy > 1 ? 100 + (dy - 1) * 100 : 100;
                const int degree = (best + 40 + 7 * kDifficulty) / (a + 30);
                if (degree > 0 && static_cast<int>(m_random.nextInt(uint32_t(base))) < degree)
                    placeFire(q, a + static_cast<int>(m_random.nextInt(5)) / 4);
            }
}

void BlockUpdates::lavaIgnites(const BlockPos& p) {
    if (!nearPlayer(p)) return;
    const int steps = static_cast<int>(m_random.nextInt(3));
    if (steps > 0) {
        // Rise 1 block per step, drifting up to 1 sideways; stop at anything solid.
        BlockPos q = p;
        for (int i = 0; i < steps; ++i) {
            q = {q.x + static_cast<int>(m_random.nextInt(3)) - 1, q.y + 1,
                 q.z + static_cast<int>(m_random.nextInt(3)) - 1};
            if (!m_world.isInHeight(q.y) || !chunkAt(q)) return;
            const BlockStateId s = at(q);
            if (s == 0) {
                bool catches = false;
                for (int d = 0; d < kDirectionCount && !catches; ++d)
                    catches = ignitedByLava(blockOf(at(rel(q, static_cast<Direction>(d)))));
                if (catches) {
                    placeFire(q, 0);
                    return;
                }
            } else if (R().collides(s)) {
                return;
            }
        }
        return;
    }
    // Or the top of a flammable block beside it.
    for (int i = 0; i < 3; ++i) {
        const BlockPos q{p.x + static_cast<int>(m_random.nextInt(3)) - 1, p.y,
                         p.z + static_cast<int>(m_random.nextInt(3)) - 1};
        if (!m_world.isInHeight(q.y + 1) || !chunkAt(q)) return;
        const BlockPos up{q.x, q.y + 1, q.z};
        if (at(up) == 0 && ignitedByLava(blockOf(at(q)))) placeFire(up, 0);
    }
}

} // namespace mc::world
