// Redstone 2 (M21): doors, trapdoors, fences, gates, pressure plates; block shapes.
#include "world/BlockShapes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

#include <ostream>

using namespace mc::world;
using namespace mc::world::properties;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }
std::string_view val(BlockStateId s, std::string_view p) { return *R().value(s, p); }

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
    void place(BlockId b, BlockPos p, Direction side = Direction::Up, float yaw = 180.0f) {
        redstone.setTime(time);
        const auto s = BlockUpdates::placement(world, S(b), p, side, yaw, 0.0f);
        REQUIRE(s);
        world.updateBlock(p, *s);
    }
    void put(BlockPos p, BlockStateId s) {
        redstone.setTime(time);
        world.updateBlock(p, s);
    }
    BlockStateId at(BlockPos p) const { return world.getBlock(p); }
    void tick(int n = 1) {
        for (int i = 0; i < n; ++i) {
            ++time;
            redstone.setTime(time);
            redstone.tick();
        }
    }
};

} // namespace

TEST_CASE("doors: placing brings the upper half; hand opens wooden doors only; redstone opens both kinds") {
    Scene s;
    s.place(blocks::OakDoor, {4, 64, 4});
    CHECK(R().blockOf(s.at({4, 65, 4})) == blocks::OakDoor);
    CHECK(val(s.at({4, 65, 4}), "half") == "upper");
    CHECK(val(s.at({4, 64, 4}), "facing") == "north"); // (yaw 180: looking north)
    s.redstone.setTime(s.time);
    CHECK(s.redstone.use({4, 65, 4})); // the upper half opens the whole door
    CHECK(val(s.at({4, 64, 4}), "open") == "true");
    CHECK(val(s.at({4, 65, 4}), "open") == "true");
    s.place(blocks::IronDoor, {8, 64, 4});
    CHECK_FALSE(s.redstone.use({8, 64, 4}));
    s.put({9, 64, 4}, S(blocks::RedstoneBlock));
    CHECK(val(s.at({8, 64, 4}), "open") == "true");
    CHECK(val(s.at({8, 65, 4}), "powered") == "true");
    s.put({9, 64, 4}, 0);
    CHECK(val(s.at({8, 64, 4}), "open") == "false");
    // Breaking the upper half takes the lower (which drops the door).
    s.put({4, 65, 4}, 0);
    CHECK(s.at({4, 64, 4}) == 0);
    CHECK(s.redstone.drops().size() == 1);
    // A second door to the right hinges the other way: a double door.
    s.place(blocks::OakDoor, {6, 64, 8});
    s.place(blocks::OakDoor, {7, 64, 8}); // (the first is on its left when facing north)
    CHECK(val(s.at({6, 64, 8}), "hinge") == "left");
    CHECK(val(s.at({7, 64, 8}), "hinge") == "right");
}

TEST_CASE("collision shapes: closed doors are thin panels, open gates let you through, fences are 1.5 tall") {
    const BlockStateId door = S(blocks::OakDoor); // facing north, closed
    const BlockShape& d = collisionShape(door);
    REQUIRE(d.count == 1);
    CHECK(d.boxes[0].to[2] - d.boxes[0].from[2] == 3); // 3/16 thick
    const BlockStateId gate = S(blocks::OakFenceGate);
    CHECK(collisionShape(gate).boxes[0].to[1] == 24);
    CHECK(collisionShape(*R().with(gate, "open", "true")).count == 0);
    CHECK(collisionShape(S(blocks::OakFence)).boxes[0].to[1] == 24);
    CHECK(collisionShape(S(blocks::Farmland)).boxes[0].to[1] == 15);
    CHECK(collisionShape(S(blocks::Stone)).count == 1);
    CHECK(collisionShape(0).count == 0);
}

TEST_CASE("fences join fences, gates and solid blocks; trapdoors and gates open by hand and by redstone") {
    Scene s;
    s.place(blocks::OakFence, {4, 64, 4});
    s.place(blocks::OakFence, {5, 64, 4});
    s.put({4, 64, 5}, S(blocks::Stone));
    CHECK(val(s.at({4, 64, 4}), "east") == "true");
    CHECK(val(s.at({4, 64, 4}), "south") == "true");
    CHECK(val(s.at({4, 64, 4}), "west") == "false");
    s.place(blocks::OakTrapdoor, {8, 64, 8});
    s.redstone.setTime(s.time);
    CHECK(s.redstone.use({8, 64, 8}));
    CHECK(val(s.at({8, 64, 8}), "open") == "true");
    s.place(blocks::OakFenceGate, {10, 64, 8});
    s.put({11, 64, 8}, S(blocks::RedstoneBlock));
    CHECK(val(s.at({10, 64, 8}), "open") == "true");
}

