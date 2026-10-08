// Building blocks (M23.1): slabs, stairs, walls (wiki: Slab, Stairs, Wall).
#include "gameplay/Furnace.h"
#include "gameplay/Mining.h"
#include "gameplay/Recipes.h"
#include "world/BlockShapes.h"
#include "world/BlockUpdates.h"
#include "world/ChunkSerializer.h"
#include "world/Blocks.h"
#include "world/ItemContainers.h"
#include "world/Items.h"
#include "world/Potions.h"
#include "world/Raycast.h"
#include "world/Sounds.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdio>

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
    CHECK(collisionShape(S("stone_slab[type=double]")).count == 1); // a full cube (count 0 would be no collision)
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

TEST_CASE("wood sets: every wood's doors, fences and buttons behave like oak's (M23.3)") {
    Scene s;
    const BlockStateId door = R().defaultState(B("birch_door"));
    REQUIRE(R().likeOf(B("birch_door")) == blocks::OakDoor);
    s.world.updateBlock({0, 64, 0}, door);
    s.world.updateBlock({0, 65, 0}, R().set(door, properties::doorHalf, 0));
    CHECK(s.updates.use({0, 64, 0})); // wooden: opens by hand
    CHECK(R().value(s.at({0, 64, 0}), "open") == "true");
    CHECK(R().blockOf(s.at({0, 64, 0})) == B("birch_door")); // still birch
    // Fences of different woods join; crimson doesn't burn (wiki: Fire).
    s.world.updateBlock({3, 64, 0}, R().defaultState(blocks::OakFence));
    s.world.updateBlock({4, 64, 0}, R().defaultState(B("crimson_fence")));
    CHECK(R().value(s.at({4, 64, 0}), "west") == "true");
    CHECK(BlockUpdates::igniteOdds(B("crimson_fence")) == 0);
    CHECK(BlockUpdates::igniteOdds(B("birch_fence")) == BlockUpdates::igniteOdds(blocks::OakFence));
    CHECK(collisionShape(R().defaultState(B("jungle_fence_gate"))).count ==
          collisionShape(R().defaultState(blocks::OakFenceGate)).count);
    // Each wood's planks make its own door.
    const ItemId planks = *itemRegistry().find("spruce_planks");
    std::array<ItemStack, 9> grid{};
    for (const int i : {0, 1, 3, 4, 6, 7})
        grid[size_t(i)] = {planks, 1};
    const auto out = craft(grid, 3);
    REQUIRE(out);
    CHECK(out->item == *itemRegistry().find("spruce_door"));
}

TEST_CASE("new woods: axes strip logs, wood and stems keeping their axis (M23.3b)") {
    Scene s;
    s.world.updateBlock({0, 64, 0}, S("mangrove_log[axis=x]"));
    CHECK(BlockUpdates::strip(s.world, {0, 64, 0}));
    CHECK(R().toString(s.at({0, 64, 0})) == "minecraft:stripped_mangrove_log[axis=x]");
    CHECK_FALSE(BlockUpdates::strip(s.world, {0, 64, 0})); // already stripped
    s.world.updateBlock({1, 64, 0}, S("crimson_stem"));
    CHECK(BlockUpdates::strip(s.world, {1, 64, 0}));
    CHECK(R().blockOf(s.at({1, 64, 0})) == B("stripped_crimson_stem"));
    s.world.updateBlock({2, 64, 0}, S("stone"));
    CHECK_FALSE(BlockUpdates::strip(s.world, {2, 64, 0}));
    // Every log, wood and stripped log counts as a log for leaves (vanilla #logs).
    CHECK(BlockUpdates::isLog(B("stripped_oak_wood")));
    CHECK(BlockUpdates::isLog(B("warped_hyphae")));
    CHECK_FALSE(BlockUpdates::isLog(blocks::MushroomStem));
    CHECK(BlockUpdates::isLeaves(blocks::PaleOakLeaves));
}

