// Building blocks (M23.1): slabs, stairs, walls (wiki: Slab, Stairs, Wall).
#include "gameplay/Mining.h"
#include "gameplay/Recipes.h"
#include "world/BlockShapes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Items.h"
#include "world/Raycast.h"
#include "world/Sounds.h"

#include <doctest/doctest.h>

#include <array>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(std::string_view text) { return *R().parse(text); }
BlockId B(std::string_view id) { return *R().findBlock(id); }

struct Scene {
    World world;
    BlockUpdates updates{world};
    Scene() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, R().defaultState(blocks::Stone));
            }
    }
    BlockStateId at(BlockPos p) const { return world.getBlock(p); }
};

} // namespace

TEST_CASE("building: families register with their base block and kind") {
    const BlockId stairs = B("stone_brick_stairs"), slab = B("oak_slab"), wall = B("cobblestone_wall");
    CHECK(R().kind(stairs) == BlockKind::Stairs);
    CHECK(R().block(stairs).settings.base == blocks::StoneBricks);
    CHECK(R().block(stairs).stateCount == 4 * 2 * 5); // facing x half x shape
    CHECK(R().block(slab).stateCount == 3);
    CHECK(R().block(wall).stateCount == 2 * 3 * 3 * 3 * 3); // up x 4 sides
    CHECK(R().toString(R().defaultState(slab)) == "minecraft:oak_slab[type=bottom]");
    CHECK(R().toString(R().defaultState(wall)) == "minecraft:cobblestone_wall[east=none,north=none,south=none,up=true,west=none]");
    CHECK(R().findBlock("deepslate_tile_wall"));
    CHECK(R().findBlock("polished_tuff_stairs"));
    CHECK_FALSE(R().findBlock("smooth_stone_stairs")); // (vanilla has the slab only)
    // Only a double slab is a full, light-blocking block.
    CHECK(R().opaqueCube(S("stone_slab[type=double]")));
    CHECK_FALSE(R().opaqueCube(S("stone_slab[type=top]")));
    CHECK(R().lightOpacity(S("stone_slab[type=double]")) == 15);
}

TEST_CASE("building: shapes - slab halves, stair steps and corners, 1.5-tall walls") {
    const BlockShape& bottom = collisionShape(S("stone_slab[type=bottom]"));
    REQUIRE(bottom.count == 1);
    CHECK(bottom.boxes[0].to[1] == 8);
    CHECK(collisionShape(S("stone_slab[type=top]")).boxes[0].from[1] == 8);
    CHECK(collisionShape(S("stone_slab[type=double]")).count == 0); // a full cube
    // Straight stairs facing north: the half slab and the north half raised.
    const BlockShape st = collisionShape(S("stone_stairs[facing=north,half=bottom,shape=straight]"));
    REQUIRE(st.count == 3);
    CHECK(st.boxes[1].from[2] == 0); // quarters on the north side (z 0..8)
    CHECK(st.boxes[2].from[2] == 0);
    CHECK(collisionShape(S("stone_stairs[facing=north,shape=outer_left]")).count == 2);
    CHECK(collisionShape(S("stone_stairs[facing=north,shape=inner_right]")).count == 4);
    const BlockShape upside = collisionShape(S("stone_stairs[facing=east,half=top,shape=straight]"));
    CHECK(upside.boxes[0].from[1] == 8); // the slab part on top
    CHECK(collisionShape(S("cobblestone_wall[up=true]")).boxes[0].to[1] == 24);
}

TEST_CASE("building: slabs go in the upper half from a ceiling or a side's top half") {
    Scene s;
    const BlockStateId slab = R().defaultState(B("oak_slab"));
    CHECK(*BlockUpdates::placement(s.world, slab, {0, 64, 0}, Direction::Up, 0, 0) == S("oak_slab[type=bottom]"));
    CHECK(*BlockUpdates::placement(s.world, slab, {0, 64, 0}, Direction::Down, 0, 0) == S("oak_slab[type=top]"));
    CHECK(*BlockUpdates::placement(s.world, slab, {0, 64, 0}, Direction::North, 0, 0, 0.8) == S("oak_slab[type=top]"));
    CHECK(*BlockUpdates::placement(s.world, slab, {0, 64, 0}, Direction::North, 0, 0, 0.2) == S("oak_slab[type=bottom]"));
}