TEST_CASE("pressure plates: pressed while something stands on them, released after 20 ticks; weighted ones count") {
    Scene s;
    s.place(blocks::StonePressurePlate, {4, 64, 4});
    s.put({4, 64, 5}, S(blocks::RedstoneLamp));
    for (int i = 0; i < 30; ++i) { // someone stands on it for 30 ticks
        s.redstone.setTime(s.time);
        s.redstone.pressPlate({4, 64, 4}, false);
        s.redstone.settlePlates();
        s.tick();
    }
    CHECK(val(s.at({4, 64, 4}), "powered") == "true");
    CHECK(val(s.at({4, 64, 5}), "lit") == "true");
    s.tick(25); // gone: up within 20 ticks
    CHECK(val(s.at({4, 64, 4}), "powered") == "false");
    // Items don't press stone plates; a light weighted plate gives one per entity.
    s.redstone.pressPlate({4, 64, 4}, true);
    s.redstone.settlePlates();
    CHECK(val(s.at({4, 64, 4}), "powered") == "false");
    s.place(blocks::LightWeightedPressurePlate, {8, 64, 4});
    s.redstone.setTime(s.time);
    for (int i = 0; i < 5; ++i)
        s.redstone.pressPlate({8, 64, 4}, true);
    s.redstone.settlePlates();
    CHECK(R().get(s.at({8, 64, 4}), power) == 5);
}

#include "world/ChunkSerializer.h"
#include "world/Items.h"

TEST_CASE("comparators: pass the rear signal on, compare against or subtract the sides, read container fullness") {
    Scene s;
    // Facing north: input from the north (z 3), output south (z 5).
    s.place(blocks::Comparator, {4, 64, 4}, Direction::Up, 0.0f); // (yaw 0: looking south -> facing north)
    CHECK(val(s.at({4, 64, 4}), "facing") == "north");
    s.put({4, 64, 5}, S(blocks::RedstoneLamp));
    s.put({4, 64, 3}, S(blocks::RedstoneBlock)); // rear 15
    s.tick(3);
    CHECK(val(s.at({4, 64, 4}), "powered") == "true");
    CHECK(val(s.at({4, 64, 5}), "lit") == "true");
    // A side input of 15 in subtract mode: 15 - 15 = 0.
    s.put({5, 64, 4}, S(blocks::RedstoneBlock));
    s.redstone.setTime(s.time);
    s.redstone.use({4, 64, 4}); // -> subtract
    s.tick(3);
    CHECK(val(s.at({4, 64, 4}), "powered") == "false");
    // Compare mode: rear 15 >= side 15 -> 15 again.
    s.redstone.use({4, 64, 4});
    s.tick(3);
    CHECK(val(s.at({4, 64, 4}), "powered") == "true");

    // A chest behind: 1 stack of 64 in 27 slots -> floor(1 + 1/27 x 14) = 1.
    s.put({5, 64, 4}, 0);
    s.put({4, 64, 3}, S(blocks::Chest));
    s.tick(3);
    CHECK(val(s.at({4, 64, 4}), "powered") == "false"); // empty
    s.world.chunk({0, 0})->chest(4, 64, 3)->items[0] = ItemStack{*itemRegistry().find("stone"), 64};
    s.tick(3);
    CHECK(val(s.at({4, 64, 4}), "powered") == "true");
    CHECK(s.world.chunk({0, 0})->comparator(4, 64, 4)->output == 1);
    // OutputSignal saves.
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(*mc::nbt::read(mc::nbt::write(chunkToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}))))), back));
    REQUIRE(back.comparator(4, 64, 4));
    CHECK(back.comparator(4, 64, 4)->output == 1);
}

TEST_CASE("observers: a change in front of the face sends a 2-tick pulse out of the back") {
    Scene s;
    s.place(blocks::Observer, {4, 64, 4}, Direction::Up, 180.0f); // looking north: face north
    CHECK(val(s.at({4, 64, 4}), "facing") == "north");
    s.put({4, 64, 5}, S(blocks::RedstoneLamp)); // behind it (south)
    s.tick(3);
    CHECK(val(s.at({4, 64, 4}), "powered") == "false");
    s.put({4, 64, 3}, S(blocks::Stone)); // the watched block changes
    s.tick(2);
    CHECK(val(s.at({4, 64, 4}), "powered") == "true");
    CHECK(val(s.at({4, 64, 5}), "lit") == "true");
    s.tick(2);
    CHECK(val(s.at({4, 64, 4}), "powered") == "false");
}