TEST_CASE("new woods: pale oak grows from four saplings, bamboo grows and cuts by sword") {
    Scene s;
    s.world.forEachChunk([](Chunk& c) {
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                c.set(x, 63, z, R().defaultState(blocks::GrassBlock));
    });
    for (int k = 0; k < 4; ++k)
        s.world.setBlock({k & 1, 64, k >> 1}, R().defaultState(blocks::PaleOakSapling));
    for (int i = 0; i < 40 && R().blockOf(s.at({0, 64, 0})) == blocks::PaleOakSapling; ++i)
        s.updates.boneMeal({0, 64, 0}); // (bone meal: 45% a use to advance; wiki: Bone Meal)
    CHECK(s.at({0, 64, 0}) == R().defaultState(blocks::PaleOakLog)); // a 2x2 trunk
    CHECK(s.at({1, 65, 1}) == R().defaultState(blocks::PaleOakLog));
    // A lone pale oak sapling never grows (like dark oak).
    s.world.setBlock({8, 64, 8}, R().defaultState(blocks::PaleOakSapling));
    for (int i = 0; i < 40; ++i)
        s.updates.boneMeal({8, 64, 8});
    CHECK(R().blockOf(s.at({8, 64, 8})) == blocks::PaleOakSapling);
    // Bamboo: swords cut it at once; it roots only on soil or bamboo.
    CHECK(breakTicks(R().defaultState(blocks::Bamboo), {*itemRegistry().find("iron_sword"), 1}, true, false) == 0);
    CHECK(BlockUpdates::placement(s.world, R().defaultState(blocks::Bamboo), {12, 64, 12}, Direction::Up, 0, 0));
    CHECK_FALSE(BlockUpdates::placement(s.world, R().defaultState(blocks::Bamboo), {12, 70, 12}, Direction::Up, 0, 0));
}

TEST_CASE("new woods: planks from every log and wood, 4 logs make 3 wood") {
    std::array<ItemStack, 4> one{};
    one[0] = {*itemRegistry().find("stripped_pale_oak_wood"), 1};
    const auto planks = craft(one, 2);
    REQUIRE(planks);
    CHECK(planks->item == *itemRegistry().find("pale_oak_planks"));
    CHECK(planks->count == 4);
    std::array<ItemStack, 4> four{};
    for (auto& g : four)
        g = {*itemRegistry().find("mangrove_log"), 1};
    const auto wood = craft(four, 2);
    REQUIRE(wood);
    CHECK(wood->item == *itemRegistry().find("mangrove_wood"));
    CHECK(wood->count == 3);
}

TEST_CASE("signs: placement, text saved with the chunk, the editor's line width (M23.3c)") {
    Scene s;
    // On top: a standing sign facing the player (yaw 180 looks north: rotation 0, south).
    const BlockStateId sign = R().defaultState(B("oak_sign"));
    const auto standing = BlockUpdates::placement(s.world, sign, {0, 64, 0}, Direction::Up, 180.0f, 0);
    REQUIRE(standing);
    CHECK(R().value(*standing, "rotation") == "0");
    // On a side: the wall sign.
    s.world.setBlock({3, 64, 1}, R().defaultState(blocks::Stone));
    const auto wall = BlockUpdates::placement(s.world, sign, {3, 64, 0}, Direction::North, 0, 0);
    REQUIRE(wall);
    CHECK(R().blockOf(*wall) == B("oak_wall_sign"));
    CHECK(R().value(*wall, "facing") == "north");
    // A sign block brings its block entity; the text saves and loads.
    s.world.updateBlock({5, 64, 5}, *standing);
    Chunk* c = s.world.chunk({0, 0});
    SignData* data = c->sign(5, 64, 5);
    REQUIRE(data);
    std::snprintf(data->front.lines[1].data(), SignData::kChars + 1, "%s", "Hello sign");
    data->front.colour = 14; // red
    const auto nbt = chunkToNbt(ChunkSnapshot::of(*c, 0));
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(nbt, back));
    const SignData* loaded = back.sign(5, 64, 5);
    REQUIRE(loaded);
    CHECK(std::string(loaded->front.lines[1].data()) == "Hello sign");
    CHECK(loaded->front.colour == 14);
    // Breaking it removes the entity; it drops the sign item.
    s.world.updateBlock({5, 64, 5}, 0);
    CHECK(c->sign(5, 64, 5) == nullptr);
    Xoroshiro rng(1);
    std::vector<ItemStack> drops;
    blockDrops(*wall, {}, rng, drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].item == *itemRegistry().find("oak_sign"));
}

