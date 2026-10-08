#include "world/BlockUpdates.h"

#include "world/Rails.h"

#include "core/Log.h"
#include "world/Blocks.h"
#include "world/Rotation.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
// Family members act like their prototype (M23.3: every wooden door like the oak door).
BlockId blockOf(BlockStateId s) { return R().likeOf(R().blockOf(s)); }

Direction opposite(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }
BlockPos rel(const BlockPos& p, Direction d, int n = 1) {
    const glm::ivec3 v = normal(d) * n;
    return {p.x + v.x, p.y + v.y, p.z + v.z};
}
bool horizontal(Direction d) { return d != Direction::Up && d != Direction::Down; }

// Vanilla's neighbour update order (Level.updateNeighborsAt).
constexpr Direction kUpdateOrder[6] = {Direction::West, Direction::East,  Direction::Down,
                                       Direction::Up,   Direction::North, Direction::South};
constexpr Direction kHorizontal[4] = {Direction::North, Direction::East, Direction::South,
                                      Direction::West};

// "facing" (horizontal) values are north, south, west, east: Direction 2..5.
Direction hFacing(BlockStateId s) { return static_cast<Direction>(R().get(s, facing) + 2); }
BlockStateId withHFacing(BlockStateId s, Direction d) {
    return R().set(s, facing, static_cast<int>(d) - 2);
}
// "facing" (6 directions) values are in Direction order.
Direction facing6Of(BlockStateId s) { return static_cast<Direction>(R().get(s, facing6)); }
bool flag(BlockStateId s, const Property& p) { return R().get(s, p) == 0; } // [true, false]
BlockStateId withFlag(BlockStateId s, const Property& p, bool v) {
    return R().set(s, p, v ? 0 : 1);
}

// Wire sides (values up, side, none).
enum WireSide { kUp = 0, kSide = 1, kNone = 2 };
const Property& wireProp(Direction d) {
    switch (d) {
    case Direction::North:
        return north;
    case Direction::East:
        return east;
    case Direction::South:
        return south;
    default:
        return west;
    }
}
int wireSide(BlockStateId s, Direction d) { return R().get(s, wireProp(d)); }

bool isPiston(BlockId b) { return b == B::Piston || b == B::StickyPiston; }
bool isButton(BlockId b) { return b == B::StoneButton || b == B::OakButton; }
bool isTorch(BlockId b) { return b == B::RedstoneTorch || b == B::RedstoneWallTorch; }
// Blocks that give power themselves (vanilla isSignalSource).
bool signalSource(BlockId b) {
    return b == B::RedstoneWire || isTorch(b) || b == B::Repeater || b == B::Lever || isButton(b) ||
           b == B::RedstoneBlock || b == B::Comparator;
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
    if (b == B::MovingPiston) return Push::Block; // (already in flight)
    switch (b) {
    case B::RedstoneWire:
    case B::RedstoneTorch:
    case B::RedstoneWallTorch:
    case B::Repeater:
    case B::Comparator:
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
    case B::OakDoor: // (wiki: Piston/Table - doors and pressure plates break)
    case B::IronDoor:
    case B::OakPressurePlate:
    case B::StonePressurePlate:
    case B::LightWeightedPressurePlate:
    case B::HeavyWeightedPressurePlate:
    case B::Torch:
    case B::ShortGrass:
    case B::Fern:
    case B::Dandelion:
    case B::Poppy:
    case B::Cornflower:
    case B::AzureBluet:
    case B::OxeyeDaisy:
    case B::DeadBush:
    case B::BrownMushroom:
    case B::RedMushroom:
    case B::Cactus:
    case B::CrimsonFungus:
    case B::WarpedFungus:
    case B::CrimsonRoots:
    case B::WarpedRoots:
    case B::NetherSprouts:
    case B::WeepingVines:
    case B::WeepingVinesPlant:
    case B::TwistingVines:
    case B::TwistingVinesPlant:
    case B::NetherWart:
    case B::Snow:
    case B::Water:
    case B::Lava:
    case B::NetherPortal:
    case B::OakLeaves: // leaves break (wiki: Leaves › Piston interactivity)
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
    case B::JungleLeaves:
    case B::DarkOakLeaves:
    case B::CherryLeaves:
    case B::MangroveLeaves:
    case B::PaleOakLeaves:
    case B::OakSapling:
    case B::BirchSapling:
    case B::SpruceSapling:
    case B::AcaciaSapling:
    case B::JungleSapling:
    case B::DarkOakSapling:
    case B::CherrySapling:
    case B::PaleOakSapling:
    case B::MangrovePropagule:
    case B::Fire:
    case B::RedBed:
        return Push::Destroy; // (wiki: Piston - beds break)
    case B::Obsidian:         // (wiki: Piston/Table)
    case B::Spawner:          // (wiki: Monster Spawner - immovable)
    case B::Furnace:          // block entities don't move
    case B::Chest:
    case B::Hopper:
    case B::Dispenser:
    case B::Dropper:
    case B::BrewingStand:
    case B::EnchantingTable:
    case B::PistonHead:
        return Push::Block;
    case B::Piston:
    case B::StickyPiston:
        return flag(s, extended) ? Push::Block : Push::Move;
    default:
        return R().block(b).settings.hardness < 0.0f ? Push::Block : Push::Move;
    }
}

} // namespace

BlockUpdates::BlockUpdates(World& world) : m_world(world) {
    m_world.setListener(this);
    m_due.reserve(16384); // a /fill of fluid sources makes thousands due at once
    m_events.reserve(64);
    m_lightning.reserve(64);
    m_changed.reserve(4096);
    m_settling.reserve(4096);
    m_remesh.reserve(4096);
    m_drops.reserve(256); // leaf decay bursts
    m_falling.reserve(256);
    m_push.reserve(13);
    m_pushStates.reserve(13);
    m_toggles.reserve(64);
    m_plates.reserve(1024); // (pressure plates being pressed)
    m_tntPrimed.reserve(256);
    m_dispensed.reserve(256);
    m_moving.reserve(256);
    m_pushDestroy.reserve(16);
}

BlockUpdates::~BlockUpdates() { m_world.setListener(nullptr); }

bool BlockUpdates::conductor(BlockStateId s) {
    // Opaque full blocks conduct, except these (wiki: Redstone circuits › Conductivity).
    const BlockId b = blockOf(s);
    return R().opaqueCube(s) && b != B::RedstoneBlock && !isPiston(b) && b != B::Glowstone && b != B::Observer;
}

// --- Power ------------------------------------------------------------------------

int BlockUpdates::weak(BlockStateId s, Direction toward) const {
    switch (blockOf(s)) {
    case B::RedstoneWire: {
        if (m_wiresMuted || toward == Direction::Up) return 0;
        const int p = R().get(s, power);
        if (toward == Direction::Down) return p;
        return wireSide(s, toward) != kNone ? p : 0;
    }
    case B::RedstoneTorch:
        return flag(s, lit) && toward != Direction::Down ? 15 : 0;
    case B::RedstoneWallTorch:
        return flag(s, lit) && toward != opposite(hFacing(s)) ? 15 : 0;
    case B::Repeater:
        return flag(s, powered) && toward == opposite(hFacing(s)) ? 15 : 0;
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
        return flag(s, powered) ? 15 : 0;
    case B::RedstoneBlock:
        return 15;
    case B::OakPressurePlate:
    case B::StonePressurePlate:
    case B::DetectorRail:
        return flag(s, powered) ? 15 : 0;
    case B::LightWeightedPressurePlate:
    case B::HeavyWeightedPressurePlate:
        return R().get(s, power);
    case B::Observer: // out of its back (wiki: Observer)
        return flag(s, powered) && toward == opposite(facing6Of(s)) ? 15 : 0;
    default:
        return 0;
    }
}

int BlockUpdates::strong(BlockStateId s, Direction toward) const {
    switch (blockOf(s)) {
    case B::RedstoneWire:
    case B::Repeater:
    case B::Observer:
        return weak(s, toward);
    case B::RedstoneTorch:
    case B::RedstoneWallTorch:
        return flag(s, lit) && toward == Direction::Up ? 15 : 0;
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
        return flag(s, powered) && toward == attachDir(s) ? 15 : 0;
    case B::OakPressurePlate: // plates power the block under them strongly (wiki)
    case B::StonePressurePlate:
    case B::DetectorRail:
    case B::LightWeightedPressurePlate:
    case B::HeavyWeightedPressurePlate:
        return toward == Direction::Down ? weak(s, toward) : 0;
    default:
        return 0;
    }
}

int BlockUpdates::weakAt(const BlockPos& q, Direction toward) const {
    const BlockStateId s = at(q);
    if (blockOf(s) != B::Comparator) return weak(s, toward);
    // Out of its front only, at the strength it keeps (wiki: Redstone Comparator).
    if (toward != opposite(hFacing(s))) return 0;
    const Chunk* c = chunkAt(q);
    const ComparatorData* d = c ? const_cast<Chunk*>(c)->comparator(blockToLocal(q.x), q.y, blockToLocal(q.z)) : nullptr;
    return d ? d->output : 0;
}

int BlockUpdates::strongAt(const BlockPos& q, Direction toward) const {
    const BlockStateId s = at(q);
    return blockOf(s) == B::Comparator ? weakAt(q, toward) : strong(s, toward);
}

int BlockUpdates::containerSignal(const BlockPos& p) const {
    // Fullness 0..15: 0 when empty, else floor(1 + (sum of count / max stack) / slots x 14)
    // (wiki: Redstone Comparator › Measure block state).
    const BlockId b = blockOf(at(p));
    Chunk* c = chunkAt(p);
    if (!c) return -1;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    double fill = 0.0;
    int slots = 0;
    bool any = false;
    auto count = [&](const ItemStack& st) {
        ++slots;
        if (st.empty()) return;
        any = true;
        fill += double(st.count) / std::max(1, int(itemRegistry().item(st.item).maxStack));
    };
    if (b == B::Chest) {
        if (const ChestData* d = c->chest(x, p.y, z))
            for (const ItemStack& st : d->items)
                count(st);
        if (const auto partner = chestPartner(m_world, p)) // a double chest counts both halves
            if (Chunk* pc = chunkAt(*partner))
                if (const ChestData* d = pc->chest(blockToLocal(partner->x), partner->y, blockToLocal(partner->z)))
                    for (const ItemStack& st : d->items)
                        count(st);
    } else if (b == B::Furnace) {
        if (const FurnaceData* d = c->furnace(x, p.y, z)) {
            count(d->input);
            count(d->fuel);
            count(d->output);
        }
    } else if (b == B::Hopper) {
        if (const HopperData* d = c->hopper(x, p.y, z))
            for (const ItemStack& st : d->items)
                count(st);
    } else if (b == B::Dispenser || b == B::Dropper) {
        if (const DispenserData* d = c->dispenser(x, p.y, z))
            for (const ItemStack& st : d->items)
                count(st);
    } else if (b == B::BrewingStand) {
        if (const BrewingData* d = c->brewing(x, p.y, z)) {
            for (const ItemStack& st : d->bottles)
                count(st);
            count(d->ingredient);
            count(d->fuel);
        }
    } else {
        return -1;
    }
    if (!any || slots == 0) return 0;
    return std::min(15, static_cast<int>(std::floor(1.0 + fill / slots * 14.0)));
}

