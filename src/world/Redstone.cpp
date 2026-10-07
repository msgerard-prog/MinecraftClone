#include "world/Redstone.h"

#include "core/Log.h"
#include "world/Blocks.h"
#include "world/Rotation.h"

#include <algorithm>
#include <cmath>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }

Direction opposite(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }
BlockPos rel(const BlockPos& p, Direction d, int n = 1) {
    const glm::ivec3 v = normal(d) * n;
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}
bool horizontal(Direction d) { return d != Direction::Up && d != Direction::Down; }

// Vanilla's neighbour update order (Level.updateNeighborsAt).
constexpr Direction kUpdateOrder[6] = {Direction::West, Direction::East, Direction::Down,
                                       Direction::Up,   Direction::North, Direction::South};
constexpr Direction kHorizontal[4] = {Direction::North, Direction::East, Direction::South, Direction::West};

// "facing" (horizontal) values are north, south, west, east: Direction 2..5.
Direction hFacing(BlockStateId s) { return static_cast<Direction>(R().get(s, facing) + 2); }
BlockStateId withHFacing(BlockStateId s, Direction d) { return R().set(s, facing, static_cast<int>(d) - 2); }
// "facing" (6 directions) values are in Direction order.
Direction facing6Of(BlockStateId s) { return static_cast<Direction>(R().get(s, facing6)); }
bool flag(BlockStateId s, const Property& p) { return R().get(s, p) == 0; } // [true, false]
BlockStateId withFlag(BlockStateId s, const Property& p, bool v) { return R().set(s, p, v ? 0 : 1); }

// Wire sides (values up, side, none).
enum WireSide { kUp = 0, kSide = 1, kNone = 2 };
const Property& wireProp(Direction d) {
    switch (d) {
    case Direction::North: return north;
    case Direction::East: return east;
    case Direction::South: return south;
    default: return west;
    }
}
int wireSide(BlockStateId s, Direction d) { return R().get(s, wireProp(d)); }

bool isPiston(BlockId b) { return b == B::Piston || b == B::StickyPiston; }
bool isButton(BlockId b) { return b == B::StoneButton || b == B::OakButton; }
bool isTorch(BlockId b) { return b == B::RedstoneTorch || b == B::RedstoneWallTorch; }
// Blocks that give power themselves (vanilla isSignalSource).
bool signalSource(BlockId b) {
    return b == B::RedstoneWire || isTorch(b) || b == B::Repeater || b == B::Lever || isButton(b) ||
           b == B::RedstoneBlock;
}

// Blocks that dust, torches, repeaters, levers and buttons can stand on or hang from:
// full-collision blocks except leaves (wiki: Opacity/Placement).
bool supports(BlockStateId s) {
    return R().collides(s) && !R().block(blockOf(s)).id.ends_with("_leaves");
}

// Lever/button: the direction of the block it hangs on.
Direction attachDir(BlockStateId s) {
    const int f = R().get(s, face);
    if (f == 0) return Direction::Down; // floor
    if (f == 2) return Direction::Up;   // ceiling
    return opposite(hFacing(s));        // wall: facing points away from it
}

// What a piston does to the block in front of it (wiki: Piston › Movable blocks).
enum class Push { Air, Destroy, Move, Block };
Push pushKind(BlockStateId s) {
    if (s == 0) return Push::Air;
    const BlockId b = blockOf(s);
    switch (b) {
    case B::RedstoneWire:
    case B::RedstoneTorch:
    case B::RedstoneWallTorch:
    case B::Repeater:
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
    case B::Torch:
    case B::ShortGrass:
    case B::Fern:
    case B::Dandelion:
    case B::Poppy:
    case B::Cornflower:
    case B::AzureBluet:
    case B::OxeyeDaisy:
    case B::DeadBush:
    case B::Snow:
    case B::Water:
    case B::Lava:
    case B::NetherPortal:
    case B::OakLeaves: // leaves break (wiki: Leaves › Piston interactivity)
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves: return Push::Destroy;
    case B::Obsidian:  // (wiki: Piston/Table)
    case B::Furnace:   // block entities don't move
    case B::PistonHead:
        return Push::Block;
    case B::Piston:
    case B::StickyPiston: return flag(s, extended) ? Push::Block : Push::Move;
    default: return R().block(b).settings.hardness < 0.0f ? Push::Block : Push::Move;
    }
}

} // namespace