TEST_CASE("concrete powder falls like sand and hardens by water; terracotta dyes and glazes (M23.4a)") {
    Scene s;
    // Next to water: placed as concrete straight away.
    s.world.setBlock({0, 64, 1}, R().defaultState(blocks::Water));
    const auto placed = BlockUpdates::placement(s.world, S("red_concrete_powder"), {0, 64, 0}, Direction::Up, 0, 0);
    REQUIRE(placed);
    CHECK(R().blockOf(*placed) == B("red_concrete"));
    // Powder that water reaches later hardens on the update.
    s.world.updateBlock({3, 64, 0}, S("blue_concrete_powder"));
    CHECK(R().blockOf(s.at({3, 64, 0})) == B("blue_concrete_powder"));
    s.world.updateBlock({3, 65, 0}, R().defaultState(blocks::Water));
    CHECK(R().blockOf(s.at({3, 64, 0})) == B("blue_concrete"));
    // It falls like sand (a scheduled 2-tick check when air is below it).
    CHECK(R().likeOf(B("lime_concrete_powder")) == blocks::Sand);
    // Glazed terracotta faces the player; dyed terracotta smelts into it.
    const auto glazed = BlockUpdates::placement(s.world, S("cyan_glazed_terracotta"), {6, 64, 0}, Direction::Up, 0, 0);
    REQUIRE(glazed);
    CHECK(R().value(*glazed, "facing") == "north"); // (looking south: it faces back north)
    const auto out = smelt({*itemRegistry().find("cyan_terracotta"), 1});
    REQUIRE(out);
    CHECK(out->item == *itemRegistry().find("cyan_glazed_terracotta"));
}

TEST_CASE("copper: oxidizes over random ticks unless waxed; axes scrape, honeycomb waxes; bulbs toggle (M23.4b)") {
    Scene s;
    // A lone block at a high random tick speed (vanilla's chance is low: 64/1125 a
    // tick to try, then x0.75), and one held back by a less oxidized neighbour.
    s.updates.setRandomTicks({0, 0}, 1, 300);
    s.world.setBlock({4, 64, 4}, S("copper_block"));
    s.world.setBlock({10, 64, 10}, S("exposed_copper"));
    s.world.setBlock({11, 64, 10}, S("copper_block")); // keeps the exposed one from ageing
    s.world.setBlock({0, 66, 0}, S("waxed_copper_block"));
    for (int t = 0; t < 400; ++t) {
        s.updates.setTime(t);
        s.updates.tick();
    }
    CHECK(R().blockOf(s.at({4, 64, 4})) != B("copper_block")); // aged
    CHECK(R().blockOf(s.at({0, 66, 0})) == B("waxed_copper_block")); // waxed: never
    // (the exposed block can only age once its unaffected neighbour has caught up)
    if (R().blockOf(s.at({11, 64, 10})) == B("copper_block"))
        CHECK(R().blockOf(s.at({10, 64, 10})) == B("exposed_copper"));
    // Scraping and waxing.
    s.world.updateBlock({2, 70, 2}, S("weathered_cut_copper_stairs[facing=east,half=top]"));
    CHECK(BlockUpdates::scrapeCopper(s.world, {2, 70, 2}));
    CHECK(R().toString(s.at({2, 70, 2})) == "minecraft:exposed_cut_copper_stairs[facing=east,half=top,shape=straight]");
    CHECK(BlockUpdates::waxCopper(s.world, {2, 70, 2}));
    CHECK(R().blockOf(s.at({2, 70, 2})) == B("waxed_exposed_cut_copper_stairs"));
    CHECK(BlockUpdates::scrapeCopper(s.world, {2, 70, 2})); // wax off
    CHECK(R().blockOf(s.at({2, 70, 2})) == B("exposed_cut_copper_stairs"));
    // A bulb toggles on each rising edge of power; lit bulbs glow by oxidation.
    s.world.setBlock({5, 69, 5}, S("stone"));
    s.world.setBlock({6, 69, 5}, S("stone"));
    s.world.updateBlock({5, 70, 5}, S("copper_bulb"));
    s.world.updateBlock({6, 70, 5}, S("lever[face=floor]"));
    s.updates.use({6, 70, 5}); // lever on
    CHECK(R().value(s.at({5, 70, 5}), "lit") == "true");
    s.updates.use({6, 70, 5}); // off: stays lit
    CHECK(R().value(s.at({5, 70, 5}), "lit") == "true");
    s.updates.use({6, 70, 5}); // on again: off
    CHECK(R().value(s.at({5, 70, 5}), "lit") == "false");
    CHECK(R().lightEmission(S("weathered_copper_bulb[lit=true]")) == 8);
    // Copper doors open by hand, need a pickaxe and never burn.
    CHECK(harvestInfo(B("copper_door")).tool == ToolType::Pickaxe);
    CHECK(BlockUpdates::igniteOdds(B("copper_door")) == 0);
}