int BlockUpdates::comparatorTarget(const BlockPos& p, BlockStateId s) const {
    // Rear: a container's fullness (also through one solid block), else its signal;
    // sides: only dust, repeaters, comparators and redstone blocks count.
    const Direction back = hFacing(s);
    const BlockPos in = rel(p, back);
    int rear = std::max(signalFrom(in, opposite(back)), wirePower(in));
    if (const int c = containerSignal(in); c >= 0) rear = std::max(rear, c);
    else if (conductor(at(in)))
        if (const int c2 = containerSignal(rel(in, back)); c2 >= 0) rear = std::max(rear, c2);
    int side = 0;
    for (const Direction d : kHorizontal) {
        if (d == back || d == opposite(back)) continue;
        const BlockPos q = rel(p, d);
        const BlockId nb = blockOf(at(q));
        if (nb == B::RedstoneWire) side = std::max(side, wirePower(q));
        else if (nb == B::Repeater || nb == B::Comparator || nb == B::RedstoneBlock || nb == B::Observer)
            side = std::max(side, weakAt(q, opposite(d)));
    }
    return R().get(s, comparatorMode) == 0 ? (rear >= side ? rear : 0) : std::max(0, rear - side);
}

void BlockUpdates::comparatorChanged(const BlockPos& p, BlockStateId s) {
    // A change takes 1 redstone tick (2 game ticks) to show (wiki).
    const Chunk* c = chunkAt(p);
    const ComparatorData* d = c ? const_cast<Chunk*>(c)->comparator(blockToLocal(p.x), p.y, blockToLocal(p.z)) : nullptr;
    const int have = d ? d->output : 0;
    if (comparatorTarget(p, s) != have && !hasTick(p, B::Comparator)) schedule(p, B::Comparator, 2, -1);
}

void BlockUpdates::watchComparators() {
    // Containers don't send updates here; comparators reading one look each tick.
    m_world.forEachTickingChunk([&](Chunk& c) {
        for (const auto& e : std::as_const(c).comparators()) {
            const BlockPos p{c.pos().x * 16 + e.x, e.y, c.pos().z * 16 + e.z};
            const BlockStateId s = c.get(e.x, e.y, e.z);
            if (blockOf(s) != B::Comparator) continue;
            const Direction back = hFacing(s);
            const BlockPos in = rel(p, back);
            if (containerSignal(in) >= 0 || (conductor(at(in)) && containerSignal(rel(in, back)) >= 0))
                comparatorChanged(p, s);
        }
    });
}

int BlockUpdates::strongInto(const BlockPos& p) const {
    int best = 0;
    for (int d = 0; d < kDirectionCount && best < 15; ++d) {
        const Direction dir = static_cast<Direction>(d);
        best = std::max(best, strongAt(rel(p, dir), opposite(dir)));
    }
    return best;
}

int BlockUpdates::signalFrom(const BlockPos& p, Direction toward) const {
    const BlockStateId s = at(p);
    return conductor(s) ? strongInto(p) : weakAt(p, toward);
}

int BlockUpdates::bestNeighbourSignal(const BlockPos& p) const {
    int best = 0;
    for (int d = 0; d < kDirectionCount && best < 15; ++d) {
        const Direction dir = static_cast<Direction>(d);
        best = std::max(best, signalFrom(rel(p, dir), opposite(dir)));
    }
    return best;
}

Direction BlockUpdates::chestClockwise(Direction f) {
    // A chest of type "right" has its partner clockwise of its facing, "left"
    // counter-clockwise (N -> E -> S -> W).
    switch (f) {
    case Direction::North:
        return Direction::East;
    case Direction::East:
        return Direction::South;
    case Direction::South:
        return Direction::West;
    default:
        return Direction::North;
    }
}

std::optional<BlockPos> BlockUpdates::chestPartner(const World& world, const BlockPos& p) {
    const BlockStateId s = world.getBlock(p);
    if (blockOf(s) != B::Chest) return std::nullopt;
    const int t = R().get(s, chestType);
    if (t == 0) return std::nullopt;
    const Direction cw = chestClockwise(hFacing(s));
    const BlockPos q = rel(p, t == 2 ? cw : opposite(cw));
    if (blockOf(world.getBlock(q)) != B::Chest) return std::nullopt;
    return q;
}

// --- Falling blocks ---------------------------------------------------------------

bool BlockUpdates::hasGravity(BlockId b) {
    return b == B::Sand || b == B::RedSand || b == B::Gravel || b == B::Anvil ||
           b == B::ChippedAnvil || b == B::DamagedAnvil;
}

bool BlockUpdates::sugarCaneCanStay(const World& world, const BlockPos& p) {
    // On more cane, or on dirt/grass/sand next to water (wiki: Sugar Cane).
    const BlockPos below{p.x, p.y - 1, p.z};
    const BlockId b = blockOf(world.getBlock(below));
    if (b == B::SugarCane) return true;
    if (b != B::GrassBlock && b != B::Dirt && b != B::CoarseDirt && b != B::Podzol && b != B::Mycelium &&
        b != B::Sand && b != B::RedSand)
        return false;
    for (const Direction d : kHorizontal)
        if (blockOf(world.getBlock(rel(below, d))) == B::Water) return true;
    return false;
}

bool BlockUpdates::cactusCanStay(const World& world, const BlockPos& p) {
    const BlockId below = blockOf(world.getBlock({p.x, p.y - 1, p.z}));
    if (below != B::Cactus && below != B::Sand && below != B::RedSand) return false;
    for (const Direction d : kHorizontal) {
        const BlockStateId n = world.getBlock(rel(p, d));
        if (R().collides(n) || blockOf(n) == B::Lava) return false;
    }
    return true;
}

bool BlockUpdates::mushroomCanStay(const World& world, const BlockPos& p) {
    const BlockPos below{p.x, p.y - 1, p.z};
    const BlockId ground = blockOf(world.getBlock(below));
    if (ground == B::Mycelium || ground == B::Podzol) return true; // any light (vanilla #mushroom_grow_block)
    if (!R().opaqueCube(world.getBlock(below))) return false;
    // Unlit chunks (just generated, light not computed yet) count as dark.
    const Chunk* c = world.chunk(p.chunk());
    if (!c || !c->lit() || !world.isInHeight(p.y)) return true;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    const int sky = world.hasSkyLight() ? c->skyLight(x, p.y, z) : 0;
    return std::max<int>(sky, c->blockLight(x, p.y, z)) < 13;
}

bool BlockUpdates::netherPlantCanStay(const World& world, const BlockPos& p, BlockId plant) {
    if (plant == B::WeepingVines || plant == B::WeepingVinesPlant) {
        const BlockStateId above = world.getBlock({p.x, p.y + 1, p.z});
        const BlockId a = blockOf(above);
        return a == B::WeepingVines || a == B::WeepingVinesPlant || R().collides(above);
    }
    if (plant == B::TwistingVines || plant == B::TwistingVinesPlant) {
        const BlockStateId below = world.getBlock({p.x, p.y - 1, p.z});
        const BlockId b = blockOf(below);
        return b == B::TwistingVines || b == B::TwistingVinesPlant || R().collides(below);
    }
    const BlockId b = blockOf(world.getBlock({p.x, p.y - 1, p.z}));
    return b == B::CrimsonNylium || b == B::WarpedNylium || b == B::SoulSoil || b == B::GrassBlock || b == B::Dirt ||
           b == B::CoarseDirt || b == B::Podzol || b == B::Mycelium || b == B::Farmland;
}

bool BlockUpdates::chorusCanStay(const World& world, const BlockPos& p, BlockId block) {
    const auto isPlant = [&](const BlockPos& q) { return blockOf(world.getBlock(q)) == B::ChorusPlant; };
    const auto grounded = [&](const BlockPos& q) {
        const BlockId b = blockOf(world.getBlock({q.x, q.y - 1, q.z}));
        return b == B::ChorusPlant || b == B::EndStone;
    };
    if (grounded(p)) return true;
    if (block == B::ChorusFlower) return false;
    const bool above = world.getBlock({p.x, p.y + 1, p.z}) != 0, below = world.getBlock({p.x, p.y - 1, p.z}) != 0;
    for (const Direction d : {Direction::North, Direction::South, Direction::West, Direction::East}) {
        const BlockPos q = rel(p, d);
        if (isPlant(q) && grounded(q)) return !(above && below);
    }
    return false;
}

BlockStateId BlockUpdates::chorusConnected(const World& world, const BlockPos& p, BlockStateId plant) {
    const auto& r = R();
    auto joins = [&](Direction d) {
        const BlockId b = blockOf(world.getBlock(rel(p, d)));
        return b == B::ChorusPlant || b == B::ChorusFlower || (d == Direction::Down && b == B::EndStone);
    };
    // ("true" is value 0 of these properties)
    plant = r.set(plant, faceDown, joins(Direction::Down) ? 0 : 1);
    plant = r.set(plant, fireUp, joins(Direction::Up) ? 0 : 1);
    plant = r.set(plant, fireNorth, joins(Direction::North) ? 0 : 1);
    plant = r.set(plant, fireSouth, joins(Direction::South) ? 0 : 1);
    plant = r.set(plant, fireWest, joins(Direction::West) ? 0 : 1);
    return r.set(plant, fireEast, joins(Direction::East) ? 0 : 1);
}

bool BlockUpdates::isDoor(BlockId b) { return b == B::OakDoor || b == B::IronDoor; }
bool BlockUpdates::isPressurePlate(BlockId b) {
    return b == B::OakPressurePlate || b == B::StonePressurePlate || b == B::LightWeightedPressurePlate ||
           b == B::HeavyWeightedPressurePlate || b == B::DetectorRail; // (detector rails: pressed by minecarts)
}

BlockStateId BlockUpdates::fenceConnected(const World& world, const BlockPos& p, BlockStateId fence) {
    const auto& r = R();
    auto joins = [&](Direction d) {
        const BlockStateId s = world.getBlock(rel(p, d));
        const BlockId b = blockOf(s);
        return b == B::OakFence || b == B::OakFenceGate || r.opaqueCube(s) || b == B::Glass || b == B::SlimeBlock;
    };
    fence = r.set(fence, fireNorth, joins(Direction::North) ? 0 : 1);
    fence = r.set(fence, fireSouth, joins(Direction::South) ? 0 : 1);
    fence = r.set(fence, fireWest, joins(Direction::West) ? 0 : 1);
    return r.set(fence, fireEast, joins(Direction::East) ? 0 : 1);
}