Redstone::Redstone(World& world) : m_world(world) {
    m_world.setListener(this);
    m_due.reserve(1024);
    m_events.reserve(64);
    m_changed.reserve(4096);
    m_remesh.reserve(4096);
    m_drops.reserve(16);
    m_push.reserve(13);
    m_pushStates.reserve(13);
    m_toggles.reserve(64);
}

Redstone::~Redstone() { m_world.setListener(nullptr); }

bool Redstone::conductor(BlockStateId s) {
    // Opaque full blocks conduct, except these (wiki: Redstone circuits › Conductivity).
    const BlockId b = blockOf(s);
    return R().opaqueCube(s) && b != B::RedstoneBlock && !isPiston(b) && b != B::Glowstone;
}

// --- Power ------------------------------------------------------------------------

int Redstone::weak(BlockStateId s, Direction toward) const {
    switch (blockOf(s)) {
    case B::RedstoneWire: {
        if (m_wiresMuted || toward == Direction::Up) return 0;
        const int p = R().get(s, power);
        if (toward == Direction::Down) return p;
        return wireSide(s, toward) != kNone ? p : 0;
    }
    case B::RedstoneTorch: return flag(s, lit) && toward != Direction::Down ? 15 : 0;
    case B::RedstoneWallTorch: return flag(s, lit) && toward != opposite(hFacing(s)) ? 15 : 0;
    case B::Repeater: return flag(s, powered) && toward == opposite(hFacing(s)) ? 15 : 0;
    case B::Lever:
    case B::StoneButton:
    case B::OakButton: return flag(s, powered) ? 15 : 0;
    case B::RedstoneBlock: return 15;
    default: return 0;
    }
}

int Redstone::strong(BlockStateId s, Direction toward) const {
    switch (blockOf(s)) {
    case B::RedstoneWire:
    case B::Repeater: return weak(s, toward);
    case B::RedstoneTorch:
    case B::RedstoneWallTorch: return flag(s, lit) && toward == Direction::Up ? 15 : 0;
    case B::Lever:
    case B::StoneButton:
    case B::OakButton: return flag(s, powered) && toward == attachDir(s) ? 15 : 0;
    default: return 0;
    }
}

int Redstone::strongInto(const BlockPos& p) const {
    int best = 0;
    for (int d = 0; d < kDirectionCount && best < 15; ++d) {
        const Direction dir = static_cast<Direction>(d);
        best = std::max(best, strong(at(rel(p, dir)), opposite(dir)));
    }
    return best;
}

int Redstone::signalFrom(const BlockPos& p, Direction toward) const {
    const BlockStateId s = at(p);
    return conductor(s) ? strongInto(p) : weak(s, toward);
}

int Redstone::bestNeighbourSignal(const BlockPos& p) const {
    int best = 0;
    for (int d = 0; d < kDirectionCount && best < 15; ++d) {
        const Direction dir = static_cast<Direction>(d);
        best = std::max(best, signalFrom(rel(p, dir), opposite(dir)));
    }
    return best;
}

// --- Updates ----------------------------------------------------------------------

void Redstone::set(const BlockPos& p, BlockStateId s) {
    const BlockStateId old = at(p);
    if (old == s) return;
    m_world.setBlock(p, s);
    record(p, old, s);
    afterChange(p, old, s);
}

void Redstone::setDiode(const BlockPos& p, BlockStateId s) {
    // A repeater turning on/off updates only the block in front and its neighbours
    // (wiki: Block update - exceptions).
    setRaw(p, s);
    reach(p, s);
}

void Redstone::setRaw(const BlockPos& p, BlockStateId s) {
    const BlockStateId old = at(p);
    if (old == s) return;
    m_world.setBlock(p, s);
    record(p, old, s);
}

void Redstone::record(const BlockPos& p, BlockStateId old, BlockStateId now) {
    // Light only needs recomputing when emission or opacity changed (dust power,
    // repeater and lever states only change the model).
    const auto& r = R();
    const bool light = r.lightEmission(old) != r.lightEmission(now) || r.lightOpacity(old) != r.lightOpacity(now) ||
                       r.opaqueCube(old) != r.opaqueCube(now);
    auto& list = light ? m_changed : m_remesh;
    if (list.empty() || !(list.back() == p)) list.push_back(p); // a block changing again: once
}

