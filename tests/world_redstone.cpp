// Redstone: dust, torches, repeaters, levers, buttons, lamps, pistons, block ticks
// (wiki: Redstone circuits, Redstone Dust, Redstone Torch, Redstone Repeater, Piston).
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/BlockUpdates.h"

#include <doctest/doctest.h>

#include <ostream> // doctest prints std::string_view values

using namespace mc::world;
using namespace mc::world::properties;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }
BlockStateId with(BlockStateId s, std::string_view p, std::string_view v) { return *R().with(s, p, v); }
std::string_view val(BlockStateId s, std::string_view p) { return *R().value(s, p); }

// A stone floor (top at y 63) over 3x3 chunks around the origin.
struct Scene {
    World world;
    BlockUpdates redstone{world};
    int64_t time = 0;
    Scene() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, S(blocks::Stone));
            }
    }
    // A player-style edit: neighbours react.
    void put(BlockPos p, BlockStateId s) {
        redstone.setTime(time);
        world.updateBlock(p, s);
    }
    BlockStateId at(BlockPos p) const { return world.getBlock(p); }
    void use(BlockPos p) {
        redstone.setTime(time);
        redstone.use(p);
    }
    void tick(int n = 1) {
        for (int i = 0; i < n; ++i) {
            ++time;
            redstone.setTime(time);
            redstone.tick();
        }
    }
    int power(BlockPos p) const { return R().get(at(p), properties::power); }
    bool on(BlockPos p, std::string_view prop = "lit") const { return val(at(p), prop) == "true"; }
};

BlockStateId floorLever() { return with(with(S(blocks::Lever), "face", "floor"), "facing", "north"); }
// Dust as players place it: a cross until its neighbours shape it.
BlockStateId wire() {
    BlockStateId s = S(blocks::RedstoneWire);
    for (const char* side : {"north", "east", "south", "west"})
        s = with(s, side, "side");
    return s;
}

} // namespace

TEST_CASE("a lever powers a line of dust that loses 1 per block") {
    Scene s;
    s.put({0, 64, 0}, floorLever());
    for (int x = 1; x <= 17; ++x)
        s.put({x, 64, 0}, S(blocks::RedstoneWire));
    CHECK(s.power({1, 64, 0}) == 0);
    s.use({0, 64, 0});
    for (int x = 1; x <= 15; ++x)
        CHECK(s.power({x, 64, 0}) == 16 - x);
    CHECK(s.power({16, 64, 0}) == 0);
    s.use({0, 64, 0});
    for (int x = 1; x <= 17; ++x)
        CHECK(s.power({x, 64, 0}) == 0);
}

TEST_CASE("dust shapes: lines, crosses, dots, climbing a block") {
    Scene s;
    s.put({0, 64, 0}, wire());
    CHECK(val(s.at({0, 64, 0}), "north") == "side"); // alone: a cross
    CHECK(val(s.at({0, 64, 0}), "east") == "side");
    s.use({0, 64, 0}); // right-click: a dot
    CHECK(val(s.at({0, 64, 0}), "north") == "none");
    CHECK(val(s.at({0, 64, 0}), "east") == "none");
    s.put({1, 64, 0}, wire()); // a neighbour: an east-west line
    CHECK(val(s.at({0, 64, 0}), "east") == "side");
    CHECK(val(s.at({0, 64, 0}), "west") == "side");
    CHECK(val(s.at({0, 64, 0}), "north") == "none");
    // Up a step: dust on a block next to dust.
    s.put({0, 64, 3}, wire());
    s.put({1, 64, 3}, S(blocks::Stone));
    s.put({1, 65, 3}, wire());
    CHECK(val(s.at({0, 64, 3}), "east") == "up");
    // A block on top cuts the climb.
    s.put({0, 65, 3}, S(blocks::Stone));
    CHECK(val(s.at({0, 64, 3}), "east") != "up");
}