BlockStateId BlockUpdates::stairsShaped(const World& world, const BlockPos& p, BlockStateId st) {
    const auto& r = R();
    const Direction f = hFacing(st);
    const int half = r.get(st, slabHalf);
    auto stairsAt = [&](const BlockPos& q, BlockStateId& out) {
        out = world.getBlock(q);
        return r.kind(blockOf(out)) == BlockKind::Stairs && r.get(out, slabHalf) == half;
    };
    // Counterclockwise from a horizontal direction (north -> west -> south -> east).
    auto ccw = [](Direction d) {
        switch (d) {
        case Direction::North: return Direction::West;
        case Direction::West: return Direction::South;
        case Direction::South: return Direction::East;
        default: return Direction::North;
        }
    };
    auto perpendicular = [](Direction a, Direction b) {
        const bool az = a == Direction::North || a == Direction::South, bz = b == Direction::North || b == Direction::South;
        return az != bz;
    };
    // A side may bend only if the stair beside it isn't a straight continuation of this one.
    auto free = [&](Direction side) {
        BlockStateId n;
        return !stairsAt(rel(p, side), n) || hFacing(n) != f;
    };
    BlockStateId n;
    if (stairsAt(rel(p, f), n) && perpendicular(hFacing(n), f) && free(opposite(hFacing(n))))
        return r.set(st, stairShape, hFacing(n) == ccw(f) ? 3 : 4); // outer_left / outer_right
    if (stairsAt(rel(p, opposite(f)), n) && perpendicular(hFacing(n), f) && free(hFacing(n)))
        return r.set(st, stairShape, hFacing(n) == ccw(f) ? 1 : 2); // inner_left / inner_right
    return r.set(st, stairShape, 0);
}

std::optional<BlockStateId> BlockUpdates::concreteFor(BlockStateId powder) {
    const std::string_view id = R().block(R().blockOf(powder)).id;
    if (!id.ends_with("_concrete_powder")) return std::nullopt;
    const auto concrete = R().findBlock(id.substr(0, id.size() - 7)); // "..._concrete"
    return concrete ? std::optional(R().defaultState(*concrete)) : std::nullopt;
}

bool BlockUpdates::hardenPowder(const BlockPos& p, BlockStateId s) {
    // Concrete powder touching water on any side but its bottom hardens (wiki: Concrete
    // Powder).
    const auto concrete = concreteFor(s);
    if (!concrete) return false;
    for (const Direction d : {Direction::Up, Direction::North, Direction::South, Direction::West, Direction::East})
        if (blockOf(at(rel(p, d))) == B::Water) {
            set(p, *concrete);
            return true;
        }
    return false;
}

BlockStateId BlockUpdates::paneConnected(const World& world, const BlockPos& p, BlockStateId pane) {
    const auto& r = R();
    auto joins = [&](Direction d) {
        const BlockStateId s = world.getBlock(rel(p, d));
        const BlockId b = blockOf(s);
        const BlockKind k = r.kind(b);
        return k == BlockKind::Pane || k == BlockKind::Wall || b == B::IronBars || b == B::Glass ||
               r.block(b).id.ends_with("_stained_glass") || r.opaqueCube(s);
    };
    pane = r.set(pane, fireNorth, joins(Direction::North) ? 0 : 1);
    pane = r.set(pane, fireSouth, joins(Direction::South) ? 0 : 1);
    pane = r.set(pane, fireWest, joins(Direction::West) ? 0 : 1);
    return r.set(pane, fireEast, joins(Direction::East) ? 0 : 1);
}

BlockStateId BlockUpdates::wallConnected(const World& world, const BlockPos& p, BlockStateId wall) {
    const auto& r = R();
    const BlockStateId above = world.getBlock(rel(p, Direction::Up));
    const bool aboveWall = r.kind(blockOf(above)) == BlockKind::Wall;
    auto joins = [&](Direction d) {
        const BlockStateId s = world.getBlock(rel(p, d));
        const BlockId b = blockOf(s);
        return r.kind(b) == BlockKind::Wall || r.kind(b) == BlockKind::Pane || b == B::IronBars || b == B::OakFenceGate ||
               r.opaqueCube(s);
    };
    const Property* sides[4] = {&wallNorth, &wallSouth, &wallWest, &wallEast};
    const Direction dirs[4] = {Direction::North, Direction::South, Direction::West, Direction::East};
    bool joined[4];
    for (int i = 0; i < 4; ++i) {
        joined[i] = joins(dirs[i]);
        // Tall under a full block, or under a wall reaching out the same way.
        const bool tall = r.opaqueCube(above) || (aboveWall && r.get(above, *sides[i]) != 0);
        wall = r.set(wall, *sides[i], joined[i] ? (tall ? 2 : 1) : 0);
    }
    // No post on a straight run (two opposite arms only) unless the wall above has one.
    const bool straight = (joined[0] && joined[1] && !joined[2] && !joined[3]) ||
                          (joined[2] && joined[3] && !joined[0] && !joined[1]);
    const bool post = !straight || (aboveWall && r.get(above, fireUp) == 0);
    return r.set(wall, fireUp, post ? 0 : 1);
}

void BlockUpdates::setDoor(const BlockPos& lower, BlockStateId s, bool openNow, bool poweredNow) {
    // Both halves change together (the upper half mirrors the lower one).
    const BlockStateId lowerNow = withFlag(withFlag(s, open, openNow), powered, poweredNow);
    if (openNow != flag(s, open))
        m_world.playSound(openNow ? Sound::DoorOpen : Sound::DoorClose, lower.x + 0.5, lower.y + 0.5, lower.z + 0.5);
    set(lower, lowerNow);
    const BlockPos up{lower.x, lower.y + 1, lower.z};
    if (isDoor(blockOf(at(up)))) set(up, R().set(lowerNow, doorHalf, 0));
}

int BlockUpdates::plateTarget(BlockId b, int count) const {
    if (count <= 0) return 0;
    if (b == B::LightWeightedPressurePlate) return std::min(15, count);
    if (b == B::HeavyWeightedPressurePlate) return (std::min(count, 150) + 9) / 10;
    return 15;
}

void BlockUpdates::pressPlate(const BlockPos& p, bool item, bool minecart) {
    const BlockId b = blockOf(at(p));
    if (!isPressurePlate(b) || (item && b == B::StonePressurePlate)) return; // (stone: mobs and players only)
    if (b == B::DetectorRail && !minecart) return;
    if (b == B::StonePressurePlate && minecart) return; // (stone: the rider, not the cart)
    for (Plate& pl : m_plates)
        if (pl.pos == p) {
            if (pl.time != m_now) pl.count = 0;
            pl.time = m_now;
            ++pl.count;
            return;
        }
    if (m_plates.size() < 1024) m_plates.push_back({p, m_now, 1});
}

bool BlockUpdates::railPowered(const BlockPos& p, BlockStateId s) const {
    // Powered (or activator) rails pass power on along their line: one with power of
    // its own lights up to 8 more of the same kind (wiki: Powered Rail).
    if (bestNeighbourSignal(p) > 0) return true;
    const BlockId kind = blockOf(s);
    const RailExits ex = railExits(railShapeOf(s));
    for (const Direction start : {ex.a, ex.b}) {
        BlockPos q = p;
        Direction d = start;
        for (int i = 0; i < 8; ++i) {
            BlockPos next = rel(q, d);
            if (blockOf(at(next)) != kind) next.y += 1;
            if (blockOf(at(next)) != kind) next.y -= 2;
            const BlockStateId ns = at(next);
            if (blockOf(ns) != kind) break;
            const int shape = railShapeOf(ns);
            const bool alongZ = d == Direction::North || d == Direction::South;
            if (shape == 1 ? alongZ : shape == 0 ? !alongZ : false) break; // (turned across: not in line)
            if (bestNeighbourSignal(next) > 0) return true;
            q = next;
        }
    }
    return false;
}

void BlockUpdates::primeTnt(const BlockPos& p) {
    if (blockOf(at(p)) != B::Tnt || m_tntPrimed.size() >= m_tntPrimed.capacity()) return; // (full: lit next tick)
    set(p, 0);
    m_tntPrimed.push_back(p);
    m_world.playSound(Sound::Fuse, p.x + 0.5, p.y + 0.5, p.z + 0.5);
}

void BlockUpdates::settlePlates() {
    // Forget plates nothing has stood on for 2 s (a plate broken while pressed never
    // runs its release tick).
    std::erase_if(m_plates, [&](const Plate& pl) { return m_now - pl.time > 40; });
    for (const Plate& pl : m_plates) {
        if (pl.time != m_now) continue;
        const BlockStateId s = at(pl.pos);
        const BlockId b = blockOf(s);
        if (!isPressurePlate(b)) continue;
        const int want = plateTarget(b, pl.count);
        const bool weighted = b == B::LightWeightedPressurePlate || b == B::HeavyWeightedPressurePlate;
        const int now = weighted ? R().get(s, power) : (flag(s, powered) ? 15 : 0);
        if (want > now) set(pl.pos, weighted ? R().set(s, power, want) : withFlag(s, powered, true));
        if (!hasTick(pl.pos, b)) schedule(pl.pos, b, weighted ? 10 : 20, 0);
    }
}

BlockStateId BlockUpdates::barsConnected(const World& world, const BlockPos& p, BlockStateId bars) {
    const auto& r = R();
    auto joins = [&](Direction d) {
        const BlockStateId s = world.getBlock(rel(p, d));
        return blockOf(s) == B::IronBars || r.kind(blockOf(s)) == BlockKind::Wall ||
               r.kind(blockOf(s)) == BlockKind::Pane || r.opaqueCube(s);
    };
    bars = r.set(bars, fireNorth, joins(Direction::North) ? 0 : 1);
    bars = r.set(bars, fireSouth, joins(Direction::South) ? 0 : 1);
    bars = r.set(bars, fireWest, joins(Direction::West) ? 0 : 1);
    return r.set(bars, fireEast, joins(Direction::East) ? 0 : 1);
}

bool BlockUpdates::replaceable(BlockStateId s) {
    // Blocks others replace when placed into them (wiki: Replaceable): air, fluids,
    // fire, short grass, ferns, dead bushes, a single snow layer. Not flowers or torches.
    const BlockId b = blockOf(s);
    return s == 0 || b == B::Water || b == B::Lava || b == B::Fire || b == B::ShortGrass ||
           b == B::Fern || b == B::DeadBush || (b == B::Snow && R().get(s, layers) == 0);
}

bool BlockUpdates::fallThrough(BlockStateId below) { return replaceable(below); }

// --- Updates ----------------------------------------------------------------------

void BlockUpdates::set(const BlockPos& p, BlockStateId s) {
    const BlockStateId old = at(p);
    if (old == s) return;
    m_world.setBlock(p, s);
    record(p, old, s);
    afterChange(p, old, s);
}