TEST_CASE("campfires: cook in 30 s, signal over hay, drop charcoal, save their food (M23.4c)") {
    CampfireData cf;
    cf.items[0] = {*itemRegistry().find("beef"), 1};
    std::array<ItemStack, 4> done{};
    int n = 0;
    for (int t = 0; t < 599 && n == 0; ++t)
        n = tickCampfire(cf, done);
    CHECK(n == 0);
    n = tickCampfire(cf, done); // the 600th tick
    REQUIRE(n == 1);
    CHECK(done[0].item == *itemRegistry().find("cooked_beef"));
    CHECK(cf.items[0].empty());
    Scene s;
    s.world.setBlock({0, 64, 0}, R().defaultState(blocks::HayBlock));
    const auto signal = BlockUpdates::placement(s.world, R().defaultState(blocks::Campfire), {0, 65, 0}, Direction::Up, 0, 0);
    REQUIRE(signal);
    CHECK(R().value(*signal, "signal_fire") == "true");
    CHECK(R().lightEmission(R().defaultState(blocks::Campfire)) == 15);
    CHECK(R().lightEmission(R().set(R().defaultState(blocks::SoulCampfire), properties::lit, 1)) == 0);
    Xoroshiro rng(2);
    std::vector<ItemStack> drops;
    blockDrops(R().defaultState(blocks::Campfire), {}, rng, drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].item == *itemRegistry().find("charcoal"));
    CHECK(drops[0].count == 2);
    // The food on it saves with the chunk.
    s.world.updateBlock({4, 64, 4}, R().defaultState(blocks::Campfire));
    Chunk* c = s.world.chunk({0, 0});
    REQUIRE(c->campfire(4, 64, 4));
    c->campfire(4, 64, 4)->items[2] = {*itemRegistry().find("porkchop"), 1};
    c->campfire(4, 64, 4)->cookTime[2] = 123;
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(chunkToNbt(ChunkSnapshot::of(*c, 0)), back));
    REQUIRE(back.campfire(4, 64, 4));
    CHECK(back.campfire(4, 64, 4)->items[2].item == *itemRegistry().find("porkchop"));
    CHECK(back.campfire(4, 64, 4)->cookTime[2] == 123);
}