void Redstone::onBlockChanged(const BlockPos& p, BlockStateId old, BlockStateId now) {
    afterChange(p, old, now);
    neighbourChanged(p); // the new block checks its surroundings (vanilla onPlace)
}

void Redstone::afterChange(const BlockPos& p, BlockStateId old, BlockStateId now) {
    const BlockId was = blockOf(old), is = blockOf(now);
    // A piston and its head go together (wiki: Piston › Behavior).
    if (isPiston(was) && flag(old, extended) && !(isPiston(is) && flag(now, extended))) {
        const BlockPos head = rel(p, facing6Of(old));
        const BlockStateId h = at(head);
        if (blockOf(h) == B::PistonHead && facing6Of(h) == facing6Of(old)) set(head, 0);
    }
    if (was == B::PistonHead && is != B::PistonHead) {
        const BlockPos base = rel(p, opposite(facing6Of(old)));
        const BlockStateId b = at(base);
        if (isPiston(blockOf(b)) && flag(b, extended) && facing6Of(b) == facing6Of(old)) {
            if (!m_creative)
                if (const ItemId item = itemRegistry().blockItem(blockOf(b))) m_drops.push_back({base, {item, 1}});
            set(base, 0);
        }
    }
    notifyNeighbours(p);
    reach(p, old);
    // Same block in a new state: its reach only moves if what it points at changed.
    const bool sameTarget = blockOf(now) == blockOf(old) &&
                            ((is != B::Lever && !isButton(is)) || attachDir(old) == attachDir(now)) &&
                            (is != B::Repeater || hFacing(old) == hFacing(now));
    if (!sameTarget) reach(p, now);
}

void Redstone::reach(const BlockPos& p, BlockStateId s) {
    // Components also update around the blocks they power (vanilla: dust and torches
    // all six neighbours' neighbours, levers/buttons the block they hang on, repeaters
    // the block in front; a block of redstone only its own neighbours).
    switch (blockOf(s)) {
    case B::RedstoneWire:
        for (const Direction d : kUpdateOrder)
            notifyNeighbours(rel(p, d));
        break;
    case B::RedstoneTorch:
    case B::RedstoneWallTorch: // outer order down, up, north, south, west, east (wiki: Block update)
        for (int d = 0; d < kDirectionCount; ++d)
            notifyNeighbours(rel(p, static_cast<Direction>(d)));
        break;
    case B::Lever:
    case B::StoneButton:
    case B::OakButton: notifyNeighbours(rel(p, attachDir(s))); break;
    case B::Repeater: {
        const BlockPos front = rel(p, opposite(hFacing(s)));
        neighbourChanged(front);
        notifyNeighbours(front);
        break;
    }
    default: break;
    }
}

void Redstone::notifyNeighbours(const BlockPos& p) {
    for (const Direction d : kUpdateOrder)
        neighbourChanged(rel(p, d));
}

void Redstone::pop(const BlockPos& p) {
    const BlockStateId s = at(p);
    if (const ItemId item = itemRegistry().blockItem(blockOf(s))) m_drops.push_back({p, {item, 1}});
    set(p, 0);
}

bool Redstone::survives(const BlockPos& p, BlockStateId s) const {
    switch (blockOf(s)) {
    case B::RedstoneWire:
    case B::RedstoneTorch:
    case B::Repeater: return supports(at(rel(p, Direction::Down)));
    case B::RedstoneWallTorch: return supports(at(rel(p, opposite(hFacing(s)))));
    case B::Lever:
    case B::StoneButton:
    case B::OakButton: return supports(at(rel(p, attachDir(s))));
    default: return true;
    }
}