TEST_CASE("a comparator still notices its chest filling after its chunk has been quiet for a while (regression)") {
    Scene s;
    s.place(blocks::Comparator, {4, 64, 4}, Direction::Up, 0.0f);
    s.put({4, 64, 3}, S(blocks::Chest));
    s.tick(100); // nothing scheduled: the chunk must stay ticking for the comparator
    s.world.chunk({0, 0})->chest(4, 64, 3)->items[0] = ItemStack{*itemRegistry().find("stone"), 64};
    s.tick(3);
    CHECK(val(s.at({4, 64, 4}), "powered") == "true");
}

#include "world/Rails.h"

TEST_CASE("rails: straight lines, corners, slopes up to a raised rail; powered rails carry power 8 along") {
    Scene s;
    for (int z = 0; z < 4; ++z)
        s.place(blocks::Rail, {4, 64, z});
    CHECK(val(s.at({4, 64, 1}), "shape") == "north_south");
    s.place(blocks::Rail, {5, 64, 4});
    s.place(blocks::Rail, {4, 64, 4}); // between (4,3) to the north and (5,4) to the east
    CHECK(val(s.at({4, 64, 4}), "shape") == "north_east");
    s.put({8, 64, 0}, S(blocks::Stone));
    s.place(blocks::Rail, {8, 65, 0});
    s.place(blocks::Rail, {8, 64, 1}); // the rail north of it is a block up
    CHECK(val(s.at({8, 64, 1}), "shape") == "ascending_north");
    // A line of 10 powered rails powered at one end: 9 light up, the 10th stays dark.
    for (int x = 0; x < 10; ++x)
        s.place(blocks::PoweredRail, {x, 64, 10}, Direction::Up, 90.0f);
    s.put({-1, 64, 10}, S(blocks::RedstoneBlock));
    CHECK(val(s.at({0, 64, 10}), "shape") == "east_west");
    CHECK(val(s.at({0, 64, 10}), "powered") == "true");
    CHECK(val(s.at({8, 64, 10}), "powered") == "true");
    CHECK(val(s.at({9, 64, 10}), "powered") == "false");
    // Detector rails act like plates for minecarts.
    s.place(blocks::DetectorRail, {12, 64, 12});
    s.redstone.setTime(s.time);
    s.redstone.pressPlate({12, 64, 12}, false); // a mob or player: no
    s.redstone.settlePlates();
    CHECK(val(s.at({12, 64, 12}), "powered") == "false");
    s.redstone.pressPlate({12, 64, 12}, false, true); // a minecart
    s.redstone.settlePlates();
    CHECK(val(s.at({12, 64, 12}), "powered") == "true");
}

TEST_CASE("pistons move blocks over 2 ticks; slime blocks drag what sticks to them along") {
    Scene s; // (up in the air: a slime block on the floor would drag the floor too)
    const BlockStateId piston = *R().with(S(blocks::StickyPiston), "facing", "east");
    s.put({0, 70, 0}, piston);
    s.put({1, 70, 0}, S(blocks::SlimeBlock));
    s.put({1, 70, 1}, S(blocks::Stone));      // stuck to the slime's side
    s.put({1, 71, 0}, S(blocks::Cobblestone)); // and on top
    s.put({0, 70, -1}, S(blocks::RedstoneBlock));
    s.tick(2); // the move starts...
    CHECK(R().blockOf(s.at({2, 70, 0})) == blocks::MovingPiston);
    CHECK(s.redstone.moving().size() >= 3);
    s.tick(2); // ...and lands
    CHECK(R().blockOf(s.at({2, 70, 0})) == blocks::SlimeBlock);
    CHECK(R().blockOf(s.at({2, 70, 1})) == blocks::Stone);
    CHECK(R().blockOf(s.at({2, 71, 0})) == blocks::Cobblestone);
    CHECK(s.at({1, 70, 1}) == 0);
    CHECK(R().blockOf(s.at({1, 70, 0})) == blocks::PistonHead);
    // Sticky retract pulls the whole slime structure back.
    s.put({0, 70, -1}, 0);
    s.tick(5);
    CHECK(R().blockOf(s.at({1, 70, 0})) == blocks::SlimeBlock);
    CHECK(R().blockOf(s.at({1, 70, 1})) == blocks::Stone);
    CHECK(R().blockOf(s.at({1, 71, 0})) == blocks::Cobblestone);
}