void BlockUpdates::setDiode(const BlockPos& p, BlockStateId s) {
    // A repeater turning on/off updates only the block in front and its neighbours
    // (wiki: Block update - exceptions).
    setRaw(p, s);
    reach(p, s);
}

void BlockUpdates::setRaw(const BlockPos& p, BlockStateId s) {
    const BlockStateId old = at(p);
    if (old == s) return;
    m_world.setBlock(p, s);
    record(p, old, s);
}

void BlockUpdates::alertObservers(const BlockPos& p) {
    // Observers looking at this block notice the change, whoever made it - players,
    // pistons, repeaters and comparators too (wiki: Observer).
    for (int d = 0; d < kDirectionCount; ++d) {
        const Direction dir = static_cast<Direction>(d);
        const BlockPos q = rel(p, dir);
        const BlockStateId o = at(q);
        if (blockOf(o) == B::Observer && facing6Of(o) == opposite(dir) && !flag(o, powered) && !hasTick(q, B::Observer))
            schedule(q, B::Observer, 2, 0);
    }
}

void BlockUpdates::record(const BlockPos& p, BlockStateId old, BlockStateId now) {
    if (old != now) alertObservers(p);
    // Leaf distance, sapling stage and fire age change neither light nor the model:
    // nothing to relight or re-mesh.
    if (blockOf(old) == blockOf(now) &&
        (isLeaves(blockOf(now)) || blockOf(now) == B::Fire || blockOf(now) == B::OakSapling ||
         blockOf(now) == B::BirchSapling || blockOf(now) == B::SpruceSapling ||
         blockOf(now) == B::AcaciaSapling || blockOf(now) == B::JungleSapling ||
         blockOf(now) == B::DarkOakSapling || blockOf(now) == B::CherrySapling ||
         blockOf(now) == B::SugarCane || blockOf(now) == B::Cactus))
        return;
    // Farmland moisture below 7 looks the same; carrots/potatoes share a texture
    // across ages 0-1, 2-3, 4-6.
    if (blockOf(old) == blockOf(now) && blockOf(now) == B::Farmland &&
        (R().get(old, moisture) == 7) == (R().get(now, moisture) == 7))
        return;
    if (blockOf(old) == blockOf(now) &&
        (blockOf(now) == B::Carrots || blockOf(now) == B::Potatoes)) {
        auto stageOf = [](int a) { return a < 2 ? 0 : a < 4 ? 1 : a < 7 ? 2 : 3; };
        if (stageOf(R().get(old, age7)) == stageOf(R().get(now, age7))) return;
    }
    // Light only needs recomputing when emission or opacity changed (dust power,
    // repeater and lever states only change the model).
    const auto& r = R();
    const bool light = r.lightEmission(old) != r.lightEmission(now) ||
                       r.lightOpacity(old) != r.lightOpacity(now) ||
                       r.opaqueCube(old) != r.opaqueCube(now);
    auto fluidOrAir = [](BlockStateId s) { return s == 0 || isFluid(blockOf(s)); };
    auto& list = !light ? m_remesh : fluidOrAir(old) && fluidOrAir(now) ? m_settling : m_changed;
    if (list.empty() || !(list.back() == p)) list.push_back(p); // a block changing again: once
}

void BlockUpdates::onBlockChanged(const BlockPos& p, BlockStateId old, BlockStateId now) {
    // A bed's foot placed by a player brings its head (one block toward its facing).
    if (blockOf(now) == B::RedBed && R().get(now, bedPart) == 1) {
        const BlockPos head = rel(p, hFacing(now));
        if (blockOf(at(head)) != B::RedBed && replaceable(at(head)))
            set(head, R().set(now, bedPart, 0));
    }
    // A door's lower half placed by a player brings its upper half (wiki: Door).
    if (isDoor(blockOf(now)) && R().get(now, doorHalf) == 1 && !isDoor(blockOf(old))) {
        const BlockPos up{p.x, p.y + 1, p.z};
        if (replaceable(at(up))) set(up, R().set(now, doorHalf, 0));
    }
    afterChange(p, old, now);
    neighbourChanged(p); // the new block checks its surroundings (vanilla onPlace)
}

void BlockUpdates::afterChange(const BlockPos& p, BlockStateId old, BlockStateId now) {
    if (old != now) alertObservers(p); // (edits that came through World::updateBlock skip record)
    if (hardenPowder(p, now)) return; // (M23.4a: concrete powder placed or landed by water)
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
                if (const ItemId item = itemRegistry().blockItem(blockOf(b)))
                    m_drops.push_back({base, {item, 1}});
            set(base, 0);
        }
    }
    notifyNeighbours(p);
    reach(p, old);
    // Same block in a new state: its reach only moves if what it points at changed.
    const bool sameTarget =
        blockOf(now) == blockOf(old) &&
        ((is != B::Lever && !isButton(is)) || attachDir(old) == attachDir(now)) &&
        (is != B::Repeater || hFacing(old) == hFacing(now));
    if (!sameTarget) reach(p, now);
}

void BlockUpdates::reach(const BlockPos& p, BlockStateId s) {
    // Components also update around the blocks they power (vanilla: dust and torches
    // all six neighbours' neighbours, levers/buttons the block they hang on, repeaters
    // the block in front; a block of redstone only its own neighbours).
    switch (blockOf(s)) {
    case B::RedstoneWire:
        for (const Direction d : kUpdateOrder)
            notifyNeighbours(rel(p, d));
        break;
    case B::RedstoneTorch:
    case B::RedstoneWallTorch: // outer order down, up, north, south, west, east (wiki: Block
                               // update)
        for (int d = 0; d < kDirectionCount; ++d)
            notifyNeighbours(rel(p, static_cast<Direction>(d)));
        break;
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
        notifyNeighbours(rel(p, attachDir(s)));
        break;
    case B::OakPressurePlate:
    case B::StonePressurePlate:
    case B::LightWeightedPressurePlate:
    case B::HeavyWeightedPressurePlate:
    case B::DetectorRail:
        notifyNeighbours(rel(p, Direction::Down));
        break;
    case B::Observer: {
        const BlockPos back = rel(p, opposite(facing6Of(s)));
        neighbourChanged(back);
        notifyNeighbours(back);
        break;
    }
    case B::Repeater:
    case B::Comparator: {
        const BlockPos front = rel(p, opposite(hFacing(s)));
        neighbourChanged(front);
        notifyNeighbours(front);
        break;
    }
    default:
        break;
    }
}

void BlockUpdates::notifyNeighbours(const BlockPos& p) {
    for (const Direction d : kUpdateOrder)
        neighbourChanged(rel(p, d));
}

void BlockUpdates::pop(const BlockPos& p) {
    m_drops.push_back({p, {}, at(p)}); // its loot, as if broken by hand
    set(p, 0);
}

bool BlockUpdates::survives(const BlockPos& p, BlockStateId s) const {
    switch (R().kind(blockOf(s))) {
    case BlockKind::Carpet: return at(rel(p, Direction::Down)) != 0; // (any block below)
    case BlockKind::Sign: return supports(at(rel(p, Direction::Down))); // (M23.3c)
    case BlockKind::HangingSign: return supports(at(rel(p, Direction::Up)));
    case BlockKind::WallSign:
    case BlockKind::WallHangingSign: return supports(at(rel(p, opposite(hFacing(s)))));
    default: break;
    }
    switch (blockOf(s)) {
    case B::Torch: // (M23.2: plain torches pop too when their support goes)
    case B::SoulTorch:
        return supports(at(rel(p, Direction::Down)));
    case B::WallTorch:
    case B::SoulWallTorch:
    case B::Ladder:
        return supports(at(rel(p, opposite(hFacing(s)))));
    case B::Bamboo: { // on bamboo or ground it can root in (wiki: Bamboo)
        const BlockStateId below = at(rel(p, Direction::Down));
        const BlockId bb = blockOf(below);
        return bb == B::Bamboo || plantableSoil(below) || bb == B::Sand || bb == B::RedSand || bb == B::Gravel;
    }
    case B::Lantern: // hanging from the block above, or standing (wiki: Lantern)
    case B::SoulLantern:
        return supports(at(rel(p, R().get(s, hanging) == 0 ? Direction::Up : Direction::Down)));
    case B::RedstoneWire:
    case B::RedstoneTorch:
    case B::Repeater:
        return supports(at(rel(p, Direction::Down)));
    case B::RedstoneWallTorch:
        return supports(at(rel(p, opposite(hFacing(s)))));
    case B::Lever:
    case B::StoneButton:
    case B::OakButton:
        return supports(at(rel(p, attachDir(s))));
    default:
        return true;
    }
}