TEST_CASE("dust powers the block it points into; that block powers a lamp but not other dust") {
    Scene s;
    s.put({0, 64, 0}, floorLever());
    s.put({1, 64, 0}, wire());
    s.put({2, 64, 0}, S(blocks::Stone)); // the dust points into this block
    s.put({3, 64, 0}, S(blocks::RedstoneLamp));
    s.put({2, 64, 1}, wire()); // dust beside the weakly powered block
    s.use({0, 64, 0});
    CHECK(s.on({3, 64, 0}));
    CHECK(s.power({2, 64, 1}) == 0);
    s.use({0, 64, 0});
    CHECK(s.on({3, 64, 0})); // lamps go out 4 ticks later
    s.tick(3);
    CHECK(s.on({3, 64, 0}));
    s.tick(1);
    CHECK_FALSE(s.on({3, 64, 0}));
}

TEST_CASE("a torch on a powered block turns off after 2 ticks and back on 2 ticks after") {
    Scene s;
    s.put({5, 64, 5}, S(blocks::Stone));
    s.put({4, 64, 5}, with(with(S(blocks::Lever), "face", "wall"), "facing", "west"));
    s.put({6, 64, 5}, with(S(blocks::RedstoneWallTorch), "facing", "east"));
    CHECK(s.on({6, 64, 5}));
    s.use({4, 64, 5});
    s.tick(1);
    CHECK(s.on({6, 64, 5}));
    s.tick(1);
    CHECK_FALSE(s.on({6, 64, 5}));
    s.use({4, 64, 5});
    s.tick(2);
    CHECK(s.on({6, 64, 5}));
}

TEST_CASE("a torch strongly powers the block above it, which powers dust beside it") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::RedstoneTorch));
    s.put({0, 65, 0}, S(blocks::Stone));
    CHECK(s.redstone.strongInto({0, 65, 0}) == 15);
    s.put({-1, 64, 0}, S(blocks::Stone));
    s.put({-1, 65, 0}, wire());
    CHECK(s.power({-1, 65, 0}) == 15);
}

TEST_CASE("redstone torches burn out after more than 8 turn-offs in 60 ticks") {
    Scene s;
    s.put({5, 64, 5}, S(blocks::Stone));
    s.put({4, 64, 5}, with(with(S(blocks::Lever), "face", "wall"), "facing", "west"));
    s.put({6, 64, 5}, with(S(blocks::RedstoneWallTorch), "facing", "east"));
    for (int i = 0; i < 20; ++i) {
        s.use({4, 64, 5});
        s.tick(3);
    }
    if (val(s.at({4, 64, 5}), "powered") == "true") s.use({4, 64, 5}); // lever off
    s.tick(20);
    CHECK_FALSE(s.on({6, 64, 5})); // burnt out
    s.tick(200);
    CHECK(s.on({6, 64, 5})); // relit after 160 ticks
}

TEST_CASE("repeaters delay by 2 ticks per step, extend short pulses, and lock from the side") {
    Scene s;
    // Lever -> repeater (delay 2: 4 ticks) facing east-out -> lamp.
    s.put({0, 64, 0}, floorLever());
    BlockStateId rep = with(with(S(blocks::Repeater), "facing", "west"), "delay", "2"); // input west
    s.put({1, 64, 0}, rep);
    s.put({2, 64, 0}, S(blocks::RedstoneLamp));
    s.use({0, 64, 0});
    s.tick(3);
    CHECK_FALSE(s.on({1, 64, 0}, "powered"));
    s.tick(1);
    CHECK(s.on({1, 64, 0}, "powered"));
    CHECK(s.on({2, 64, 0}));
    // A 1-tick pulse comes out 4 ticks long.
    s.use({0, 64, 0});
    s.tick(4);
    CHECK_FALSE(s.on({1, 64, 0}, "powered"));
    s.use({0, 64, 0});
    s.tick(1);
    s.use({0, 64, 0});
    s.tick(3);
    CHECK(s.on({1, 64, 0}, "powered"));
    s.tick(3);
    CHECK(s.on({1, 64, 0}, "powered"));
    s.tick(1);
    CHECK_FALSE(s.on({1, 64, 0}, "powered"));
    // Locking: a powered repeater pointing into its side.
    s.put({1, 64, 2}, S(blocks::RedstoneBlock));
    s.put({1, 64, 1}, with(S(blocks::Repeater), "facing", "south")); // input south, output north
    s.tick(2);
    CHECK(s.on({1, 64, 1}, "powered"));
    CHECK(s.on({1, 64, 0}, "locked"));
    s.use({0, 64, 0}); // input on, but locked: stays off
    s.tick(6);
    CHECK_FALSE(s.on({1, 64, 0}, "powered"));
}