TEST_CASE("smokers cook food, blast furnaces ores, both twice as fast; barrels hold 27 and save (M23.5)") {
    auto I = [](const char* n, int c = 1) { return ItemStack{*itemRegistry().find(n), static_cast<uint8_t>(c)}; };
    Furnace smoker;
    smoker.kind = 1;
    smoker.input = I("beef", 4);
    smoker.fuel = I("coal");
    for (int t = 0; t < 800; ++t)
        tickFurnace(smoker);
    CHECK(smoker.output.count == 4); // 100 ticks each...
    CHECK(smoker.fuel.empty()); // coal lasts 800 ticks here: 8 items
    smoker = {};
    smoker.kind = 1;
    smoker.input = I("raw_iron");
    smoker.fuel = I("coal");
    for (int t = 0; t < 300; ++t)
        tickFurnace(smoker);
    CHECK(smoker.output.empty()); // a smoker cooks only food
    CHECK(smoker.fuel.count == 1); // and lights no fuel for it
    Furnace blast;
    blast.kind = 2;
    blast.input = I("raw_iron");
    blast.fuel = I("coal");
    for (int t = 0; t < 100; ++t)
        tickFurnace(blast);
    CHECK(blast.output.item == *itemRegistry().find("iron_ingot"));
    blast = {};
    blast.kind = 2;
    blast.input = I("beef");
    blast.fuel = I("coal");
    for (int t = 0; t < 300; ++t)
        tickFurnace(blast);
    CHECK(blast.output.empty());
    // Placed, they get a furnace entity of their kind; a barrel a 27-slot one that saves.
    Scene s;
    s.world.updateBlock({1, 64, 1}, R().defaultState(blocks::BlastFurnace));
    s.world.updateBlock({2, 64, 1}, R().defaultState(blocks::Barrel));
    Chunk* c = s.world.chunk({0, 0});
    REQUIRE(c->furnace(1, 64, 1));
    CHECK(c->furnace(1, 64, 1)->kind == 2);
    REQUIRE(c->chest(2, 64, 1));
    c->chest(2, 64, 1)->items[26] = I("diamond", 3);
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(chunkToNbt(ChunkSnapshot::of(*c, 0)), back));
    REQUIRE(back.chest(2, 64, 1));
    CHECK(back.chest(2, 64, 1)->barrel);
    CHECK(back.chest(2, 64, 1)->items[26].count == 3);
    REQUIRE(back.furnace(1, 64, 1));
    CHECK(back.furnace(1, 64, 1)->kind == 2);
    // A barrel's lid faces the player; a smoker glows 13 when lit.
    const auto lid = BlockUpdates::placement(s.world, R().defaultState(blocks::Barrel), {3, 64, 1}, Direction::Up, 0, -60);
    REQUIRE(lid);
    CHECK(R().value(*lid, "facing") == "down");
    CHECK(R().lightEmission(*R().with(R().defaultState(blocks::Smoker), "lit", "true")) == 13);
    // Recipes: logs around a furnace, iron + smooth stone, planks + wooden slabs.
    std::array<ItemStack, 9> g{{{}, I("oak_log"), {}, I("crimson_stem"), I("furnace"), I("oak_log"), {}, I("oak_wood"), {}}};
    const auto smokerItem = craft(g, 3);
    REQUIRE(smokerItem);
    CHECK(smokerItem->item == *itemRegistry().find("smoker"));
    g = {I("oak_planks"), I("spruce_slab"), I("oak_planks"), I("oak_planks"), {}, I("oak_planks"), I("oak_planks"), I("oak_slab"), I("oak_planks")};
    const auto barrel = craft(g, 3);
    REQUIRE(barrel);
    CHECK(barrel->item == *itemRegistry().find("barrel"));
    g[1] = I("stone_slab"); // not a wooden slab
    CHECK_FALSE(craft(g, 3));
}