void BlockUpdates::neighbourChanged(const BlockPos& p) {
    if (!m_world.isInHeight(p.y)) return;
    if (m_depth > 2048) { // runaway update chain: stop (vanilla also caps its updates)
        static bool logged = false;
        if (!logged) MC_LOG_WARN("Block updates nested too deeply; some were skipped");
        logged = true;
        return;
    }
    ++m_depth;
    const BlockStateId s = at(p);
    if (hardenPowder(p, s)) {
        --m_depth;
        return;
    }
    if (blockOf(s) == B::Campfire || blockOf(s) == B::SoulCampfire) { // signal fire follows the hay below
        const BlockStateId want = R().set(s, signalFire, blockOf(at(rel(p, Direction::Down))) == B::HayBlock ? 0 : 1);
        if (want != s) set(p, want);
        --m_depth;
        return;
    }
    if (R().block(R().blockOf(s)).id.ends_with("copper_bulb")) { // (M23.4b)
        updateBulb(p, s);
        --m_depth;
        return;
    }
    if (const BlockKind kind = R().kind(blockOf(s));
        kind == BlockKind::Stairs || kind == BlockKind::Wall || kind == BlockKind::Pane) {
        const BlockStateId want = kind == BlockKind::Stairs ? stairsShaped(m_world, p, s)
                                  : kind == BlockKind::Wall ? wallConnected(m_world, p, s)
                                                            : paneConnected(m_world, p, s);
        if (want != s) set(p, want);
        --m_depth;
        return;
    }
    // Small blocks that need their support (M23.2): torches, wall torches, lanterns,
    // ladders, carpets.
    if (const BlockId b = blockOf(s); b == B::Torch || b == B::SoulTorch || b == B::WallTorch || b == B::SoulWallTorch ||
                                      b == B::Lantern || b == B::SoulLantern || b == B::Ladder || b == B::Bamboo ||
                                      R().kind(b) == BlockKind::Carpet || R().kind(b) == BlockKind::Sign ||
                                      R().kind(b) == BlockKind::WallSign || R().kind(b) == BlockKind::HangingSign ||
                                      R().kind(b) == BlockKind::WallHangingSign) {
        if (!survives(p, s)) pop(p);
        --m_depth;
        return;
    }
    switch (blockOf(s)) {
    case B::RedstoneWire:
        updateWire(p);
        break;
    case B::RedstoneTorch:
    case B::RedstoneWallTorch:
        if (!survives(p, s))
            pop(p);
        else if (flag(s, lit) == torchInput(p, s) && !hasTick(p, blockOf(s)))
            schedule(p, blockOf(s), 2, 0);
        break;
    case B::Repeater: {
        if (!survives(p, s)) {
            pop(p);
            break;
        }
        const bool lockedNow = repeaterLocked(p, s);
        if (lockedNow != flag(s, locked))
            setRaw(p, withFlag(s, locked, lockedNow)); // no updates (vanilla)
        if (lockedNow) break;
        const BlockStateId cur = at(p);
        const bool should = repeaterInput(p, cur) > 0;
        if (flag(cur, powered) != should && !hasTick(p, B::Repeater)) {
            // Priorities (wiki: Tick › Scheduled tick): -3 when the repeater faces into
            // the side or back of another repeater, -2 when turning off, else -1.
            const BlockStateId front = at(rel(p, opposite(hFacing(cur))));
            const int priority =
                blockOf(front) == B::Repeater && hFacing(front) != opposite(hFacing(cur)) ? -3
                : flag(cur, powered)                                                      ? -2
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
            std::none_of(m_events.begin(), m_events.end(),
                         [&](const Event& e) { return e.pos == p && e.extend == should; }))
            m_events.push_back({p, should, !m_inTick});
        break;
    }
    case B::Water:
    case B::Lava:
        fluidNeighbourChanged(p, s);
        break;
    case B::Fire:
        fireNeighbourChanged(p);
        break;
    case B::Chest: {
        // Keep double chests paired: a half whose partner is gone turns single; a
        // single chest takes the free side of a neighbour half pointing at it.
        const Direction f = hFacing(s), cw = chestClockwise(f);
        auto partnerOf = [&](BlockStateId st, const BlockPos& at) -> std::optional<BlockPos> {
            const int t = R().get(st, chestType);
            if (t == 0) return std::nullopt;
            const Direction pc = chestClockwise(hFacing(st));
            return rel(at, t == 2 ? pc : opposite(pc));
        };
        int want = 0;
        for (const auto& [side, myType] : {std::pair{cw, 2}, std::pair{opposite(cw), 1}}) {
            const BlockPos q = rel(p, side);
            const BlockStateId n = at(q);
            if (blockOf(n) == B::Chest && hFacing(n) == f) {
                const auto back = partnerOf(n, q);
                if (back && *back == p) want = myType;
            }
        }
        if (want != R().get(s, chestType)) setRaw(p, R().set(s, chestType, want));
        break;
    }
    case B::RedBed: {
        // The two halves go together: the foot's head is toward `facing`.
        const bool foot = R().get(s, bedPart) == 1;
        const BlockPos other = rel(p, foot ? hFacing(s) : opposite(hFacing(s)));
        const BlockStateId o = at(other);
        if (blockOf(o) != B::RedBed || hFacing(o) != hFacing(s) ||
            R().get(o, bedPart) == R().get(s, bedPart))
            set(p, 0); // (no drop: the half that was broken dropped the bed)
        break;
    }
    case B::Farmland: // a solid block on top turns it to dirt (wiki: Farmland)
        if (R().collides(at(rel(p, Direction::Up)))) set(p, R().defaultState(B::Dirt));
        break;
    case B::Wheat:
    case B::Carrots:
    case B::Potatoes:
    case B::Beetroots:
        if (blockOf(at(rel(p, Direction::Down))) != B::Farmland) pop(p); // lost its farmland
        break;
    case B::Sand:
    case B::RedSand:
    case B::Gravel:
    case B::Anvil:
    case B::ChippedAnvil:
    case B::DamagedAnvil:
    case B::DragonEgg:
        if (fallThrough(at(rel(p, Direction::Down))))
            schedule(p, blockOf(s), 2, 0); // wiki: 2 ticks
        break;
    case B::SugarCane:
        if (!sugarCaneCanStay(m_world, p)) pop(p);
        break;
    case B::Cactus:
        if (!cactusCanStay(m_world, p)) pop(p);
        break;
    case B::BrownMushroom:
    case B::RedMushroom:
        if (!R().opaqueCube(at(rel(p, Direction::Down))))
            pop(p); // (light is checked on placing and spreading)
        break;
    case B::NetherWart: // grows only on soul sand (wiki: Nether Wart)
        if (blockOf(at(rel(p, Direction::Down))) != B::SoulSand) pop(p);
        break;
    case B::ChorusPlant: // (vanilla breaks it on a scheduled tick; ours at once)
        if (!chorusCanStay(m_world, p, B::ChorusPlant)) pop(p);
        else set(p, chorusConnected(m_world, p, s));
        break;
    case B::ChorusFlower:
        if (!chorusCanStay(m_world, p, B::ChorusFlower)) pop(p);
        break;
    case B::IronBars:
        set(p, barsConnected(m_world, p, s));
        break;
    case B::OakFence:
        set(p, fenceConnected(m_world, p, s));
        break;
    case B::Rail:
    case B::PoweredRail:
    case B::DetectorRail:
    case B::ActivatorRail: {
        if (!supports(at(rel(p, Direction::Down)))) {
            pop(p);
            break;
        }
        BlockStateId want = withRailShape(s, chooseRailShape(m_world, p, s));
        if (blockOf(s) == B::PoweredRail || blockOf(s) == B::ActivatorRail)
            want = withFlag(want, powered, railPowered(p, want));
        if (want != s) set(p, want);
        break;
    }
    case B::Dispenser:
    case B::Dropper: {
        const bool on = bestNeighbourSignal(p) > 0 || bestNeighbourSignal(rel(p, Direction::Up)) > 0;
        if (on && !flag(s, triggered)) {
            setRaw(p, withFlag(s, triggered, true));
            schedule(p, blockOf(s), 4, 0); // (wiki: fires 4 game ticks later)
        } else if (!on && flag(s, triggered)) {
            setRaw(p, withFlag(s, triggered, false));
        }
        break;
    }
    case B::Hopper: { // power turns it off (wiki: Hopper)
        const bool on = bestNeighbourSignal(p) == 0;
        if (on != flag(s, enabled)) set(p, withFlag(s, enabled, on));
        break;
    }
    case B::Comparator:
        if (!supports(at(rel(p, Direction::Down)))) {
            pop(p);
            break;
        }
        comparatorChanged(p, s);
        break;
    case B::MovingPiston: { // one left without its move (a world saved mid-push) clears
        bool flying = false;
        for (const Moving& mv : m_moving)
            flying = flying || mv.to == p;
        if (!flying) set(p, 0);
        break;
    }
    case B::Tnt: // lit by redstone power (also when placed next to it)
        if (bestNeighbourSignal(p) > 0) primeTnt(p);
        break;
    case B::OakDoor:
    case B::IronDoor: {
        const bool upper = R().get(s, doorHalf) == 0;
        const BlockPos lower = upper ? BlockPos{p.x, p.y - 1, p.z} : p;
        const BlockStateId ls = upper ? at(lower) : s;
        if (upper) { // without its lower half it goes (the lower half drops the door)
            if (!isDoor(blockOf(ls)) || R().get(ls, doorHalf) != 1) set(p, 0);
            else neighbourChanged(lower); // (power reaching the top half opens it too)
            break;
        }
        const BlockPos up{p.x, p.y + 1, p.z};
        if (!isDoor(blockOf(at(up))) || !supports(at(rel(p, Direction::Down)))) {
            pop(p);
            break;
        }
        // Redstone: power at either half opens it; losing it closes it (wiki: Door).
        const bool on = bestNeighbourSignal(p) > 0 || bestNeighbourSignal(up) > 0;
        if (on != flag(s, powered)) setDoor(p, s, on, on);
        break;
    }
    case B::OakTrapdoor:
    case B::IronTrapdoor:
    case B::OakFenceGate: {
        const bool on = bestNeighbourSignal(p) > 0;
        if (on != flag(s, powered)) set(p, withFlag(withFlag(s, open, on), powered, on));
        break;
    }
    case B::OakPressurePlate:
    case B::StonePressurePlate:
    case B::LightWeightedPressurePlate:
    case B::HeavyWeightedPressurePlate:
        if (!supports(at(rel(p, Direction::Down)))) pop(p);
        break;
    case B::CrimsonFungus:
    case B::WarpedFungus:
    case B::CrimsonRoots:
    case B::WarpedRoots:
    case B::NetherSprouts:
    case B::WeepingVines:
    case B::WeepingVinesPlant:
    case B::TwistingVines:
    case B::TwistingVinesPlant: {
        const BlockId b = blockOf(s);
        if (!netherPlantCanStay(m_world, p, b)) {
            pop(p);
            break;
        }
        // A strand's end piece becomes its tip again (vanilla: the "plant" part with
        // no more vine beyond it turns into the head).
        const BlockId below = blockOf(at(rel(p, Direction::Down))), above = blockOf(at(rel(p, Direction::Up)));
        if (b == B::WeepingVinesPlant && below != B::WeepingVines && below != B::WeepingVinesPlant)
            set(p, R().defaultState(B::WeepingVines));
        if (b == B::TwistingVinesPlant && above != B::TwistingVines && above != B::TwistingVinesPlant)
            set(p, R().defaultState(B::TwistingVines));
        break;
    }
    case B::OakLeaves:
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
    case B::JungleLeaves:
    case B::DarkOakLeaves:
    case B::CherryLeaves:
    case B::MangroveLeaves:
    case B::PaleOakLeaves:
        leavesChanged(p, s);
        break;
    case B::OakSapling:
    case B::BirchSapling:
    case B::SpruceSapling:
    case B::AcaciaSapling:
    case B::JungleSapling:
    case B::DarkOakSapling:
    case B::CherrySapling:
    case B::PaleOakSapling:
    case B::MangrovePropagule:
        if (!plantableSoil(at(rel(p, Direction::Down)))) pop(p); // lost its soil
        break;
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
    default:
        break;
    }
    --m_depth;
}

// --- Scheduled ticks --------------------------------------------------------------

void BlockUpdates::schedule(const BlockPos& p, BlockId block, int ticks, int priority) {
    Chunk* c = chunkAt(p);
    if (!c) return;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    if (c->hasTick(x, p.y, z, block)) return; // one pending tick per block (vanilla)
    makeAbsolute(*c);
    c->markDirty(); // pending ticks are saved with the chunk
    c->addTick({static_cast<int8_t>(x), static_cast<int8_t>(z), static_cast<int16_t>(p.y),
                static_cast<int8_t>(priority), block, m_now + ticks, m_order++});
    if (!c->inTickingList) m_world.markTicking(c->pos());
}

void BlockUpdates::makeAbsolute(Chunk& c) {
    // Loaded from disk: delays count from now, in their saved order.
    if (!c.ticksRelative) return;
    auto& ticks = c.blockTicks();
    std::sort(ticks.begin(), ticks.end(),
              [](const auto& a, const auto& b) { return a.order < b.order; });
    for (auto& t : ticks) {
        t.time += m_now - 1; // it was loaded before this tick began
        t.order = m_order++;
    }
    c.ticksRelative = false;
}