TEST_CASE("buttons stay pressed 20 ticks (stone) or 30 (wood)") {
    Scene s;
    s.put({0, 64, 0}, with(S(blocks::StoneButton), "face", "floor"));
    s.put({1, 64, 0}, with(S(blocks::OakButton), "face", "floor"));
    s.use({0, 64, 0});
    s.use({1, 64, 0});
    s.tick(19);
    CHECK(s.on({0, 64, 0}, "powered"));
    s.tick(1);
    CHECK_FALSE(s.on({0, 64, 0}, "powered"));
    CHECK(s.on({1, 64, 0}, "powered"));
    s.tick(10);
    CHECK_FALSE(s.on({1, 64, 0}, "powered"));
}

TEST_CASE("components pop off when their support goes, dropping themselves") {
    Scene s;
    s.put({0, 64, 0}, wire());
    s.put({3, 64, 3}, S(blocks::Stone));
    s.put({3, 64, 4}, with(S(blocks::RedstoneWallTorch), "facing", "south"));
    s.put({0, 63, 0}, 0);
    CHECK(s.at({0, 64, 0}) == 0);
    s.put({3, 64, 3}, 0);
    CHECK(s.at({3, 64, 4}) == 0);
    REQUIRE(s.redstone.drops().size() == 2);
    CHECK(blockRegistry().blockOf(s.redstone.drops()[0].loot) == blocks::RedstoneWire); // loot: redstone
    CHECK(blockRegistry().blockOf(s.redstone.drops()[1].loot) == blocks::RedstoneWallTorch); // loot: a torch
}

TEST_CASE("pistons push up to 12 blocks, break dust, stop at bedrock; sticky pistons pull") {
    Scene s;
    const BlockStateId piston = with(S(blocks::Piston), "facing", "east");
    s.put({0, 64, 0}, piston);
    for (int x = 1; x <= 12; ++x)
        s.put({x, 64, 0}, S(blocks::Cobblestone));
    s.put({13, 64, 0}, wire());
    s.put({0, 64, 1}, S(blocks::RedstoneBlock));
    s.tick(2); // player-powered pistons move a tick later
    CHECK(s.on({0, 64, 0}, "extended"));
    CHECK(R().blockOf(s.at({1, 64, 0})) == blocks::PistonHead);
    CHECK(R().blockOf(s.at({13, 64, 0})) == blocks::Cobblestone);
    CHECK(R().blockOf(s.at({2, 64, 0})) == blocks::Cobblestone);
    CHECK_FALSE(s.redstone.drops().empty()); // the dust
    s.put({0, 64, 1}, 0);
    s.tick(2); // player-powered pistons move a tick later
    CHECK_FALSE(s.on({0, 64, 0}, "extended"));
    CHECK(s.at({1, 64, 0}) == 0); // normal pistons leave the blocks
    // 13 blocks: too many.
    s.put({1, 64, 0}, S(blocks::Cobblestone));
    s.put({0, 64, 1}, S(blocks::RedstoneBlock));
    s.tick(2); // player-powered pistons move a tick later
    CHECK_FALSE(s.on({0, 64, 0}, "extended"));
    // Bedrock doesn't move.
    s.put({0, 64, 1}, 0);
    const BlockStateId sticky = with(S(blocks::StickyPiston), "facing", "north");
    s.put({5, 64, 5}, sticky);
    s.put({5, 64, 4}, S(blocks::Bedrock));
    s.put({6, 64, 5}, S(blocks::RedstoneBlock));
    s.tick(2); // player-powered pistons move a tick later
    CHECK_FALSE(s.on({5, 64, 5}, "extended"));
    s.put({5, 64, 4}, S(blocks::Dirt));
    s.tick(2); // player-powered pistons move a tick later
    CHECK(s.on({5, 64, 5}, "extended"));
    CHECK(R().blockOf(s.at({5, 64, 3})) == blocks::Dirt);
    s.put({6, 64, 5}, 0);
    s.tick(2); // player-powered pistons move a tick later
    CHECK(R().blockOf(s.at({5, 64, 4})) == blocks::Dirt); // pulled back
    CHECK(s.at({5, 64, 3}) == 0);
}