TEST_CASE("composters fill by chance and give bone meal; cauldrons take buckets, bottles and rain (M23.5)") {
    auto I = [](const char* n, int c = 1) { return ItemStack{*itemRegistry().find(n), static_cast<uint8_t>(c)}; };
    CHECK(BlockUpdates::compostChance(I("wheat_seeds").item) == 30);
    CHECK(BlockUpdates::compostChance(I("oak_leaves").item) == 30);
    CHECK(BlockUpdates::compostChance(I("apple").item) == 65);
    CHECK(BlockUpdates::compostChance(I("bread").item) == 85);
    CHECK(BlockUpdates::compostChance(I("stone").item) == 0);
    Scene s;
    const BlockPos p{1, 64, 1};
    s.world.updateBlock(p, R().defaultState(blocks::Composter));
    CHECK_FALSE(s.updates.compost(p, I("stone").item));
    CHECK(s.updates.compost(p, I("wheat_seeds").item)); // the first always adds a layer
    CHECK(R().get(s.at(p), properties::composterLevel) == 1);
    int used = 1;
    while (R().get(s.at(p), properties::composterLevel) < 7 && used < 200)
        used += s.updates.compost(p, I("wheat_seeds").item);
    CHECK(R().get(s.at(p), properties::composterLevel) == 7);
    CHECK(used > 7); // 30%: about 21 seeds on average
    CHECK_FALSE(s.updates.compost(p, I("apple").item)); // level 7: waits for the ready tick
    CHECK(s.updates.takeCompost(p).empty());
    for (int t = 0; t <= 21; ++t) {
        s.updates.setTime(t);
        s.updates.tick();
    }
    CHECK(R().get(s.at(p), properties::composterLevel) == 8);
    CHECK(BlockUpdates::cauldronSignal(s.at(p)) == 8);
    const ItemStack meal = s.updates.takeCompost(p);
    CHECK(meal.item == I("bone_meal").item);
    CHECK(R().get(s.at(p), properties::composterLevel) == 0);
    // Cauldrons: a water bucket fills it, three bottles empty it, a bucket takes it all.
    const BlockPos c{3, 64, 1};
    s.world.updateBlock(c, R().defaultState(blocks::Cauldron));
    CHECK_FALSE(s.updates.useCauldron(c, I("bucket")));
    auto back = s.updates.useCauldron(c, I("water_bucket"));
    REQUIRE(back);
    CHECK(back->item == I("bucket").item);
    CHECK(BlockUpdates::cauldronSignal(s.at(c)) == 3);
    back = s.updates.useCauldron(c, I("glass_bottle"));
    REQUIRE(back);
    CHECK(back->potion == static_cast<uint8_t>(Potion::Water));
    CHECK(BlockUpdates::cauldronSignal(s.at(c)) == 2);
    CHECK(s.updates.useCauldron(c, *back)); // pour it back: 3
    CHECK(BlockUpdates::cauldronSignal(s.at(c)) == 3);
    back = s.updates.useCauldron(c, I("bucket"));
    REQUIRE(back);
    CHECK(back->item == I("water_bucket").item);
    CHECK(R().blockOf(s.at(c)) == blocks::Cauldron);
    CHECK(s.updates.useCauldron(c, I("lava_bucket")));
    CHECK(R().blockOf(s.at(c)) == blocks::LavaCauldron);
    CHECK(R().lightEmission(s.at(c)) == 15);
    back = s.updates.useCauldron(c, I("bucket"));
    REQUIRE(back);
    CHECK(back->item == I("lava_bucket").item);
    // A filled cauldron drops the cauldron; it has a bowl to stand in.
    Xoroshiro rng(1);
    std::vector<ItemStack> drops;
    blockDrops(R().defaultState(blocks::WaterCauldron), {I("iron_pickaxe")}, rng, drops);
    REQUIRE(drops.size() == 1);
    CHECK(drops[0].item == I("cauldron").item);
    CHECK(collisionShape(R().defaultState(blocks::Cauldron)).count == 5);
    // Recipes.
    std::array<ItemStack, 9> g{I("iron_ingot"), {}, I("iron_ingot"), I("iron_ingot"), {}, I("iron_ingot"),
                               I("iron_ingot"), I("iron_ingot"), I("iron_ingot")};
    REQUIRE(craft(g, 3));
    CHECK(craft(g, 3)->item == I("cauldron").item);
    g = {I("oak_slab"), {}, I("birch_slab"), I("oak_slab"), {}, I("oak_slab"), I("oak_slab"), I("oak_slab"), I("oak_slab")};
    REQUIRE(craft(g, 3));
    CHECK(craft(g, 3)->item == I("composter").item);
}

#include "gameplay/Beacons.h"
#include "gameplay/Vitals.h"