bool BlockUpdates::hasTick(const BlockPos& p, BlockId block) const {
    Chunk* c = chunkAt(p);
    return c && c->hasTick(blockToLocal(p.x), p.y, blockToLocal(p.z), block);
}

void BlockUpdates::tick() {
    m_inTick = true;
    m_lightning.clear();
    m_due.clear();
    m_world.forEachTickingChunk([&](Chunk& c) {
        if (std::as_const(c).blockTicks().empty()) return;
        // Beyond the simulation distance scheduled ticks wait (vanilla ticks blocks and
        // fluids only in ticking chunks): a far spring stays a lone source until you
        // come near. Their delays start counting once the chunk is in range.
        if (m_rtDistance >= 0 && (std::abs(c.pos().x - m_rtCentre.x) > m_rtDistance ||
                                  std::abs(c.pos().z - m_rtCentre.z) > m_rtDistance))
            return;
        makeAbsolute(c);
        if (c.takeDueTicks(m_now, [&](const Chunk::BlockTick& t) {
                m_due.push_back({{c.pos().x * 16 + t.x, t.y, c.pos().z * 16 + t.z}, t});
            }))
            c.markDirty();
    });
    // Block ticks first, then fluid ticks (vanilla runs them as two phases).
    std::sort(m_due.begin(), m_due.end(), [](const Due& a, const Due& b) {
        const bool fa = isFluid(a.tick.block), fb = isFluid(b.tick.block);
        if (fa != fb) return fb;
        if (a.tick.time != b.tick.time) return a.tick.time < b.tick.time;
        if (a.tick.priority != b.tick.priority) return a.tick.priority < b.tick.priority;
        return a.tick.order < b.tick.order;
    });
    for (const Due& d : m_due) {
        const BlockStateId s = at(d.pos);
        if (blockOf(s) == d.tick.block) tickBlock(d.pos, s);
    }
    watchComparators();
    finishMoves(); // (pistons: blocks 2 ticks in flight land)
    runRandomTicks(); // (vanilla: after block and fluid ticks, before block events)
    runWeatherTicks();
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
        if (e.extend && should && !flag(s, extended))
            extend(e.pos);
        else if (!e.extend && !should && flag(s, extended))
            retract(e.pos);
    }
    m_events.resize(kept);
    m_inTick = false;
    // Torch burnout memory: only the last 60 ticks count.
    std::erase_if(m_toggles, [&](const Toggle& t) { return m_now - t.time > 60; });
}