TEST_CASE("pistons: quasi-connectivity needs an update; base and head break together") {
    Scene s;
    s.put({0, 64, 0}, with(S(blocks::Piston), "facing", "up"));
    s.put({1, 65, 0}, S(blocks::RedstoneBlock)); // powers the space above the piston only
    s.tick(2); // player-powered pistons move a tick later
    CHECK_FALSE(s.on({0, 64, 0}, "extended")); // not updated: a BUD
    s.put({-1, 64, 0}, S(blocks::Stone)); // any update next to it
    s.tick(2); // player-powered pistons move a tick later
    CHECK(s.on({0, 64, 0}, "extended"));
    s.redstone.drops().clear();
    s.put({0, 65, 0}, 0); // break the head (survival): the base goes too
    CHECK(s.at({0, 64, 0}) == 0);
    REQUIRE(s.redstone.drops().size() == 1);
    CHECK(itemRegistry().item(s.redstone.drops()[0].stack.item).id == "minecraft:piston");
}

TEST_CASE("placement: wall torches, repeater and piston facings, support") {
    World w;
    Chunk& c = w.createChunk({0, 0});
    c.set(5, 64, 5, S(blocks::Stone));
    c.set(5, 63, 6, S(blocks::Stone));
    // Clicking the east face of the stone: a wall torch facing east.
    auto t = BlockUpdates::placement(w, S(blocks::RedstoneTorch), {6, 64, 5}, Direction::East, 0, 0);
    REQUIRE(t);
    CHECK(R().blockOf(*t) == blocks::RedstoneWallTorch);
    CHECK(val(*t, "facing") == "east");
    CHECK_FALSE(BlockUpdates::placement(w, S(blocks::RedstoneWire), {9, 64, 9}, Direction::Up, 0, 0));
    // Looking north (yaw 180): the repeater outputs north, so it faces (inputs) south.
    auto r = BlockUpdates::placement(w, S(blocks::Repeater), {5, 64, 6}, Direction::Up, 180.0f, 30.0f);
    REQUIRE(r);
    CHECK(val(*r, "facing") == "south");
    // Pistons face the player: looking down, it faces up.
    auto p = BlockUpdates::placement(w, S(blocks::Piston), {5, 64, 6}, Direction::Up, 0.0f, 80.0f);
    REQUIRE(p);
    CHECK(val(*p, "facing") == "up");
}

TEST_CASE("scheduled ticks save as delays and resume after loading") {
    Scene s;
    s.put({0, 64, 0}, floorLever());
    s.put({1, 64, 0}, with(with(S(blocks::Repeater), "facing", "west"), "delay", "4"));
    s.use({0, 64, 0});
    s.tick(3); // 5 ticks to go
    Chunk& c = *s.world.chunk({0, 0});
    REQUIRE(c.blockTicks().size() == 1);
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c, s.time));
    const auto* t = nbt.list("block_ticks")->items[0].get<mc::nbt::Compound>();
    CHECK(*t->string("i") == "minecraft:repeater");
    CHECK(t->integer("t") == 5);
    CHECK(t->integer("p") == -1);
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(nbt, d));
    CHECK(d.ticksRelative);
    REQUIRE(d.blockTicks().size() == 1);
    CHECK(d.blockTicks()[0].time == 5);
}