TEST_CASE("beacons count pyramid tiers and give powers; conduits count their frame (M23.6)") {
    Scene s;
    const BlockPos b{0, 70, 0};
    for (int k = 1; k <= 2; ++k)
        for (int dz = -k; dz <= k; ++dz)
            for (int dx = -k; dx <= k; ++dx)
                s.world.setBlock({dx, 70 - k, dz}, R().defaultState(k == 1 ? blocks::IronBlock : blocks::DiamondBlock));
    s.world.updateBlock(b, R().defaultState(blocks::Beacon));
    CHECK(mc::beaconTiers(s.world, b) == 2);
    s.world.setBlock({2, 68, 2}, R().defaultState(blocks::Stone)); // a hole in tier 2
    CHECK(mc::beaconTiers(s.world, b) == 1);
    CHECK(mc::beaconSky(s.world, b));
    s.world.setBlock({0, 90, 0}, R().defaultState(blocks::Glass)); // glass lets the beam through
    CHECK(mc::beaconSky(s.world, b));
    s.world.setBlock({0, 91, 0}, R().defaultState(blocks::Stone));
    CHECK_FALSE(mc::beaconSky(s.world, b));
    CHECK(mc::beaconPrimaryAllowed(Effect::Speed, 1));
    CHECK_FALSE(mc::beaconPrimaryAllowed(Effect::Strength, 2));
    BeaconData d;
    d.levels = 4;
    d.beam = true;
    d.primary = static_cast<uint8_t>(Effect::Haste);
    d.secondary = static_cast<uint8_t>(Effect::Haste);
    std::array<mc::BeaconGift, 2> gifts{};
    REQUIRE(mc::beaconGifts(d, b, {30.0, 75.0, -40.0}, gifts) == 1); // range 50
    CHECK(gifts[0].amplifier == 1);  // Haste II
    CHECK(gifts[0].duration == 17 * 20);
    CHECK(mc::beaconGifts(d, b, {60.0, 75.0, 0.0}, gifts) == 0);
    d.secondary = static_cast<uint8_t>(Effect::Regeneration);
    CHECK(mc::beaconGifts(d, b, {0.0, 75.0, 0.0}, gifts) == 2);
    // Saved under its id with its powers.
    Chunk* c = s.world.chunk({0, 0});
    REQUIRE(c->beacon(0, 70, 0));
    *c->beacon(0, 70, 0) = d;
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(chunkToNbt(ChunkSnapshot::of(*c, 0)), back));
    REQUIRE(back.beacon(0, 70, 0));
    CHECK(back.beacon(0, 70, 0)->primary == d.primary);
    CHECK(back.beacon(0, 70, 0)->secondary == d.secondary);
    CHECK(back.beacon(0, 70, 0)->levels == 4);
    // A full conduit frame: 42 prismarine places on three rings.
    const BlockPos q{0, 80, 8};
    s.world.updateBlock(q, R().defaultState(blocks::Conduit));
    for (int dy = -2; dy <= 2; ++dy)
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx)
                if (std::max({std::abs(dx), std::abs(dy), std::abs(dz)}) == 2)
                    s.world.setBlock({q.x + dx, q.y + dy, q.z + dz}, S("minecraft:prismarine"));
    CHECK(mc::conduitFrame(s.world, q) == 42);
    CHECK(mc::conduitRange(42) == 96);
    CHECK_FALSE(mc::conduitWet(s.world, q));
    // Resistance and Conduit Power on the player; Haste in mining.
    mc::Vitals v;
    v.addEffect(Effect::Resistance, 1, 100); // II: -40%
    v.damage(10.0f, false);
    CHECK(v.health() == doctest::Approx(14.0f));
    v.addEffect(Effect::ConduitPower, 0, 100);
    for (int t = 0; t < 400; ++t)
        v.breathe(true);
    CHECK(v.air() == mc::Vitals::kMaxAir);
    const ItemStack pick{*itemRegistry().find("iron_pickaxe"), 1};
    CHECK(breakTicks(R().defaultState(blocks::Stone), pick, true, false, 2) <
          breakTicks(R().defaultState(blocks::Stone), pick, true, false, 0));
}

#include "gameplay/Jukebox.h"