TEST_CASE("building: stairs face the player and form corners with stairs at right angles") {
    Scene s;
    const BlockStateId stairs = R().defaultState(B("stone_stairs"));
    // Yaw 180 looks north: the stair faces north (tall side away from the player).
    const BlockStateId placed = *BlockUpdates::placement(s.world, stairs, {0, 64, 0}, Direction::Up, 180.0f, 0);
    CHECK(R().value(placed, "facing") == "north");
    CHECK(R().value(placed, "half") == "bottom");
    // A stair facing north with one facing west in front (north of it): an outer corner.
    s.world.updateBlock({0, 64, -1}, S("stone_stairs[facing=west]"));
    s.world.updateBlock({0, 64, 0}, S("stone_stairs[facing=north]"));
    CHECK(R().value(s.at({0, 64, 0}), "shape") == "outer_left");
    // Behind it instead (south): an inner corner, updated when the neighbour arrives.
    s.world.updateBlock({3, 64, 0}, S("stone_stairs[facing=north]"));
    s.world.updateBlock({3, 64, 1}, S("stone_stairs[facing=east]"));
    CHECK(R().value(s.at({3, 64, 0}), "shape") == "inner_right");
    // Different halves don't join.
    s.world.updateBlock({6, 64, 0}, S("stone_stairs[facing=north,half=top]"));
    s.world.updateBlock({6, 64, -1}, S("stone_stairs[facing=west]"));
    CHECK(R().value(s.at({6, 64, 0}), "shape") == "straight");
}

TEST_CASE("building: walls connect, lose the post on straight runs and grow tall under blocks") {
    Scene s;
    const BlockStateId wall = R().defaultState(B("cobblestone_wall"));
    for (int x = 0; x < 3; ++x)
        s.world.updateBlock({x, 64, 0}, wall);
    const BlockStateId middle = s.at({1, 64, 0});
    CHECK(R().value(middle, "east") == "low");
    CHECK(R().value(middle, "west") == "low");
    CHECK(R().value(middle, "up") == "false"); // a straight run
    CHECK(R().value(s.at({0, 64, 0}), "up") == "true"); // the end keeps its post
    s.world.updateBlock({1, 65, 0}, R().defaultState(blocks::Stone));
    CHECK(R().value(s.at({1, 64, 0}), "east") == "tall");
    // Next to a full block it connects to it.
    s.world.updateBlock({5, 64, 0}, R().defaultState(blocks::Stone));
    s.world.updateBlock({5, 64, 1}, wall);
    CHECK(R().value(s.at({5, 64, 1}), "north") == "low");
}

TEST_CASE("building: drops, harvesting, recipes and sounds follow the base block") {
    Xoroshiro rng(1);
    std::vector<ItemStack> drops;
    blockDrops(S("stone_slab[type=double]"), {*itemRegistry().find("iron_pickaxe"), 1}, rng, drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].count == 2); // two slabs
    CHECK(drops[0].item == *itemRegistry().find("stone_slab"));
    // Stone slabs need a pickaxe (like stone); oak slabs drop by hand and are axe-fast.
    CHECK_FALSE(canHarvest(S("stone_slab"), {}));
    CHECK(canHarvest(S("oak_slab"), {}));
    CHECK(harvestInfo(B("oak_stairs")).tool == ToolType::Axe);
    // 6 planks in a staircase pattern make 4 stairs.
    const ItemId planks = *itemRegistry().find("oak_planks");
    std::array<ItemStack, 9> grid{};
    for (const int i : {0, 3, 4, 6, 7, 8})
        grid[size_t(i)] = {planks, 1};
    const auto out = craft(grid, 3);
    REQUIRE(out);
    CHECK(out->item == *itemRegistry().find("oak_stairs"));
    CHECK(out->count == 4);
    // Deepslate drops cobbled deepslate now that it exists (wiki: Deepslate).
    drops.clear();
    blockDrops(R().defaultState(blocks::Deepslate), {*itemRegistry().find("iron_pickaxe"), 1}, rng, drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].item == *itemRegistry().find("cobbled_deepslate"));
    CHECK(soundTypeOf(S("oak_stairs")) == SoundType::Wood);
    CHECK(soundTypeOf(S("stone_brick_wall")) == SoundType::Stone);
}

TEST_CASE("building: rays hit a slab's top half-way up and pass over its empty half") {
    Scene s;
    s.world.updateBlock({0, 64, 0}, S("stone_slab[type=bottom]"));
    const auto hit = raycastBlocks(s.world, {0.5, 66.0, 0.5}, {0, -1, 0}, 5.0);
    REQUIRE(hit);
    CHECK(hit->block == BlockPos{0, 64, 0});
    CHECK(hit->face == Direction::Up);
    CHECK(hit->distance == doctest::Approx(1.5));
    // Level through the empty top half: no hit on the slab (the floor beyond).
    const auto past = raycastBlocks(s.world, {-1.5, 64.75, 0.5}, {1, 0, 0}, 3.0);
    CHECK_FALSE(past);
}