void Redstone::neighbourChanged(const BlockPos& p) {
    if (!isInBuildHeight(p.y)) return;
    if (m_depth > 2048) { // runaway update chain: stop (vanilla also caps its updates)
        static bool logged = false;
        if (!logged) MC_LOG_WARN("Block updates nested too deeply; some were skipped");
        logged = true;
        return;
    }
    ++m_depth;
    const BlockStateId s = at(p);
    switch (blockOf(s)) {
    case B::RedstoneWire: updateWire(p); break;
    case B::RedstoneTorch:
    case B::RedstoneWallTorch:
        if (!survives(p, s)) pop(p);
        else if (flag(s, lit) == torchInput(p, s) && !hasTick(p, blockOf(s))) schedule(p, blockOf(s), 2, 0);
        break;
    case B::Repeater: {
        if (!survives(p, s)) {
            pop(p);
            break;
        }
        const bool lockedNow = repeaterLocked(p, s);
        if (lockedNow != flag(s, locked)) setRaw(p, withFlag(s, locked, lockedNow)); // no updates (vanilla)
        if (lockedNow) break;
        const BlockStateId cur = at(p);
        const bool should = repeaterInput(p, cur) > 0;
        if (flag(cur, powered) != should && !hasTick(p, B::Repeater)) {
            // Priorities (wiki: Tick › Scheduled tick): -3 when the repeater faces into
            // the side or back of another repeater, -2 when turning off, else -1.
            const BlockStateId front = at(rel(p, opposite(hFacing(cur))));
            const int priority = blockOf(front) == B::Repeater && hFacing(front) != opposite(hFacing(cur)) ? -3
                                 : flag(cur, powered)                                             ? -2
                                                                                                   : -1;
            schedule(p, B::Repeater, (R().get(cur, delay) + 1) * 2, priority);
        }
        break;
    }
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
        if (!survives(p, s)) pop(p);
        break;
    case B::RedstoneLamp: {
        // Lamps light at once and go out 4 ticks later (wiki: Redstone Lamp).
        const bool on = bestNeighbourSignal(p) > 0;
        if (flag(s, lit) && !on) {
            if (!hasTick(p, B::RedstoneLamp)) schedule(p, B::RedstoneLamp, 4, 0);
        } else if (!flag(s, lit) && on) {
            set(p, withFlag(s, lit, true));
        }
        break;
    }
    case B::Piston:
    case B::StickyPiston: {
        const bool should = pistonPowered(p, facing6Of(s));
        if (should != flag(s, extended) &&
            std::none_of(m_events.begin(), m_events.end(), [&](const Event& e) { return e.pos == p && e.extend == should; }))
            m_events.push_back({p, should, !m_inTick});
        break;
    }
    case B::NetherPortal: {
        // A portal block needs portal or obsidian above, below and along its axis
        // (wiki: Nether portal - breaking the frame breaks the portal).
        const Direction side = R().get(s, haxis) == 0 ? Direction::East : Direction::South;
        for (const Direction d : {Direction::Up, Direction::Down, side, opposite(side)}) {
            const BlockId n = blockOf(at(rel(p, d)));
            if (n != B::NetherPortal && n != B::Obsidian) {
                set(p, 0);
                break;
            }
        }
        break;
    }
    case B::PistonHead: {
        const BlockStateId b = at(rel(p, opposite(facing6Of(s))));
        if (!(isPiston(blockOf(b)) && flag(b, extended) && facing6Of(b) == facing6Of(s))) set(p, 0);
        break;
    }
    default: break;
    }
    --m_depth;
}

// --- Scheduled ticks --------------------------------------------------------------

void Redstone::schedule(const BlockPos& p, BlockId block, int ticks, int priority) {
    Chunk* c = m_world.chunk(p.chunk());
    if (!c || hasTick(p, block)) return; // one pending tick per block (vanilla)
    makeAbsolute(*c);
    c->markDirty(); // pending ticks are saved with the chunk
    c->blockTicks().push_back({static_cast<int8_t>(blockToLocal(p.x)), static_cast<int8_t>(blockToLocal(p.z)),
                               static_cast<int16_t>(p.y), static_cast<int8_t>(priority), block, m_now + ticks,
                               m_order++});
    m_world.markTicking(c->pos());
}