TEST_CASE("ticks loaded from disk fire on time, and new ticks in a loaded chunk aren't late") {
    // Regression: loaded chunks kept their "relative times" flag forever, so ticks
    // scheduled later were pushed back by the game time.
    Scene s;
    s.time = 24000;
    s.put({0, 64, 0}, floorLever());
    s.put({1, 64, 0}, with(with(S(blocks::Repeater), "facing", "west"), "delay", "2"));
    s.use({0, 64, 0}); // repeater on in 4 ticks
    s.tick(1);
    auto nbt = chunkToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), s.time));
    auto loaded = std::make_unique<Chunk>(ChunkPos{0, 0});
    REQUIRE(chunkFromNbt(nbt, *loaded));
    s.world.insertChunk(std::move(loaded));
    s.tick(2);
    CHECK_FALSE(s.on({1, 64, 0}, "powered"));
    s.tick(1);
    CHECK(s.on({1, 64, 0}, "powered"));
    // A chunk loaded with no pending ticks: a new tick runs at now + delay.
    nbt = chunkToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), s.time));
    loaded = std::make_unique<Chunk>(ChunkPos{0, 0});
    REQUIRE(chunkFromNbt(nbt, *loaded));
    CHECK_FALSE(loaded->ticksRelative);
    s.world.insertChunk(std::move(loaded));
    s.use({0, 64, 0}); // off in 4 ticks
    s.tick(4);
    CHECK_FALSE(s.on({1, 64, 0}, "powered"));
}

TEST_CASE("scheduling a tick marks the chunk for saving; duplicate saved ticks load once") {
    Scene s;
    s.put({0, 64, 0}, with(S(blocks::StoneButton), "face", "floor"));
    s.world.chunk({0, 0})->clearDirty();
    s.use({0, 64, 0});
    CHECK(s.world.chunk({0, 0})->dirty());
    auto nbt = chunkToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), s.time));
    auto& list = const_cast<mc::nbt::List&>(*nbt.list("block_ticks"));
    REQUIRE(list.items.size() == 1);
    list.items.push_back(list.items[0]);
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(nbt, d));
    CHECK(d.blockTicks().size() == 1);
}

TEST_CASE("a piston breaking a lever turns off what the lever powered through its block") {
    Scene s;
    s.put({5, 64, 5}, S(blocks::Stone));
    s.put({5, 64, 4}, with(with(S(blocks::Lever), "face", "wall"), "facing", "north")); // on the stone's north
    s.put({5, 64, 6}, S(blocks::RedstoneLamp));                                         // the stone's south
    s.use({5, 64, 4});
    CHECK(s.on({5, 64, 6}));
    s.put({5, 64, 3}, with(S(blocks::Piston), "facing", "south")); // pushes into the lever's cell
    s.put({4, 64, 3}, S(blocks::RedstoneBlock));
    s.tick(2); // player-powered pistons move a tick later
    CHECK(s.at({5, 64, 4}) != S(blocks::Lever));
    s.tick(5);
    CHECK_FALSE(s.on({5, 64, 6}));
}

TEST_CASE("pistons push across chunk borders at negative coordinates, not into unloaded chunks or past the top") {
    Scene s;
    s.put({2, 64, -1}, with(S(blocks::Piston), "facing", "west"));
    s.put({1, 64, -1}, S(blocks::Cobblestone));
    s.put({0, 64, -1}, S(blocks::Cobblestone)); // chunk 0 / -1 border is between 0 and -1
    s.put({2, 64, -2}, S(blocks::RedstoneBlock));
    s.tick(2); // player-powered pistons move a tick later
    CHECK(s.on({2, 64, -1}, "extended"));
    CHECK(R().blockOf(s.at({-1, 64, -1})) == blocks::Cobblestone);
    // The 3x3 scene ends at x = -16: pushing into x -17 (unloaded) fails.
    s.put({-14, 64, 3}, with(S(blocks::Piston), "facing", "west"));
    s.put({-15, 64, 3}, S(blocks::Cobblestone));
    s.put({-16, 64, 3}, S(blocks::Cobblestone));
    s.put({-14, 64, 4}, S(blocks::RedstoneBlock));
    s.tick(2); // player-powered pistons move a tick later
    CHECK_FALSE(s.on({-14, 64, 3}, "extended"));
    // Up at the build limit.
    s.put({3, 318, 3}, with(S(blocks::Piston), "facing", "up"));
    s.put({3, 319, 3}, S(blocks::Cobblestone));
    s.put({4, 318, 3}, S(blocks::RedstoneBlock));
    s.tick(2); // player-powered pistons move a tick later
    CHECK_FALSE(s.on({3, 318, 3}, "extended"));
}