TEST_CASE("note blocks: instrument from below, tuned by use, played by power edges; jukebox discs (M23.6)") {
    CHECK(BlockUpdates::noteInstrument(R().defaultState(blocks::GoldBlock)) == 6);   // bell
    CHECK(BlockUpdates::noteInstrument(R().defaultState(blocks::OakPlanks)) == 4);   // bass
    CHECK(BlockUpdates::noteInstrument(R().defaultState(blocks::Stone)) == 1);       // bass drum
    CHECK(BlockUpdates::noteInstrument(R().defaultState(blocks::Sand)) == 2);        // snare
    CHECK(BlockUpdates::noteInstrument(R().defaultState(blocks::Glass)) == 3);       // hat
    CHECK(BlockUpdates::noteInstrument(S("minecraft:white_wool")) == 7);             // guitar
    CHECK(BlockUpdates::noteInstrument(R().defaultState(blocks::Dirt)) == 0);        // harp
    Scene s;
    const BlockPos p{2, 64, 2};
    s.world.setBlock({2, 63, 2}, R().defaultState(blocks::GoldBlock));
    const auto placed = BlockUpdates::placement(s.world, R().defaultState(blocks::NoteBlock), p, Direction::Up, 0, 0);
    REQUIRE(placed);
    CHECK(R().value(*placed, "instrument") == "bell");
    s.world.updateBlock(p, *placed);
    s.world.soundEvents().clear();
    CHECK(s.updates.use(p));
    CHECK(R().get(s.at(p), properties::note) == 1);
    REQUIRE(s.world.soundEvents().size() == 1);
    CHECK(s.world.soundEvents()[0].sound == Sound::NoteBell);
    CHECK(s.world.soundEvents()[0].pitch == doctest::Approx(std::pow(2.0f, -11.0f / 12.0f)));
    for (int i = 0; i < 24; ++i)
        s.updates.use(p);
    CHECK(R().get(s.at(p), properties::note) == 0); // 24 wraps to 0
    // A lever beside it: switching on plays once, staying on doesn't.
    s.world.soundEvents().clear();
    s.world.updateBlock({3, 64, 2}, S("minecraft:lever[face=floor,facing=north,powered=false]"));
    s.updates.use({3, 64, 2});
    int notes = 0;
    for (const auto& e : s.world.soundEvents())
        notes += e.sound == Sound::NoteBell;
    CHECK(notes == 1);
    s.world.setBlock({2, 65, 2}, R().defaultState(blocks::Stone)); // a block on top silences it
    s.world.soundEvents().clear();
    s.updates.use(p);
    CHECK(s.world.soundEvents().empty());
    // Discs and the jukebox's tune: deterministic, with notes in it.
    const int cat = mc::discIndex(*itemRegistry().find("music_disc_cat"));
    REQUIRE(cat >= 0);
    CHECK(mc::discInfo(cat).comparator == 2);
    CHECK(mc::discInfo(cat).lengthTicks == 185 * 20);
    CHECK(mc::discIndex(*itemRegistry().find("stone")) == -1);
    int count = 0;
    std::array<mc::JukeboxNote, 3> a{}, b{};
    for (int t = 0; t < 400; ++t) {
        const int n = mc::jukeboxNotes(cat, t, a);
        CHECK(n == mc::jukeboxNotes(cat, t, b));
        for (int k = 0; k < n; ++k) {
            CHECK(a[size_t(k)].pitch >= 0.49f);
            CHECK(a[size_t(k)].pitch <= 2.01f);
        }
        count += n;
    }
    CHECK(count > 50);
    // A jukebox keeps its disc in the save.
    s.world.updateBlock({5, 64, 5}, R().defaultState(blocks::Jukebox));
    Chunk* c = s.world.chunk({0, 0});
    REQUIRE(c->jukebox(5, 64, 5));
    c->jukebox(5, 64, 5)->record = {*itemRegistry().find("music_disc_cat"), 1};
    c->jukebox(5, 64, 5)->playing = true;
    c->jukebox(5, 64, 5)->ticks = 77;
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(chunkToNbt(ChunkSnapshot::of(*c, 0)), back));
    REQUIRE(back.jukebox(5, 64, 5));
    CHECK(back.jukebox(5, 64, 5)->record.item == *itemRegistry().find("music_disc_cat"));
    CHECK(back.jukebox(5, 64, 5)->ticks == 77);
}

TEST_CASE("copper doors wax and scrape both halves together (M23 review)") {
    Scene s;
    const BlockId door = B("copper_door");
    s.world.updateBlock({1, 64, 1}, S("minecraft:copper_door[facing=north,half=lower,hinge=left,open=false,powered=false]"));
    s.world.updateBlock({1, 65, 1}, S("minecraft:copper_door[facing=north,half=upper,hinge=left,open=false,powered=false]"));
    REQUIRE(R().blockOf(s.at({1, 65, 1})) == door);
    CHECK(BlockUpdates::waxCopper(s.world, {1, 65, 1})); // honeycomb on the upper half
    CHECK(R().blockOf(s.at({1, 64, 1})) == B("waxed_copper_door"));
    CHECK(R().blockOf(s.at({1, 65, 1})) == B("waxed_copper_door"));
    CHECK(R().value(s.at({1, 65, 1}), "half") == "upper");
    CHECK(BlockUpdates::scrapeCopper(s.world, {1, 64, 1}));
    CHECK(R().blockOf(s.at({1, 65, 1})) == door);
}

TEST_CASE("a water cauldron washes a dyed shulker box, keeping its slots (M23 parity)") {
    Scene s;
    const BlockPos c{3, 64, 3};
    s.world.updateBlock(c, R().set(R().defaultState(blocks::WaterCauldron), properties::cauldronLevel, 2));
    ItemStack box{*itemRegistry().find("blue_shulker_box"), 1};
    ItemContents slots{};
    slots[5] = {*itemRegistry().find("apple"), 3};
    box.contents = addItemContents(slots);
    const auto washed = s.updates.useCauldron(c, box);
    REQUIRE(washed);
    CHECK(washed->item == *itemRegistry().find("shulker_box"));
    CHECK(itemContents(washed->contents)[5].count == 3);
    CHECK(BlockUpdates::cauldronSignal(s.at(c)) == 2);
    CHECK_FALSE(s.updates.useCauldron(c, *washed)); // a plain box stays as it is
}