void Redstone::makeAbsolute(Chunk& c) {
    // Loaded from disk: delays count from now, in their saved order.
    if (!c.ticksRelative) return;
    auto& ticks = c.blockTicks();
    std::sort(ticks.begin(), ticks.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
    for (auto& t : ticks) {
        t.time += m_now - 1; // it was loaded before this tick began
        t.order = m_order++;
    }
    c.ticksRelative = false;
}

bool Redstone::hasTick(const BlockPos& p, BlockId block) const {
    const Chunk* c = m_world.chunk(p.chunk());
    if (!c) return false;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    for (const Chunk::BlockTick& t : c->blockTicks())
        if (t.x == x && t.z == z && t.y == p.y && t.block == block) return true;
    return false;
}

void Redstone::tick() {
    m_inTick = true;
    m_due.clear();
    m_world.forEachTickingChunk([&](Chunk& c) {
        auto& ticks = c.blockTicks();
        if (ticks.empty()) return;
        makeAbsolute(c);
        for (const auto& t : ticks)
            if (t.time <= m_now) m_due.push_back({{c.pos().x * 16 + t.x, t.y, c.pos().z * 16 + t.z}, t});
        if (std::erase_if(ticks, [&](const Chunk::BlockTick& t) { return t.time <= m_now; })) c.markDirty();
    });
    std::sort(m_due.begin(), m_due.end(), [](const Due& a, const Due& b) {
        if (a.tick.time != b.tick.time) return a.tick.time < b.tick.time;
        if (a.tick.priority != b.tick.priority) return a.tick.priority < b.tick.priority;
        return a.tick.order < b.tick.order;
    });
    for (const Due& d : m_due) {
        const BlockStateId s = at(d.pos);
        if (blockOf(s) == d.tick.block) tickBlock(d.pos, s);
    }
    // Block events (pistons), including ones these cause (wiki: Tick › Block events).
    // Pistons powered by a player act in the next tick's block events (wiki: Piston ›
    // Start delay); they wait one tick here.
    size_t kept = 0;
    for (size_t i = 0; i < m_events.size() && i < 65536; ++i) {
        const Event e = m_events[i];
        if (e.nextTick) {
            m_events[kept++] = {e.pos, e.extend, false};
            continue;
        }
        const BlockStateId s = at(e.pos);
        if (!isPiston(blockOf(s))) continue;
        const bool should = pistonPowered(e.pos, facing6Of(s));
        if (e.extend && should && !flag(s, extended)) extend(e.pos);
        else if (!e.extend && !should && flag(s, extended)) retract(e.pos);
    }
    m_events.resize(kept);
    m_inTick = false;
    // Torch burnout memory: only the last 60 ticks count.
    std::erase_if(m_toggles, [&](const Toggle& t) { return m_now - t.time > 60; });
}

void Redstone::tickBlock(const BlockPos& p, BlockStateId s) {
    switch (blockOf(s)) {
    case B::RedstoneTorch:
    case B::RedstoneWallTorch: {
        const bool input = torchInput(p, s);
        if (flag(s, lit)) {
            if (input) {
                set(p, withFlag(s, lit, false));
                // Burnout (wiki: Redstone Torch): 8 changes within 60 ticks.
                if (toggledTooOften(p, true)) schedule(p, blockOf(s), 160, 0);
            }
        } else if (!input && !toggledTooOften(p, false)) {
            set(p, withFlag(s, lit, true));
        }
        break;
    }
    case B::Repeater: {
        if (repeaterLocked(p, s)) break;
        const bool should = repeaterInput(p, s) > 0;
        if (flag(s, powered) && !should) {
            setDiode(p, withFlag(s, powered, false));
        } else if (!flag(s, powered)) {
            setDiode(p, withFlag(s, powered, true));
            // A pulse shorter than the delay is extended to it (wiki: Redstone Repeater).
            if (!should) schedule(p, B::Repeater, (R().get(s, delay) + 1) * 2, -2);
        }
        break;
    }
    case B::RedstoneLamp:
        if (flag(s, lit) && bestNeighbourSignal(p) == 0) set(p, withFlag(s, lit, false));
        break;
    case B::StoneButton:
    case B::OakButton:
        if (flag(s, powered)) set(p, withFlag(s, powered, false));
        break;
    default: break;
    }
}

// --- Dust -------------------------------------------------------------------------

int Redstone::wirePower(const BlockPos& p) const {
    const BlockStateId s = at(p);
    return blockOf(s) == B::RedstoneWire ? R().get(s, power) : 0;
}

BlockStateId Redstone::wireShape(const BlockPos& p, BlockStateId s) const {
    // Which way the dust runs (wiki: Redstone Dust › Behavior): to dust beside it, one
    // block up (unless a conductor above cuts it) or down (unless the side block is a
    // conductor), and to components that accept a connection from that side.
    const bool aboveOpen = !conductor(at(rel(p, Direction::Up)));
    int sides[4];
    bool any = false;
    for (int i = 0; i < 4; ++i) {
        const Direction d = kHorizontal[i];
        const BlockPos n = rel(p, d);
        const BlockStateId ns = at(n);
        int side = kNone;
        if (aboveOpen && R().collides(ns) && blockOf(at(rel(n, Direction::Up))) == B::RedstoneWire)
            side = R().opaqueCube(ns) ? kUp : kSide;
        if (side == kNone) {
            const BlockId nb = blockOf(ns);
            const bool connects = nb == B::RedstoneWire ||
                                  (nb == B::Repeater ? (hFacing(ns) == d || hFacing(ns) == opposite(d)) : signalSource(nb));
            if (connects || (!conductor(ns) && blockOf(at(rel(n, Direction::Down))) == B::RedstoneWire)) side = kSide;
        }
        sides[i] = side;
        any = any || side != kNone;
    }
    if (!any) {
        // Unconnected dust is a cross, or a dot if the player made it one (right-click).
        bool dot = true;
        for (const Direction d : kHorizontal)
            dot = dot && wireSide(s, d) == kNone;
        for (int i = 0; i < 4; ++i)
            sides[i] = dot ? kNone : kSide;
    } else {
        // Dust connected along one axis only runs straight through.
        const bool ns = sides[0] != kNone || sides[2] != kNone, ew = sides[1] != kNone || sides[3] != kNone;
        if (ns && !ew) {
            if (sides[0] == kNone) sides[0] = kSide;
            if (sides[2] == kNone) sides[2] = kSide;
        } else if (ew && !ns) {
            if (sides[1] == kNone) sides[1] = kSide;
            if (sides[3] == kNone) sides[3] = kSide;
        }
    }
    for (int i = 0; i < 4; ++i)
        s = R().set(s, wireProp(kHorizontal[i]), sides[i]);
    return s;
}

int Redstone::wireTarget(const BlockPos& p) const {
    // Power from components and strongly powered blocks (other dust muted), or from
    // neighbouring dust (including one block up/down) minus 1.
    m_wiresMuted = true;
    const int strongest = bestNeighbourSignal(p);
    m_wiresMuted = false;
    if (strongest >= 15) return 15;
    const bool aboveOpen = !conductor(at(rel(p, Direction::Up)));
    int fromWire = 0;
    for (const Direction d : kHorizontal) {
        const BlockPos n = rel(p, d);
        fromWire = std::max(fromWire, wirePower(n));
        if (conductor(at(n))) {
            if (aboveOpen) fromWire = std::max(fromWire, wirePower(rel(n, Direction::Up)));
        } else {
            fromWire = std::max(fromWire, wirePower(rel(n, Direction::Down)));
        }
    }
    return std::max(strongest, fromWire - 1);
}

void Redstone::updateWire(const BlockPos& p) {
    const BlockStateId s = at(p);
    if (!survives(p, s)) {
        pop(p);
        return;
    }
    const BlockStateId next = R().set(wireShape(p, s), power, wireTarget(p));
    if (next == s) return;
    // Only a power change updates other components (wiki: Redstone Dust).
    if (R().get(next, power) == R().get(s, power)) setRaw(p, next);
    else set(p, next);
}

// --- Torches, repeaters, pistons ----------------------------------------------------

bool Redstone::torchInput(const BlockPos& p, BlockStateId s) const {
    // Off while the block it's attached to is powered (wiki: Redstone Torch).
    const Direction toTorch = blockOf(s) == B::RedstoneTorch ? Direction::Up : hFacing(s);
    return signalFrom(rel(p, opposite(toTorch)), toTorch) > 0;
}

bool Redstone::toggledTooOften(const BlockPos& p, bool add) {
    if (add) m_toggles.push_back({p, m_now});
    int n = 0;
    for (const Toggle& t : m_toggles)
        if (t.pos == p && m_now - t.time <= 60) ++n;
    return n >= 8;
}

int Redstone::repeaterInput(const BlockPos& p, BlockStateId s) const {
    const Direction back = hFacing(s); // facing points to the input side
    const BlockPos in = rel(p, back);
    return std::max(signalFrom(in, opposite(back)), wirePower(in));
}

bool Redstone::repeaterLocked(const BlockPos& p, BlockStateId s) const {
    // Locked by a powered repeater pointing into either side (wiki: Redstone Repeater).
    const Direction f = hFacing(s);
    for (const Direction side : kHorizontal) {
        if (side == f || side == opposite(f)) continue;
        const BlockStateId n = at(rel(p, side));
        if (blockOf(n) == B::Repeater && flag(n, powered) && hFacing(n) == side) return true;
    }
    return false;
}

bool Redstone::pistonPowered(const BlockPos& p, Direction f) const {
    // Any side but the front, or the block above it (quasi-connectivity, wiki: Piston).
    for (int d = 0; d < kDirectionCount; ++d) {
        const Direction dir = static_cast<Direction>(d);
        if (dir != f && signalFrom(rel(p, dir), opposite(dir)) > 0) return true;
    }
    const BlockPos above = rel(p, Direction::Up);
    for (int d = 0; d < kDirectionCount; ++d) {
        const Direction dir = static_cast<Direction>(d);
        if (dir != Direction::Down && signalFrom(rel(above, dir), opposite(dir)) > 0) return true;
    }
    return false;
}

bool Redstone::pushList(const BlockPos& base, Direction f, std::optional<BlockPos>& destroy) {
    m_push.clear();
    destroy.reset();
    BlockPos p = rel(base, f);
    for (;;) {
        if (!isInBuildHeight(p.y) || !m_world.chunk(p.chunk())) return false;
        const Push k = pushKind(at(p));
        if (k == Push::Air) return true;
        if (k == Push::Destroy) {
            destroy = p;
            return true;
        }
        if (k == Push::Block || m_push.size() == 12) return false; // push limit 12
        m_push.push_back(p);
        p = rel(p, f);
    }
}

void Redstone::extend(const BlockPos& p) {
    const BlockStateId s = at(p);
    const Direction f = facing6Of(s);
    std::optional<BlockPos> destroy;
    if (!pushList(p, f, destroy)) return;
    BlockStateId destroyed = 0;
    if (destroy) {
        destroyed = at(*destroy);
        const BlockId b = blockOf(destroyed);
        if (b != B::Water && b != B::Lava)
            if (const ItemId item = itemRegistry().blockItem(b)) m_drops.push_back({*destroy, {item, 1}});
        setRaw(*destroy, 0);
    }
    // Moves happen at once (vanilla animates them over 2 ticks: known deviation).
    m_pushStates.clear();
    for (const BlockPos& q : m_push)
        m_pushStates.push_back(at(q));
    for (const BlockPos& q : m_push)
        setRaw(q, 0);
    for (size_t i = 0; i < m_push.size(); ++i)
        setRaw(rel(m_push[i], f), m_pushStates[i]);
    setRaw(p, withFlag(s, extended, true));
    BlockStateId head = R().set(R().defaultState(B::PistonHead), facing6, static_cast<int>(f));
    head = R().set(head, pistonType, blockOf(s) == B::StickyPiston ? 1 : 0);
    setRaw(rel(p, f), head);
    // Then everything touched gets its updates.
    const size_t n = m_push.size();
    neighbourChanged(p);
    notifyNeighbours(p);
    notifyNeighbours(rel(p, f));
    if (destroy) {
        notifyNeighbours(*destroy);
        reach(*destroy, destroyed); // what the broken component powered loses it
    }
    for (size_t i = 0; i < n && i < m_push.size(); ++i) {
        const BlockPos to = rel(m_push[i], f);
        neighbourChanged(to);
        notifyNeighbours(to);
        notifyNeighbours(m_push[i]);
    }
}

void Redstone::retract(const BlockPos& p) {
    const BlockStateId s = at(p);
    const Direction f = facing6Of(s);
    const BlockPos front = rel(p, f);
    const BlockStateId h = at(front);
    if (blockOf(h) == B::PistonHead && facing6Of(h) == f) setRaw(front, 0);
    setRaw(p, withFlag(s, extended, false));
    // Sticky pistons pull the block in front of the head back with them.
    std::optional<BlockPos> pulled;
    if (blockOf(s) == B::StickyPiston) {
        const BlockPos far = rel(p, f, 2);
        if (isInBuildHeight(far.y) && m_world.chunk(far.chunk()) && at(front) == 0 && pushKind(at(far)) == Push::Move) {
            setRaw(front, at(far));
            setRaw(far, 0);
            pulled = far;
        }
    }
    neighbourChanged(p);
    notifyNeighbours(p);
    neighbourChanged(front);
    notifyNeighbours(front);
    if (pulled) notifyNeighbours(*pulled);
}

// --- Players ----------------------------------------------------------------------

bool Redstone::usable(BlockStateId s) {
    const BlockId b = blockOf(s);
    return b == B::Lever || isButton(b) || b == B::Repeater || b == B::RedstoneWire;
}

bool Redstone::use(const BlockPos& p) {
    const BlockStateId s = at(p);
    switch (blockOf(s)) {
    case B::Lever: set(p, withFlag(s, powered, !flag(s, powered))); return true;
    case B::StoneButton:
    case B::OakButton:
        if (!flag(s, powered)) {
            set(p, withFlag(s, powered, true));
            // Pressed for 1 s (stone) or 1.5 s (wood) (wiki: Button).
            schedule(p, blockOf(s), blockOf(s) == B::StoneButton ? 20 : 30, 0);
        }
        return true;
    case B::Repeater: set(p, R().set(s, delay, (R().get(s, delay) + 1) % 4)); return true;
    case B::RedstoneWire: {
        // Unconnected dust toggles between a cross and a dot (wiki: Redstone Dust).
        BlockStateId dotState = s;
        for (const Direction d : kHorizontal)
            dotState = R().set(dotState, wireProp(d), kNone);
        if (wireShape(p, dotState) != dotState) return false; // connected: a normal use
        BlockStateId next = dotState;
        if (s == dotState)
            for (const Direction d : kHorizontal)
                next = R().set(next, wireProp(d), kSide);
        set(p, next);
        return true;
    }
    default: return false;
    }
}

std::optional<BlockStateId> Redstone::placement(const World& world, BlockStateId state, const BlockPos& at,
                                                Direction faceDir, float yaw, float pitch) {
    const auto& r = R();
    auto solid = [&](Direction d) { return supports(world.getBlock(rel(at, d))); };
    // The player's horizontal look direction (vanilla yaw: 0 south, 90 west).
    const float y = std::fmod(std::fmod(yaw, 360.0f) + 360.0f, 360.0f);
    static constexpr Direction kLook[4] = {Direction::South, Direction::West, Direction::North, Direction::East};
    const Direction look = kLook[static_cast<int>(std::floor((y + 45.0f) / 90.0f)) % 4];
    switch (blockOf(state)) {
    case B::RedstoneWire: {
        if (!solid(Direction::Down)) return std::nullopt;
        BlockStateId s = state; // placed as a cross; its neighbours shape it
        for (const Direction d : kHorizontal)
            s = r.set(s, wireProp(d), kSide);
        return s;
    }
    case B::RedstoneTorch:
        // On a wall when clicked on its side, else standing (wiki: Redstone Torch).
        if (horizontal(faceDir) && solid(opposite(faceDir)))
            return withHFacing(r.defaultState(B::RedstoneWallTorch), faceDir);
        if (solid(Direction::Down)) return state;
        return std::nullopt;
    case B::Repeater:
        if (!solid(Direction::Down)) return std::nullopt;
        return withHFacing(state, opposite(look)); // the output points away from the player
    case B::Lever:
    case B::StoneButton:
    case B::OakButton: {
        if (!solid(opposite(faceDir))) return std::nullopt;
        if (faceDir == Direction::Up) return withHFacing(r.set(state, face, 0), look);
        if (faceDir == Direction::Down) return withHFacing(r.set(state, face, 2), look);
        return withHFacing(r.set(state, face, 1), faceDir);
    }
    case B::Piston:
    case B::StickyPiston: {
        // Facing the player along the axis they look along most (vanilla).
        const glm::vec3 v = lookVector(yaw, pitch);
        const glm::vec3 a = glm::abs(v);
        Direction d = a.y >= a.x && a.y >= a.z ? (v.y > 0 ? Direction::Up : Direction::Down)
                      : a.x >= a.z            ? (v.x > 0 ? Direction::East : Direction::West)
                                              : (v.z > 0 ? Direction::South : Direction::North);
        return r.set(state, facing6, static_cast<int>(opposite(d)));
    }
    default: return state;
    }
}

} // namespace mc::world