TEST_CASE("player-powered pistons start a tick later; in-line repeaters schedule with priority -3") {
    Scene s;
    s.put({0, 64, 0}, with(S(blocks::Piston), "facing", "east"));
    s.put({0, 64, 1}, S(blocks::RedstoneBlock));
    s.tick(1);
    CHECK_FALSE(s.on({0, 64, 0}, "extended"));
    s.tick(1);
    CHECK(s.on({0, 64, 0}, "extended"));
    // Two repeaters in a row (both inputs west): the first faces the second's back.
    s.put({5, 64, 5}, with(S(blocks::Repeater), "facing", "west"));
    s.put({6, 64, 5}, with(S(blocks::Repeater), "facing", "west"));
    s.put({4, 64, 5}, S(blocks::RedstoneBlock));
    const auto& ticks = s.world.chunk({0, 0})->blockTicks();
    REQUIRE_FALSE(ticks.empty());
    CHECK(ticks.back().priority == -3);
}

TEST_CASE("dust can't stand on leaves; pistons break leaves and can't move obsidian") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::OakLeaves));
    CHECK_FALSE(BlockUpdates::placement(s.world, S(blocks::RedstoneWire), {0, 65, 0}, Direction::Up, 0, 0));
    s.put({3, 64, 3}, with(S(blocks::Piston), "facing", "east"));
    s.put({4, 64, 3}, S(blocks::OakLeaves));
    s.put({3, 64, 4}, S(blocks::RedstoneBlock));
    s.tick(2);
    CHECK(s.on({3, 64, 3}, "extended"));
    CHECK(R().blockOf(s.at({5, 64, 3})) != blocks::OakLeaves); // broken, not pushed
    s.put({3, 64, 7}, with(S(blocks::Piston), "facing", "east"));
    s.put({4, 64, 7}, S(blocks::Obsidian));
    s.put({3, 64, 8}, S(blocks::RedstoneBlock));
    s.tick(2);
    CHECK_FALSE(s.on({3, 64, 7}, "extended"));
}

TEST_CASE("a torch survives 8 turn-offs in 60 ticks and burns out on the 9th") {
    for (int offs : {8, 9}) {
        Scene s;
        s.put({5, 64, 5}, S(blocks::Stone));
        s.put({4, 64, 5}, with(with(S(blocks::Lever), "face", "wall"), "facing", "west"));
        s.put({6, 64, 5}, with(S(blocks::RedstoneWallTorch), "facing", "east"));
        for (int i = 0; i < offs; ++i) { // lever on (torch off), lever off (torch on)
            s.use({4, 64, 5});
            s.tick(3);
            s.use({4, 64, 5});
            s.tick(3);
        }
        CHECK(s.on({6, 64, 5}) == (offs == 8)); // lit again unless burnt out
    }
}

TEST_CASE("pistons destroy saplings, fire and leaves; the drop is the block's loot") {
    Scene s;
    for (const BlockId b : {blocks::OakSapling, blocks::Fire, blocks::OakLeaves}) {
        s.put({0, 64, 4}, 0);
        s.put({0, 64, 3}, with(S(blocks::Piston), "facing", "east"));
        if (b == blocks::Fire) s.world.setBlock({1, 64, 3}, BlockUpdates::fireState(0));
        else s.world.setBlock({1, 64, 3}, S(b));
        s.redstone.drops().clear();
        s.put({0, 64, 4}, S(blocks::RedstoneBlock));
        s.tick(2);
        CHECK(R().blockOf(s.at({1, 64, 3})) == blocks::PistonHead);
        CHECK(R().blockOf(s.at({2, 64, 3})) != b); // not pushed along
        REQUIRE(s.redstone.drops().size() == 1);
        CHECK(R().blockOf(s.redstone.drops()[0].loot) == b); // rolled as loot (leaves: no leaves item)
        s.put({0, 64, 4}, 0);
        s.tick(2);
        s.put({0, 64, 3}, 0);
        s.put({1, 64, 3}, 0);
    }
}