void BlockUpdates::tickBlock(const BlockPos& p, BlockStateId s) {
    switch (blockOf(s)) {
    case B::RedstoneTorch:
    case B::RedstoneWallTorch: {
        const bool input = torchInput(p, s);
        if (flag(s, lit)) {
            if (input) {
                set(p, withFlag(s, lit, false));
                // Burnout (wiki: Redstone Torch): more than 8 turn-offs within 60 ticks.
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
    case B::Water:
    case B::Lava:
        tickFluid(p, s);
        break;
    case B::Fire:
        tickFire(p, s);
        break;
    case B::Sand:
    case B::RedSand:
    case B::Gravel:
    case B::Anvil:
    case B::ChippedAnvil:
    case B::DamagedAnvil:
    case B::DragonEgg:
        if (p.y > m_world.height().minY && fallThrough(at(rel(p, Direction::Down)))) {
            m_falling.push_back({p, s});
            set(p, 0);
        }
        break;
    case B::OakLeaves:
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
    case B::JungleLeaves:
    case B::DarkOakLeaves:
    case B::MangroveLeaves:
    case B::PaleOakLeaves:
    case B::CherryLeaves: {
        const int d = leafDistance(p);
        if (d != R().get(s, distance) + 1) set(p, R().set(s, distance, d - 1)); // neighbours follow
        break;
    }
    case B::RedstoneLamp:
        if (flag(s, lit) && bestNeighbourSignal(p) == 0) set(p, withFlag(s, lit, false));
        break;
    case B::StoneButton:
    case B::OakButton:
        if (flag(s, powered)) set(p, withFlag(s, powered, false));
        break;
    case B::Comparator: {
        const int want = comparatorTarget(p, s);
        if (Chunk* c = chunkAt(p))
            if (ComparatorData* d = c->comparator(blockToLocal(p.x), p.y, blockToLocal(p.z))) {
                if (d->output == want) break;
                d->output = want;
                c->markDirty();
            }
        const BlockStateId now = withFlag(s, powered, want > 0);
        if (now != s) setRaw(p, now);
        // What it points into hears about it, even when only the strength changed.
        const BlockPos front = rel(p, opposite(hFacing(s)));
        neighbourChanged(front);
        notifyNeighbours(front);
        break;
    }
    case B::Dispenser:
    case B::Dropper:
        if (m_dispensed.size() < m_dispensed.capacity()) m_dispensed.push_back(p);
        break;
    case B::Observer: // (a full update: observers watching this one see it too)
        if (flag(s, powered)) {
            set(p, withFlag(s, powered, false));
        } else {
            set(p, withFlag(s, powered, true));
            schedule(p, B::Observer, 2, 0); // a 2-tick pulse (wiki: Observer)
        }
        break;
    case B::OakPressurePlate:
    case B::StonePressurePlate:
    case B::LightWeightedPressurePlate:
    case B::HeavyWeightedPressurePlate:
    case B::DetectorRail: {
        // Still something on it (this tick or the last)? Stay down; else spring up.
        const BlockId b = blockOf(s);
        const bool weighted = b == B::LightWeightedPressurePlate || b == B::HeavyWeightedPressurePlate;
        int count = 0;
        for (size_t i = 0; i < m_plates.size(); ++i)
            if (m_plates[i].pos == p) {
                if (m_now - m_plates[i].time <= 1) count = m_plates[i].count;
                else m_plates.erase(m_plates.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        const int want = plateTarget(b, count);
        set(p, weighted ? R().set(s, power, want) : withFlag(s, powered, want > 0));
        if (want > 0) schedule(p, b, weighted ? 10 : 20, 0);
        break;
    }
    default:
        break;
    }
}

// --- Dust -------------------------------------------------------------------------

int BlockUpdates::wirePower(const BlockPos& p) const {
    const BlockStateId s = at(p);
    return blockOf(s) == B::RedstoneWire ? R().get(s, power) : 0;
}

BlockStateId BlockUpdates::wireShape(const BlockPos& p, BlockStateId s) const {
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
            const bool connects =
                nb == B::RedstoneWire ||
                (nb == B::Repeater ? (hFacing(ns) == d || hFacing(ns) == opposite(d))
                                   : signalSource(nb));
            if (connects ||
                (!conductor(ns) && blockOf(at(rel(n, Direction::Down))) == B::RedstoneWire))
                side = kSide;
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
        const bool ns = sides[0] != kNone || sides[2] != kNone,
                   ew = sides[1] != kNone || sides[3] != kNone;
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

int BlockUpdates::wireTarget(const BlockPos& p) const {
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

void BlockUpdates::updateWire(const BlockPos& p) {
    const BlockStateId s = at(p);
    if (!survives(p, s)) {
        pop(p);
        return;
    }
    const BlockStateId next = R().set(wireShape(p, s), power, wireTarget(p));
    if (next == s) return;
    // Only a power change updates other components (wiki: Redstone Dust).
    if (R().get(next, power) == R().get(s, power))
        setRaw(p, next);
    else
        set(p, next);
}

// --- Torches, repeaters, pistons ----------------------------------------------------

bool BlockUpdates::torchInput(const BlockPos& p, BlockStateId s) const {
    // Off while the block it's attached to is powered (wiki: Redstone Torch).
    const Direction toTorch = blockOf(s) == B::RedstoneTorch ? Direction::Up : hFacing(s);
    return signalFrom(rel(p, opposite(toTorch)), toTorch) > 0;
}

bool BlockUpdates::toggledTooOften(const BlockPos& p, bool add) {
    if (add) m_toggles.push_back({p, m_now});
    int n = 0;
    for (const Toggle& t : m_toggles)
        if (t.pos == p && m_now - t.time <= 60) ++n;
    return n > 8; // "more than eight" turn-offs (wiki; user decision: the 9th burns out)
}

int BlockUpdates::repeaterInput(const BlockPos& p, BlockStateId s) const {
    const Direction back = hFacing(s); // facing points to the input side
    const BlockPos in = rel(p, back);
    return std::max(signalFrom(in, opposite(back)), wirePower(in));
}

bool BlockUpdates::repeaterLocked(const BlockPos& p, BlockStateId s) const {
    // Locked by a powered repeater pointing into either side (wiki: Redstone Repeater).
    const Direction f = hFacing(s);
    for (const Direction side : kHorizontal) {
        if (side == f || side == opposite(f)) continue;
        const BlockStateId n = at(rel(p, side));
        if (blockOf(n) == B::Repeater && flag(n, powered) && hFacing(n) == side) return true;
    }
    return false;
}

bool BlockUpdates::pistonPowered(const BlockPos& p, Direction f) const {
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

bool BlockUpdates::pushList(const BlockPos& base, Direction f, std::optional<BlockPos>& destroy) {
    m_push.clear();
    destroy.reset();
    BlockPos p = rel(base, f);
    for (;;) {
        if (!m_world.isInHeight(p.y) || !m_world.chunk(p.chunk())) return false;
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

bool BlockUpdates::gatherPush(const BlockPos& base, const BlockPos& first, Direction move,
                              std::vector<BlockPos>& destroy) {
    // Everything that moves: the line of blocks ahead, and with slime blocks every
    // movable block stuck to them, each pushing its own line (wiki: Slime Block ›
    // Behavior). At most 12 blocks; an immovable block in the way stops it all.
    m_push.clear();
    destroy.clear();
    BlockPos queue[64];
    int head = 0, tail = 0;
    queue[tail++] = first;
    auto queued = [&](const BlockPos& q) {
        for (int i = 0; i < tail; ++i)
            if (queue[i] == q) return true;
        return false;
    };
    while (head < tail) {
        const BlockPos q = queue[head++];
        if (!m_world.isInHeight(q.y) || !m_world.chunk(q.chunk())) return false;
        if (q == base) continue;
        const Push k = pushKind(at(q));
        if (k == Push::Air) continue;
        if (k == Push::Destroy) {
            if (destroy.size() < 16) destroy.push_back(q);
            continue;
        }
        if (k == Push::Block) return false;
        if (std::find(m_push.begin(), m_push.end(), q) != m_push.end()) continue;
        if (m_push.size() == 12) return false; // push limit 12
        m_push.push_back(q);
        const BlockPos ahead = rel(q, move);
        if (!queued(ahead) && tail < 64) queue[tail++] = ahead;
        if (blockOf(at(q)) == B::SlimeBlock)
            for (int d = 0; d < kDirectionCount; ++d) {
                const Direction dir = static_cast<Direction>(d);
                if (dir == move) continue;
                const BlockPos n = rel(q, dir);
                if (n == base || queued(n) || tail >= 64) continue;
                if (pushKind(at(n)) == Push::Move) queue[tail++] = n; // stuck to the slime
            }
    }
    return true;
}

void BlockUpdates::extend(const BlockPos& p) {
    const BlockStateId s = at(p);
    const Direction f = facing6Of(s);
    std::vector<BlockPos>& destroy = m_pushDestroy;
    if (!gatherPush(p, rel(p, f), f, destroy)) return;
    if (m_moving.size() + m_push.size() + 1 > m_moving.capacity()) return; // (too much in flight: stays put)
    m_world.playSound(Sound::PistonOut, p.x + 0.5, p.y + 0.5, p.z + 0.5);
    BlockStateId destroyedStates[16];
    for (size_t i = 0; i < destroy.size(); ++i) {
        const BlockPos& d = destroy[i];
        const BlockStateId ds = at(d);
        destroyedStates[i] = ds;
        const BlockId b = blockOf(ds);
        if (b != B::Water && b != B::Lava) m_drops.push_back({d, {}, ds}); // its loot
        setRaw(d, 0);
    }
    // The blocks leave their cells at once; for 2 ticks their targets (and the head's
    // cell) hold moving pistons, then they land.
    m_pushStates.clear();
    for (const BlockPos& q : m_push)
        m_pushStates.push_back(at(q));
    for (const BlockPos& q : m_push)
        setRaw(q, 0);
    const BlockStateId moving = R().set(R().defaultState(B::MovingPiston), facing6, static_cast<int>(f));
    for (size_t i = 0; i < m_push.size(); ++i) {
        const BlockPos to = rel(m_push[i], f);
        setRaw(to, moving);
        if (m_moving.size() < m_moving.capacity()) m_moving.push_back({to, m_pushStates[i], f, m_now});
    }
    setRaw(p, withFlag(s, extended, true));
    BlockStateId head = R().set(R().defaultState(B::PistonHead), facing6, static_cast<int>(f));
    head = R().set(head, pistonType, blockOf(s) == B::StickyPiston ? 1 : 0);
    setRaw(rel(p, f), moving);
    if (m_moving.size() < m_moving.capacity()) m_moving.push_back({rel(p, f), head, f, m_now});
    neighbourChanged(p);
    notifyNeighbours(p);
    notifyNeighbours(rel(p, f));
    for (size_t i = 0; i < destroy.size(); ++i) {
        notifyNeighbours(destroy[i]);
        reach(destroy[i], destroyedStates[i]); // what a broken component powered loses it
    }
    for (const BlockPos& q : m_push)
        notifyNeighbours(q);
}

void BlockUpdates::retract(const BlockPos& p) {
    const BlockStateId s = at(p);
    m_world.playSound(Sound::PistonIn, p.x + 0.5, p.y + 0.5, p.z + 0.5);
    const Direction f = facing6Of(s);
    const BlockPos front = rel(p, f);
    const BlockStateId h = at(front);
    if (blockOf(h) == B::PistonHead && facing6Of(h) == f) {
        setRaw(front, 0);
        if (m_moving.size() < m_moving.capacity()) m_moving.push_back({p, h, opposite(f), m_now, true}); // (drawn sliding in)
    } else if (blockOf(h) == B::MovingPiston) {
        finishMoves(true); // (pulled back before it landed: land everything first)
        const BlockStateId landed = at(front);
        if (blockOf(landed) == B::PistonHead && facing6Of(landed) == f) setRaw(front, 0);
    }
    setRaw(p, withFlag(s, extended, false));
    // Sticky pistons pull the block in front of the head back (with what sticks to it).
    if (blockOf(s) == B::StickyPiston) {
        const BlockPos far = rel(p, f, 2);
        std::vector<BlockPos>& destroy = m_pushDestroy;
        if (m_world.isInHeight(far.y) && m_world.chunk(far.chunk()) && at(front) == 0 &&
            pushKind(at(far)) == Push::Move && gatherPush(p, far, opposite(f), destroy) &&
            m_moving.size() + m_push.size() <= m_moving.capacity()) {
            for (const BlockPos& d : destroy) {
                m_drops.push_back({d, {}, at(d)});
                setRaw(d, 0);
            }
            m_pushStates.clear();
            for (const BlockPos& q : m_push)
                m_pushStates.push_back(at(q));
            for (const BlockPos& q : m_push)
                setRaw(q, 0);
            const BlockStateId moving =
                R().set(R().defaultState(B::MovingPiston), facing6, static_cast<int>(opposite(f)));
            for (size_t i = 0; i < m_push.size(); ++i) {
                const BlockPos to = rel(m_push[i], opposite(f));
                setRaw(to, moving);
                if (m_moving.size() < m_moving.capacity())
                    m_moving.push_back({to, m_pushStates[i], opposite(f), m_now});
            }
            for (const BlockPos& q : m_push)
                notifyNeighbours(q);
        }
    }
    neighbourChanged(p);
    notifyNeighbours(p);
    neighbourChanged(front);
    notifyNeighbours(front);
}

void BlockUpdates::finishMoves(bool force) {
    // Blocks whose 2 ticks are up land in their cells, then everything around them
    // hears about it.
    size_t kept = 0;
    size_t landed = 0;
    BlockPos done[256];
    for (size_t i = 0; i < m_moving.size(); ++i) {
        const Moving& mv = m_moving[i];
        if (m_now - mv.start < 2 && !force) {
            m_moving[kept++] = mv;
            continue;
        }
        if (!mv.visual && blockOf(at(mv.to)) == B::MovingPiston) {
            if (blockOf(mv.state) == B::PistonHead) { // only while its piston is still out
                const BlockStateId base = at(rel(mv.to, opposite(facing6Of(mv.state))));
                if (!isPiston(blockOf(base)) || !flag(base, extended) || facing6Of(base) != facing6Of(mv.state)) {
                    setRaw(mv.to, 0);
                    if (landed < 256) done[landed++] = mv.to;
                    continue;
                }
            }
            setRaw(mv.to, mv.state);
            if (landed < 256) done[landed++] = mv.to;
            // A moved observer pulses after its delay; one that was on goes off (wiki).
            if (blockOf(mv.state) == B::Observer) {
                if (flag(mv.state, powered)) setRaw(mv.to, withFlag(mv.state, powered, false));
                else if (!hasTick(mv.to, B::Observer)) schedule(mv.to, B::Observer, 2, 0);
            }
        }
    }
    m_moving.resize(kept);
    for (size_t i = 0; i < landed; ++i) {
        neighbourChanged(done[i]);
        notifyNeighbours(done[i]);
    }
}

// --- Players ----------------------------------------------------------------------

bool BlockUpdates::usable(BlockStateId s) {
    const BlockId b = blockOf(s);
    return b == B::Lever || isButton(b) || b == B::Repeater || b == B::RedstoneWire || b == B::OakDoor ||
           b == B::OakTrapdoor || b == B::OakFenceGate || b == B::Comparator;
}

bool BlockUpdates::use(const BlockPos& p) {
    const BlockStateId s = at(p);
    switch (blockOf(s)) {
    case B::Lever:
        // Vanilla: pitch 0.6 switching on, 0.5 off.
        m_world.playSound(Sound::Click, p.x + 0.5, p.y + 0.5, p.z + 0.5, 1.0f, flag(s, powered) ? 0.5f / 0.6f : 1.0f);
        set(p, withFlag(s, powered, !flag(s, powered)));
        return true;
    case B::StoneButton:
    case B::OakButton:
        if (!flag(s, powered)) {
            m_world.playSound(blockOf(s) == B::StoneButton ? Sound::Click : Sound::WoodClick, p.x + 0.5, p.y + 0.5,
                              p.z + 0.5);
            set(p, withFlag(s, powered, true));
            // Pressed for 1 s (stone) or 1.5 s (wood) (wiki: Button).
            schedule(p, blockOf(s), blockOf(s) == B::StoneButton ? 20 : 30, 0);
        }
        return true;
    case B::Repeater:
        set(p, R().set(s, delay, (R().get(s, delay) + 1) % 4));
        return true;
    case B::OakDoor: { // wooden doors open by hand (iron ones only by redstone)
        const bool upper = R().get(s, doorHalf) == 0;
        const BlockPos lower = upper ? BlockPos{p.x, p.y - 1, p.z} : p;
        const BlockStateId ls = at(lower);
        if (!isDoor(blockOf(ls))) return false;
        setDoor(lower, ls, !flag(ls, open), flag(ls, powered));
        return true;
    }
    case B::OakTrapdoor:
    case B::OakFenceGate:
        m_world.playSound(flag(s, open) ? Sound::DoorClose : Sound::DoorOpen, p.x + 0.5, p.y + 0.5, p.z + 0.5);
        set(p, withFlag(s, open, !flag(s, open)));
        return true;
    case B::Comparator: { // compare <-> subtract
        const BlockStateId now = R().set(s, comparatorMode, 1 - R().get(s, comparatorMode));
        set(p, now);
        comparatorChanged(p, now);
        return true;
    }
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
    default:
        return false;
    }
}

std::optional<BlockStateId> BlockUpdates::placement(const World& world, BlockStateId state,
                                                    const BlockPos& at, Direction faceDir,
                                                    float yaw, float pitch, double hitY) {
    const auto& r = R();
    auto solid = [&](Direction d) { return supports(world.getBlock(rel(at, d))); };
    // The player's horizontal look direction (vanilla yaw: 0 south, 90 west).
    const float y = std::fmod(std::fmod(yaw, 360.0f) + 360.0f, 360.0f);
    static constexpr Direction kLook[4] = {Direction::South, Direction::West, Direction::North,
                                           Direction::East};
    const Direction look = kLook[static_cast<int>(std::floor((y + 45.0f) / 90.0f)) % 4];
    // Slabs and stairs go in the top half when put against a block's underside or
    // the upper half of its side (wiki: Slab, Stairs).
    const bool upper = faceDir == Direction::Down || (horizontal(faceDir) && hitY > 0.5);
    switch (r.kind(blockOf(state))) {
    case BlockKind::Slab: return r.set(state, slabType, upper ? 0 : 1);
    case BlockKind::Stairs: // facing the way the player looks: they walk up away from themselves
        return stairsShaped(world, at, r.set(withHFacing(state, look), slabHalf, upper ? 0 : 1));
    case BlockKind::Wall: return wallConnected(world, at, state);
    case BlockKind::Pane: return paneConnected(world, at, state);
    case BlockKind::Carpet:
        if (world.getBlock(rel(at, Direction::Down)) == 0) return std::nullopt;
        return state;
    case BlockKind::Sign: // on a side: the wall sign; on top: turned to face the player (wiki: Sign)
    case BlockKind::HangingSign: {
        const bool hangingSign = r.kind(blockOf(state)) == BlockKind::HangingSign;
        if (horizontal(faceDir)) {
            if (!solid(opposite(faceDir))) return std::nullopt;
            std::string id = r.block(R().blockOf(state)).id; // (the real block: blockOf follows `like`)
            id.insert(id.rfind(hangingSign ? "_hanging_sign" : "_sign"), "_wall");
            return withHFacing(r.defaultState(*r.findBlock(id)), faceDir);
        }
        if (hangingSign ? !solid(Direction::Up) : !solid(Direction::Down)) return std::nullopt;
        if (hangingSign && faceDir != Direction::Down) return std::nullopt;
        // 16 directions, 0 = facing south, toward the player (vanilla: yaw + 180).
        const int rotation = int(std::floor((yaw + 180.0f) * 16.0f / 360.0f + 0.5f)) & 15;
        return r.set(state, rotation16, rotation);
    }
    case BlockKind::WallSign:
    case BlockKind::WallHangingSign:
        return state;
    case BlockKind::Plain: break;
    }
    // Glazed terracotta faces the player (wiki: Glazed Terracotta); concrete powder put
    // by water hardens at once (wiki: Concrete Powder).
    if (R().block(R().blockOf(state)).id.ends_with("_glazed_terracotta")) return withHFacing(state, opposite(look));
    if (const auto concrete = concreteFor(state))
        for (const Direction d : {Direction::Up, Direction::North, Direction::South, Direction::West, Direction::East})
            if (blockOf(world.getBlock(rel(at, d))) == B::Water) return *concrete;
    switch (blockOf(state)) {
    case B::OakLeaves:
    case B::BirchLeaves:
    case B::SpruceLeaves:
    case B::AcaciaLeaves:
    case B::JungleLeaves:
    case B::DarkOakLeaves:
    case B::MangroveLeaves:
    case B::PaleOakLeaves:
    case B::CherryLeaves: {
        // Placed leaves are persistent (never decay; wiki: Leaves), with their distance.
        BlockStateId s = r.set(state, persistent, 0);
        int best = 7;
        for (int d = 0; d < kDirectionCount; ++d) {
            const BlockStateId n = world.getBlock(rel(at, static_cast<Direction>(d)));
            if (isLog(blockOf(n)))
                best = 1;
            else if (isLeaves(blockOf(n)))
                best = std::min(best, r.get(n, distance) + 2);
        }
        return r.set(s, distance, best - 1);
    }
    case B::OakSapling:
    case B::BirchSapling:
    case B::SpruceSapling:
    case B::AcaciaSapling:
    case B::JungleSapling:
    case B::DarkOakSapling:
    case B::CherrySapling:
    case B::PaleOakSapling:
    case B::MangrovePropagule:
        if (!plantableSoil(world.getBlock(rel(at, Direction::Down)))) return std::nullopt;
        return state;
    case B::ChorusPlant:
    case B::ChorusFlower:
        if (!chorusCanStay(world, at, blockOf(state))) return std::nullopt;
        return blockOf(state) == B::ChorusPlant ? chorusConnected(world, at, state) : state;
    case B::IronBars:
        return barsConnected(world, at, state);
    case B::OakDoor:
    case B::IronDoor: {
        // On solid ground with room above; it faces the way the player looks and hinges
        // on the left unless a door is already there (a double door; wiki: Door).
        if (!solid(Direction::Down) || !replaceable(world.getBlock(rel(at, Direction::Up)))) return std::nullopt;
        BlockStateId s = r.set(withHFacing(state, look), doorHalf, 1);
        static constexpr Direction kLeftOf[6] = {Direction::Down, Direction::Up, Direction::West,
                                                 Direction::East, Direction::South, Direction::North};
        const Direction leftDir = kLeftOf[int(look)];
        const BlockStateId left = world.getBlock(rel(at, leftDir));
        if (isDoor(blockOf(left)) && r.get(left, hinge) == 0) return r.set(s, hinge, 1); // a double door
        // Else the hinge goes to the side with more solid blocks beside the two halves.
        auto solidCount = [&](Direction d) {
            const BlockPos b = rel(at, d);
            return int(r.opaqueCube(world.getBlock(b))) + int(r.opaqueCube(world.getBlock(rel(b, Direction::Up))));
        };
        if (solidCount(opposite(leftDir)) > solidCount(leftDir)) s = r.set(s, hinge, 1);
        return s;
    }
    case B::OakTrapdoor:
    case B::IronTrapdoor: {
        // On a block's side it hangs from that side; clicked from below it sits at the
        // top of its cell (wiki: Trapdoor - ours: from a top face the bottom).
        const Direction f = horizontal(faceDir) ? faceDir : opposite(look);
        return r.set(withHFacing(state, f), slabHalf, faceDir == Direction::Down ? 0 : 1);
    }
    case B::OakFenceGate:
        return withHFacing(state, look);
    case B::Rail:
    case B::PoweredRail:
    case B::DetectorRail:
    case B::ActivatorRail:
        if (!solid(Direction::Down)) return std::nullopt;
        return withRailShape(state, chooseRailShape(world, at, state));
    case B::Hopper: // points into the block it was put against (down when put on top)
        if (horizontal(faceDir)) return r.set(state, hopperFacing, static_cast<int>(opposite(faceDir)) - 1);
        return r.set(state, hopperFacing, 0);
    case B::Dispenser:
    case B::Dropper: { // its front faces the player (wiki)
        const Direction f = pitch > 45.0f ? Direction::Up : pitch < -45.0f ? Direction::Down : opposite(look);
        return r.set(state, facing6, static_cast<int>(f));
    }
    case B::Comparator:
        if (!solid(Direction::Down)) return std::nullopt;
        return withHFacing(state, opposite(look)); // like a repeater: the output away from the player
    case B::Observer: { // its face looks where the player looks (wiki: Observer)
        const Direction f = pitch > 45.0f ? Direction::Down : pitch < -45.0f ? Direction::Up : look;
        return r.set(state, facing6, static_cast<int>(f));
    }
    case B::OakFence:
        return fenceConnected(world, at, state);
    case B::OakPressurePlate:
    case B::StonePressurePlate:
    case B::LightWeightedPressurePlate:
    case B::HeavyWeightedPressurePlate:
        if (!solid(Direction::Down)) return std::nullopt;
        return state;
    case B::EndRod: // points out of the face it was put on (wiki: End Rod)
        return r.set(state, facing6, static_cast<int>(faceDir));
    case B::RedBed: {
        // The foot where clicked, the head one block further in the player's look;
        // the head needs room (Java beds need no support; wiki: Bed).
        const BlockPos head = rel(at, look);
        if (!replaceable(world.getBlock(head))) return std::nullopt;
        return r.set(withHFacing(state, look), bedPart, 1);
    }
    case B::SugarCane:
        if (!sugarCaneCanStay(world, at)) return std::nullopt;
        return state;
    case B::Cactus:
        if (!cactusCanStay(world, at)) return std::nullopt;
        return state;
    case B::BrownMushroom:
    case B::RedMushroom:
        if (!mushroomCanStay(world, at)) return std::nullopt;
        return state;
    case B::CrimsonFungus:
    case B::WarpedFungus:
    case B::CrimsonRoots:
    case B::WarpedRoots:
    case B::NetherSprouts:
    case B::WeepingVines:
    case B::TwistingVines:
        if (!netherPlantCanStay(world, at, blockOf(state))) return std::nullopt;
        return state;
    case B::NetherWart: // only on soul sand (wiki: Nether Wart)
        if (blockOf(world.getBlock(rel(at, Direction::Down))) != B::SoulSand) return std::nullopt;
        return state;
    case B::Anvil:
    case B::ChippedAnvil:
    case B::DamagedAnvil:
        return withHFacing(state, look);
    case B::Chest: {
        // The front faces the player; next to a single chest with the same facing
        // (on its left or right) it becomes the other half of a double chest.
        BlockStateId s = withHFacing(state, opposite(look));
        const Direction cw = chestClockwise(opposite(look));
        for (const auto& [side, myType] :
             {std::pair{cw, 2}, std::pair{opposite(cw), 1}}) { // right, left
            const BlockStateId n = world.getBlock(rel(at, side));
            if (blockOf(n) == B::Chest && r.get(n, chestType) == 0 && hFacing(n) == opposite(look))
                return r.set(s, chestType, myType);
        }
        return s;
    }
    case B::Wheat:
    case B::Carrots:
    case B::Potatoes:
    case B::Beetroots: // planted on farmland only
        if (blockOf(world.getBlock(rel(at, Direction::Down))) != B::Farmland) return std::nullopt;
        return state;
    case B::RedstoneWire: {
        if (!solid(Direction::Down)) return std::nullopt;
        BlockStateId s = state; // placed as a cross; its neighbours shape it
        for (const Direction d : kHorizontal)
            s = r.set(s, wireProp(d), kSide);
        return s;
    }
    case B::Torch: // on a wall when clicked on its side, else standing (wiki: Torch)
    case B::SoulTorch:
        if (horizontal(faceDir) && solid(opposite(faceDir)))
            return withHFacing(r.defaultState(blockOf(state) == B::Torch ? B::WallTorch : B::SoulWallTorch), faceDir);
        if (solid(Direction::Down)) return state;
        return std::nullopt;
    case B::Bamboo: {
        const BlockStateId below = world.getBlock(rel(at, Direction::Down));
        const BlockId bb = blockOf(below);
        if (bb != B::Bamboo && !plantableSoil(below) && bb != B::Sand && bb != B::RedSand && bb != B::Gravel)
            return std::nullopt;
        return state;
    }
    case B::Campfire: // faces the player; over a hay bale its smoke goes higher (wiki: Campfire)
    case B::SoulCampfire:
        return r.set(withHFacing(state, opposite(look)), signalFire,
                     blockOf(world.getBlock(rel(at, Direction::Down))) == B::HayBlock ? 0 : 1);
    case B::Ladder: // only on a block's side, facing out from it (wiki: Ladder)
        if (!horizontal(faceDir) || !solid(opposite(faceDir))) return std::nullopt;
        return withHFacing(state, faceDir);
    case B::Lantern: // hangs when put under a block, else stands (wiki: Lantern)
    case B::SoulLantern: {
        const bool hang = faceDir == Direction::Down ? solid(Direction::Up) : !solid(Direction::Down) && solid(Direction::Up);
        if (!hang && !solid(Direction::Down)) return std::nullopt;
        return r.set(state, hanging, hang ? 0 : 1);
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
                      : a.x >= a.z             ? (v.x > 0 ? Direction::East : Direction::West)
                                               : (v.z > 0 ? Direction::South : Direction::North);
        return r.set(state, facing6, static_cast<int>(opposite(d)));
    }
    default:
        return state;
    }
}

} // namespace mc::world
