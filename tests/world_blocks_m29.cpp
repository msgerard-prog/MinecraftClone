// Blocks of M29 (completeness; wiki pages of each block).
#include "gameplay/Beds.h"
#include "gameplay/Buckets.h"
#include "gameplay/FluidContact.h"
#include "gameplay/Mining.h"
#include "gameplay/Vitals.h"
#include "gameplay/Mobs.h"
#include "gameplay/Recipes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/LevelData.h"
#include "world/Potions.h"
#include "world/World.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace mc;
using namespace mc::world;

TEST_CASE("M29.4a: flowers behave like poppies; lily pads need water; cave air is air; raw blocks craft") {
    const auto& r = blockRegistry();
    CHECK(r.likeOf(blocks::RedTulip) == blocks::Poppy);
    CHECK(r.findBlock("minecraft:cave_air") == BlockId(0));
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    // A lily pad stays on water, breaks when the water goes; a wither rose stands on soul sand.
    w.setBlock({4, 63, 4}, r.defaultState(blocks::Water));
    w.setBlock({4, 64, 4}, r.defaultState(blocks::LilyPad));
    w.updateBlock({4, 63, 4}, r.defaultState(blocks::Water));
    CHECK(w.getBlock({4, 64, 4}) == r.defaultState(blocks::LilyPad));
    w.updateBlock({4, 63, 4}, r.defaultState(blocks::Stone));
    CHECK(w.getBlock({4, 64, 4}) == 0);
    w.setBlock({6, 63, 4}, r.defaultState(blocks::SoulSand));
    w.setBlock({6, 64, 4}, r.defaultState(blocks::WitherRose));
    w.updateBlock({6, 63, 4}, r.defaultState(blocks::SoulSoil));
    CHECK(w.getBlock({6, 64, 4}) == r.defaultState(blocks::WitherRose));
    std::array<ItemStack, 9> g{};
    for (auto& s : g) s = {*itemRegistry().find("raw_gold"), 1};
    CHECK(itemRegistry().item(craft(g, 3)->item).id == "minecraft:raw_gold_block");
    g = {};
    g[0] = {*itemRegistry().find("wither_rose"), 1};
    CHECK(itemRegistry().item(craft(g, 3)->item).id == "minecraft:black_dye");
    // A jack o'lantern on two snow blocks makes a snow golem.
    w.setBlock({8, 64, 8}, r.defaultState(blocks::SnowBlock));
    w.setBlock({8, 65, 8}, r.defaultState(blocks::SnowBlock));
    w.setBlock({8, 66, 8}, r.defaultState(blocks::JackOLantern));
    Xoroshiro rng{1};
    CHECK(Mobs::buildSnowGolem(w, {8, 66, 8}, rng));
    CHECK(r.lightEmission(r.defaultState(blocks::JackOLantern)) == 15);
}

TEST_CASE("M29.4b: flower pots hold 37 plants; a potted plant drops the pot and the plant") {
    const auto& r = blockRegistry();
    CHECK(pottedFor(blocks::Poppy) == *r.findBlock("minecraft:potted_poppy"));
    CHECK(plantInPot(*r.findBlock("minecraft:potted_flowering_azalea_bush")) == blocks::FloweringAzalea);
    CHECK(pottedFor(blocks::Stone) == 0);
    for (BlockId b = blocks::PottedFirst; b <= blocks::PottedLast; ++b) {
        INFO(r.block(b).id);
        CHECK(plantInPot(b) != 0);
        CHECK(r.likeOf(b) == blocks::FlowerPot);
    }
    CHECK_FALSE(itemRegistry().find("potted_cactus"));
    Xoroshiro rng{1};
    std::vector<ItemStack> out;
    blockDrops(r.defaultState(pottedFor(blocks::Cactus)), {}, rng, out);
    REQUIRE(out.size() == 2);
    CHECK(itemRegistry().item(out[0].item).id == "minecraft:flower_pot");
    CHECK(itemRegistry().item(out[1].item).id == "minecraft:cactus");
}

TEST_CASE("M29.4c: soul fire on soul blocks; glow lichen and wall fans hold to their wall; bamboo shoots") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    // Fire lit on soul sand burns as soul fire and goes with it.
    w.setBlock({2, 63, 2}, r.defaultState(blocks::SoulSand));
    w.updateBlock({2, 64, 2}, BlockUpdates::fireState(0));
    CHECK(r.blockOf(w.getBlock({2, 64, 2})) == blocks::SoulFire);
    CHECK(r.lightEmission(r.defaultState(blocks::SoulFire)) == 10);
    w.updateBlock({2, 63, 2}, r.defaultState(blocks::Stone));
    CHECK(w.getBlock({2, 64, 2}) == 0);
    // Glow lichen put on a block's east side covers its west side.
    w.setBlock({5, 64, 5}, r.defaultState(blocks::Stone));
    const auto lichen = BlockUpdates::placement(w, r.defaultState(blocks::GlowLichen), {6, 64, 5}, Direction::East, 0, 0);
    REQUIRE(lichen);
    CHECK(r.value(*lichen, "facing") == "west");
    w.updateBlock({6, 64, 5}, *lichen);
    w.updateBlock({5, 64, 5}, 0);
    CHECK(w.getBlock({6, 64, 5}) == 0);
    // A fan put on a side becomes a wall fan facing out, and breaks with its wall.
    w.setBlock({8, 64, 8}, r.defaultState(blocks::Stone));
    const auto fan = BlockUpdates::placement(w, r.defaultState(*r.findBlock("minecraft:tube_coral_fan")), {8, 64, 9},
                                             Direction::South, 0, 0);
    REQUIRE(fan);
    CHECK(r.block(r.blockOf(*fan)).id == "minecraft:tube_coral_wall_fan");
    CHECK(r.value(*fan, "facing") == "south");
    w.updateBlock({8, 64, 9}, *fan);
    w.updateBlock({8, 64, 8}, 0);
    CHECK(r.blockOf(w.getBlock({8, 64, 9})) == blocks::Water); // (its water stays)
    // Bamboo planted on grass is a shoot; bone meal grows it into two bamboo blocks.
    w.setBlock({10, 63, 10}, r.defaultState(blocks::GrassBlock));
    const auto shoot = BlockUpdates::placement(w, r.defaultState(blocks::Bamboo), {10, 64, 10}, Direction::Up, 0, 0);
    REQUIRE(shoot);
    CHECK(r.blockOf(*shoot) == blocks::BambooSapling);
    w.updateBlock({10, 64, 10}, *shoot);
    CHECK(u.boneMeal({10, 64, 10}));
    CHECK(r.blockOf(w.getBlock({10, 64, 10})) == blocks::Bamboo);
    CHECK(r.blockOf(w.getBlock({10, 65, 10})) == blocks::Bamboo);
}

TEST_CASE("M29.4c: powder snow freezes in 140 ticks, then hurts every 40; leather keeps warm; buckets") {
    Vitals v;
    for (int i = 0; i < 140; ++i) v.tickFreezing(true, false);
    CHECK(v.frozen() == doctest::Approx(1.0f));
    const float before = v.health();
    for (int i = 0; i < 40; ++i) v.tickFreezing(true, false);
    CHECK(v.health() == doctest::Approx(before - 1.0f));
    for (int i = 0; i < 70; ++i) v.tickFreezing(false, false);
    CHECK(v.frozenTicks() == 0);
    for (int i = 0; i < 100; ++i) v.tickFreezing(true, true); // leather armor
    CHECK(v.frozenTicks() == 0);
    // An empty bucket scoops it; the bucket puts it back.
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    w.setBlock({4, 64, 4}, r.defaultState(blocks::PowderSnow));
    std::vector<BlockPos> changed;
    const auto got = useBucket(w, *itemRegistry().find("bucket"), {4.5, 66.5, 4.5}, {0, -1, 0}, 5.0, changed);
    REQUIRE(got);
    CHECK(itemRegistry().item(got->filled).id == "minecraft:powder_snow_bucket");
    CHECK(w.getBlock({4, 64, 4}) == 0);
    w.setBlock({4, 63, 4}, r.defaultState(blocks::Stone));
    const auto back = useBucket(w, got->filled, {4.5, 66.5, 4.5}, {0, -1, 0}, 5.0, changed);
    REQUIRE(back);
    CHECK(r.blockOf(w.getBlock({4, 64, 4})) == blocks::PowderSnow);
}

TEST_CASE("M29.5: two hooks with tripwire between attach; something in the wire powers them") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    int64_t t = 0;
    u.setTime(t);
    for (const int x : {2, 8}) w.setBlock({x, 64, 4}, r.defaultState(blocks::Stone));
    auto place = [&](BlockId b, BlockPos p, Direction face) {
        const auto s = BlockUpdates::placement(w, r.defaultState(b), p, face, 0, 0);
        REQUIRE(s);
        w.updateBlock(p, *s);
    };
    place(blocks::TripwireHook, {3, 64, 4}, Direction::East); // on the west wall, facing east
    for (int x = 4; x <= 6; ++x) place(blocks::Tripwire, {x, 64, 4}, Direction::Up);
    place(blocks::TripwireHook, {7, 64, 4}, Direction::West);
    CHECK(r.value(w.getBlock({3, 64, 4}), "attached") == "true");
    CHECK(r.value(w.getBlock({7, 64, 4}), "attached") == "true");
    CHECK(r.value(w.getBlock({5, 64, 4}), "attached") == "true");
    CHECK(r.value(w.getBlock({5, 64, 4}), "east") == "true");
    // Stepping in: both hooks power for as long as something stays, then 10 ticks more at most.
    u.setTime(++t);
    u.pressPlate({5, 64, 4}, false);
    u.settlePlates();
    CHECK(r.value(w.getBlock({3, 64, 4}), "powered") == "true");
    CHECK(r.value(w.getBlock({7, 64, 4}), "powered") == "true");
    for (int i = 0; i < 12; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(r.value(w.getBlock({3, 64, 4}), "powered") == "false");
    // Cutting the line (without shears) trips the hooks for 10 ticks, then they detach
    // (M29 review; wiki: Tripwire).
    w.updateBlock({5, 64, 4}, 0);
    CHECK(r.value(w.getBlock({3, 64, 4}), "powered") == "true");
    CHECK(r.value(w.getBlock({7, 64, 4}), "powered") == "true");
    for (int i = 0; i < 12; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(r.value(w.getBlock({3, 64, 4}), "powered") == "false");
    CHECK(r.value(w.getBlock({3, 64, 4}), "attached") == "false");
    CHECK(r.value(w.getBlock({4, 64, 4}), "attached") == "false");
    // Mended, then cut with shears (disarmed first, as BlockInteraction does): no pulse.
    place(blocks::Tripwire, {5, 64, 4}, Direction::Up);
    REQUIRE(r.value(w.getBlock({3, 64, 4}), "attached") == "true");
    w.setBlock({5, 64, 4}, r.set(w.getBlock({5, 64, 4}), properties::disarmed, 0));
    w.updateBlock({5, 64, 4}, 0);
    CHECK(r.value(w.getBlock({3, 64, 4}), "powered") == "false");
    CHECK(r.value(w.getBlock({3, 64, 4}), "attached") == "false");
}

TEST_CASE("M29.5: a daylight detector reads the sky - full at noon, none at night; inverted the other way") {
    const auto& r = blockRegistry();
    World w;
    Chunk& c = w.createChunk({0, 0});
    auto l = std::make_shared<SectionLight>();
    l->sky.fill(15);
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
    light.fill(l);
    c.setLight(light);
    BlockUpdates u(w);
    w.setListener(&u);
    u.setDayTime(6000); // noon
    u.setSkyDarken(0);
    CHECK(u.daylightPower({4, 64, 4}, false) == 15);
    CHECK(u.daylightPower({4, 64, 4}, true) == 0);
    u.setDayTime(18000); // midnight: the sky is 11 levels darker
    u.setSkyDarken(11);
    CHECK(u.daylightPower({4, 64, 4}, false) == 0);
    CHECK(u.daylightPower({4, 64, 4}, true) == 11);
    // Placed, it updates on its own tick; using it inverts it.
    int64_t t = 0;
    u.setTime(t);
    w.updateBlock({4, 64, 4}, r.defaultState(blocks::DaylightDetector));
    for (int i = 0; i < 3; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(r.get(w.getBlock({4, 64, 4}), properties::power) == 0);
    CHECK(u.use({4, 64, 4}));
    CHECK(r.get(w.getBlock({4, 64, 4}), properties::power) == 11);
    // Dust joins it (regression: dust ignored detectors, plates, targets) and lights a lamp.
    for (int x = 3; x <= 6; ++x)
        for (int z = 3; z <= 7; ++z) w.setBlock({x, 63, z}, r.defaultState(blocks::Stone));
    w.updateBlock({4, 64, 5}, r.defaultState(blocks::RedstoneWire));
    w.updateBlock({4, 64, 6}, r.defaultState(blocks::RedstoneLamp));
    CHECK(r.value(w.getBlock({4, 64, 5}), "north") == "side");
    CHECK(r.value(w.getBlock({4, 64, 6}), "lit") == "true");
}

TEST_CASE("M29.5: trapped chests pair only with trapped chests and power a lamp while open") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    for (int x = 2; x <= 8; ++x)
        for (int z = 2; z <= 8; ++z) w.setBlock({x, 63, z}, r.defaultState(blocks::Stone));
    auto place = [&](BlockId b, BlockPos p) {
        const auto s = BlockUpdates::placement(w, r.defaultState(b), p, Direction::Up, 0, 0); // looking south
        REQUIRE(s);
        w.updateBlock(p, *s);
    };
    place(blocks::TrappedChest, {4, 64, 4});
    place(blocks::TrappedChest, {5, 64, 4});
    CHECK(BlockUpdates::chestPartner(w, {4, 64, 4}) == std::optional<BlockPos>(BlockPos{5, 64, 4}));
    place(blocks::Chest, {3, 64, 4}); // a plain chest beside stays single
    CHECK_FALSE(BlockUpdates::chestPartner(w, {3, 64, 4}));
    CHECK(w.chunk({0, 0})->chest(4, 64, 4) != nullptr); // (it holds items like a chest)
    w.updateBlock({4, 64, 5}, r.defaultState(blocks::RedstoneLamp));
    CHECK(r.value(w.getBlock({4, 64, 5}), "lit") == "false");
    u.setChestOpen({5, 64, 4}, true); // opening either half powers both
    CHECK(r.value(w.getBlock({4, 64, 5}), "lit") == "true");
    u.setChestOpen({5, 64, 4}, false);
    int64_t t = 0;
    for (int i = 0; i < 6; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(r.value(w.getBlock({4, 64, 5}), "lit") == "false");
}

TEST_CASE("M29.5: lightning goes to a rod on top within 128 blocks, powers it 8 ticks and cleans its copper") {
    const auto& r = blockRegistry();
    World w;
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) w.createChunk({cx, cz});
    BlockUpdates u(w);
    w.setListener(&u);
    w.setBlock({5, 63, 5}, r.defaultState(blocks::Stone));
    const BlockId weathered = *r.findBlock("minecraft:weathered_lightning_rod");
    CHECK(r.likeOf(weathered) == blocks::LightningRod);
    CHECK(BlockUpdates::isCopper(weathered));
    w.setBlock({5, 64, 5}, r.defaultState(weathered));
    const auto aim = u.lightningRodNear({-10, 70, 12});
    REQUIRE(aim);
    CHECK(*aim == BlockPos{5, 65, 5});
    int64_t t = 0;
    u.setTime(t);
    w.updateBlock({5, 62, 5}, r.defaultState(blocks::RedstoneLamp)); // under the stone it stands on
    u.strikeLightning(*aim);
    CHECK(r.blockOf(w.getBlock({5, 64, 5})) == blocks::LightningRod); // back to bare copper
    CHECK(r.value(w.getBlock({5, 64, 5}), "powered") == "true");
    CHECK(r.value(w.getBlock({5, 62, 5}), "lit") == "true"); // (strong power through the stone)
    CHECK(w.getBlock({5, 65, 5}) == 0);                        // no fire
    for (int i = 0; i < 9; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(r.value(w.getBlock({5, 64, 5}), "powered") == "false");
    // Covered by a block, it is not the top of its column: no redirect.
    w.setBlock({5, 66, 5}, r.defaultState(blocks::Stone));
    CHECK_FALSE(u.lightningRodNear({-10, 70, 12}));
}

TEST_CASE("M29.5: a calibrated sculk sensor hears vibrations 16 blocks away; a plain one only 8") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    w.createChunk({1, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    w.updateBlock({2, 64, 4}, r.defaultState(blocks::SculkSensor));
    w.updateBlock({3, 64, 4}, r.defaultState(blocks::CalibratedSculkSensor));
    u.vibrate({15.5, 64.5, 4.5}, false); // 12-13 blocks away
    CHECK(r.value(w.getBlock({2, 64, 4}), "sculk_sensor_phase") == "inactive");
    CHECK(r.value(w.getBlock({3, 64, 4}), "sculk_sensor_phase") == "active");
    CHECK(r.get(w.getBlock({3, 64, 4}), properties::power) > 0);
}

TEST_CASE("M29.5: respawn anchors - light and comparator by charge, a stand spot only while charged") {
    const auto& r = blockRegistry();
    const BlockStateId empty = r.defaultState(blocks::RespawnAnchor);
    const BlockStateId full = r.set(empty, properties::charges, 4);
    CHECK(r.lightEmission(empty) == 0);
    CHECK(r.lightEmission(r.set(empty, properties::charges, 1)) == 3);
    CHECK(r.lightEmission(full) == 15);
    World w;
    w.createChunk({0, 0});
    for (int x = 2; x <= 6; ++x)
        for (int z = 2; z <= 6; ++z) w.setBlock({x, 63, z}, r.defaultState(blocks::Netherrack));
    w.setBlock({4, 64, 4}, empty);
    CHECK_FALSE(anchorStandSpot(w, {4, 64, 4}));
    w.setBlock({4, 64, 4}, full);
    const auto spot = anchorStandSpot(w, {4, 64, 4});
    REQUIRE(spot);
    CHECK(spot->y == doctest::Approx(64.0));
    // The respawn point keeps its dimension in level.dat (vanilla respawn.dimension).
    LevelData l;
    l.hasRespawn = true;
    l.respawn[0] = 4, l.respawn[1] = 64, l.respawn[2] = 4;
    l.respawnDimension = 1;
    const auto dir = std::filesystem::temp_directory_path() / "mc_test_anchor";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    REQUIRE(l.save(dir));
    const auto back = LevelData::load(dir);
    REQUIRE(back);
    std::filesystem::remove_all(dir);
    CHECK(back->respawnDimension == 1);
}

TEST_CASE("M29.5: chiseled bookshelves - slots by where the front is clicked, books in and out, comparator") {
    const auto& r = blockRegistry();
    CHECK(BlockUpdates::bookshelfSlot(0.1, 0.9) == 0); // top left
    CHECK(BlockUpdates::bookshelfSlot(0.5, 0.9) == 1);
    CHECK(BlockUpdates::bookshelfSlot(0.9, 0.1) == 5); // bottom right
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    w.updateBlock({4, 64, 4}, r.defaultState(blocks::ChiseledBookshelf));
    const ItemStack book{*itemRegistry().find("enchanted_book"), 1};
    CHECK_FALSE(u.putBook({4, 64, 4}, 2, {*itemRegistry().find("stick"), 1}));
    CHECK(u.putBook({4, 64, 4}, 2, book));
    CHECK(r.value(w.getBlock({4, 64, 4}), "slot_2_occupied") == "true");
    CHECK(w.chunk({0, 0})->chest(4, 64, 4)->lastSlot == 2); // (a comparator reads last slot + 1)
    CHECK_FALSE(u.putBook({4, 64, 4}, 2, book)); // (taken)
    const ItemStack back = u.takeBook({4, 64, 4}, 2);
    CHECK(back.item == book.item);
    CHECK(r.value(w.getBlock({4, 64, 4}), "slot_2_occupied") == "false");
}

TEST_CASE("M29.5: scaffolding reaches 6 out from its support; the rest breaks when the support goes") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    int64_t t = 0;
    u.setTime(t);
    w.setBlock({2, 63, 4}, r.defaultState(blocks::Stone));
    auto place = [&](BlockPos p) {
        const auto s = BlockUpdates::placement(w, r.defaultState(blocks::Scaffolding), p, Direction::Up, 0, 0);
        if (s) w.updateBlock(p, *s);
        return s.has_value();
    };
    REQUIRE(place({2, 64, 4}));
    CHECK(r.get(w.getBlock({2, 64, 4}), properties::scaffoldDistance) == 0);
    for (int x = 3; x <= 8; ++x) REQUIRE(place({x, 64, 4}));
    CHECK(r.get(w.getBlock({8, 64, 4}), properties::scaffoldDistance) == 6);
    CHECK(r.value(w.getBlock({8, 64, 4}), "bottom") == "true");
    CHECK_FALSE(place({9, 64, 4})); // 7 out: too far
    w.updateBlock({2, 63, 4}, 0);    // the support goes
    for (int i = 0; i < 20; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(w.getBlock({8, 64, 4}) == 0);
    CHECK(w.getBlock({2, 64, 4}) == 0);
}

TEST_CASE("M29.5: soul sand under water makes a bubble column up, magma one down; it goes with its base") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    int64_t t = 0;
    auto run = [&](int n) {
        for (int i = 0; i < n; ++i) {
            u.setTime(++t);
            u.tick();
        }
    };
    for (int y = 64; y <= 66; ++y) w.setBlock({4, y, 4}, r.defaultState(blocks::Water));
    w.updateBlock({4, 63, 4}, r.defaultState(blocks::SoulSand));
    run(25);
    for (int y = 64; y <= 66; ++y) {
        CHECK(r.blockOf(w.getBlock({4, y, 4})) == blocks::BubbleColumn);
        CHECK(r.value(w.getBlock({4, y, 4}), "drag") == "false");
    }
    CHECK(r.waterlogged(w.getBlock({4, 65, 4}))); // (a water source to everything else)
    w.updateBlock({4, 63, 4}, r.defaultState(blocks::MagmaBlock));
    run(40);
    CHECK(r.value(w.getBlock({4, 66, 4}), "drag") == "true");
    w.updateBlock({4, 63, 4}, r.defaultState(blocks::Stone));
    run(40);
    CHECK(w.getBlock({4, 66, 4}) == r.defaultState(blocks::Water));
    // The push: up inside, harder at the top; down at most 0.3 inside.
    FluidContact c;
    c.bubble = 1;
    glm::dvec3 v{0.0};
    for (int i = 0; i < 30; ++i) applyBubbleColumn(c, v);
    CHECK(v.y == doctest::Approx(0.7));
    c.bubble = -1;
    v = {};
    for (int i = 0; i < 30; ++i) applyBubbleColumn(c, v);
    CHECK(v.y == doctest::Approx(-0.3));
}

TEST_CASE("M29.6: copper bars, chains, lanterns and torches; chain is iron_chain now") {
    const auto& r = blockRegistry();
    CHECK(r.findBlock("minecraft:chain") == std::optional<BlockId>(BlockId(blocks::Chain)));
    CHECK(r.block(blocks::Chain).id == "minecraft:iron_chain");
    CHECK(itemRegistry().find("chain") == itemRegistry().find("iron_chain"));
    for (const char* id : {"copper_bars", "weathered_copper_chain", "waxed_oxidized_copper_lantern"}) {
        INFO(id);
        const auto b = r.findBlock(std::string("minecraft:") + id);
        REQUIRE(b);
        CHECK(BlockUpdates::isCopper(*b));
    }
    CHECK(r.likeOf(*r.findBlock("minecraft:exposed_copper_lantern")) == blocks::Lantern);
    CHECK(r.lightEmission(r.defaultState(blocks::CopperTorch)) == 14);
    World w;
    w.createChunk({0, 0});
    w.setBlock({4, 64, 4}, r.defaultState(blocks::Stone));
    const auto wall = BlockUpdates::placement(w, r.defaultState(blocks::CopperTorch), {5, 64, 4}, Direction::East, 0, 0);
    REQUIRE(wall);
    CHECK(r.blockOf(*wall) == blocks::CopperWallTorch);
    CHECK(itemRegistry().blockItem(blocks::CopperWallTorch) == *itemRegistry().find("copper_torch"));
}

TEST_CASE("M29.6: shelves hold 3 stacks; powered ones side by side make a row of up to 3") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    const BlockId cherry = *r.findBlock("minecraft:cherry_shelf");
    CHECK(r.likeOf(cherry) == blocks::Shelf);
    for (int x = 3; x <= 6; ++x) // facing south: the viewer, looking north, has +x... on the right
        w.updateBlock({x, 64, 4}, *r.with(r.defaultState(x == 4 ? cherry : blocks::Shelf), "facing", "south"));
    CHECK(w.chunk({0, 0})->chest(4, 64, 4)->shelf);
    std::array<BlockPos, 3> row;
    CHECK(u.shelfRow({4, 64, 4}, row) == 1); // unpowered: alone
    for (int x = 3; x <= 6; ++x) w.updateBlock({x, 65, 4}, r.defaultState(blocks::RedstoneBlock));
    CHECK(r.value(w.getBlock({4, 64, 4}), "powered") == "true");
    const int n = u.shelfRow({5, 64, 4}, row);
    CHECK(n == 3);
    CHECK(row[0] == BlockPos{3, 64, 4}); // the viewer's left end
    CHECK(r.value(w.getBlock({3, 64, 4}), "side_chain") != "unconnected");
}

TEST_CASE("M29.6: copper golem statues age like copper and change pose when used") {
    const auto& r = blockRegistry();
    const BlockId exposed = *r.findBlock("minecraft:exposed_copper_golem_statue");
    CHECK(BlockUpdates::isCopper(exposed));
    CHECK(r.likeOf(exposed) == blocks::CopperGolemStatue);
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    w.updateBlock({4, 64, 4}, r.defaultState(blocks::CopperGolemStatue));
    CHECK(u.use({4, 64, 4}));
    CHECK(r.value(w.getBlock({4, 64, 4}), "copper_golem_pose") == "sitting");
}

TEST_CASE("M29.7: technical blocks - barriers block, light blocks shine, player heads, petrified oak slabs") {
    const auto& r = blockRegistry();
    CHECK(r.collides(r.defaultState(blocks::Barrier)));
    CHECK_FALSE(r.opaqueCube(r.defaultState(blocks::Barrier)));
    CHECK_FALSE(r.collides(r.defaultState(blocks::Light)));
    CHECK(r.lightEmission(r.defaultState(blocks::Light)) == 15);
    CHECK(r.lightEmission(r.set(r.defaultState(blocks::Light), properties::level, 7)) == 7);
    CHECK(isMobHead(blocks::PlayerHead));
    CHECK(isWallHead(blocks::PlayerWallHead));
    CHECK_FALSE(isWallHead(blocks::PlayerHead));
    const ItemId head = *itemRegistry().find("player_head");
    CHECK(itemRegistry().item(head).armorSlot == 1);
    CHECK(itemRegistry().blockItem(blocks::PlayerWallHead) == head);
    World w;
    w.createChunk({0, 0});
    w.setBlock({4, 64, 4}, r.defaultState(blocks::Stone));
    const auto wall = BlockUpdates::placement(w, r.defaultState(blocks::PlayerHead), {5, 64, 4}, Direction::East, 0, 0);
    REQUIRE(wall);
    CHECK(r.blockOf(*wall) == blocks::PlayerWallHead);
    CHECK(r.kind(blocks::PetrifiedOakSlab) == BlockKind::Slab);
    CHECK(r.block(blocks::PetrifiedOakSlab).settings.tool == HarvestTool::Pickaxe);
    CHECK(itemRegistry().item(*itemRegistry().find("barrier")).texture == "item/barrier");
}

TEST_CASE("M29 review: pistons don't move chiseled bookshelves, shelves or crafters (their slots stay)") {
    const auto& r = blockRegistry();
    for (const BlockId b : {BlockId(blocks::ChiseledBookshelf), BlockId(blocks::Shelf + 2), BlockId(blocks::Crafter)}) {
        World w;
        w.createChunk({0, 0});
        BlockUpdates u(w);
        w.setListener(&u);
        w.updateBlock({8, 64, 8}, r.set(r.defaultState(blocks::Piston), properties::facing6, 5)); // (east)
        w.updateBlock({9, 64, 8}, r.defaultState(b));
        w.updateBlock({8, 64, 9}, r.defaultState(blocks::RedstoneBlock));
        for (int t = 1; t < 6; ++t) {
            u.setTime(t);
            u.tick();
        }
        CHECK(r.blockOf(w.getBlock({9, 64, 8})) == b);
    }
}

TEST_CASE("M29 review: a trapped chest broken while open stops powering; dust connects to trapped chests; replaced dispensers save as droppers") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    for (int x = 2; x <= 8; ++x)
        for (int z = 2; z <= 8; ++z) w.setBlock({x, 63, z}, r.defaultState(blocks::Stone));
    w.updateBlock({4, 64, 4}, r.defaultState(blocks::TrappedChest));
    w.updateBlock({5, 64, 4}, r.defaultState(blocks::TrappedChest));
    u.setChestOpen({4, 64, 4}, true);
    w.updateBlock({5, 64, 4}, 0); // (blown up while looked into)
    u.setChestOpen({4, 64, 4}, false);
    w.updateBlock({5, 64, 5}, r.defaultState(blocks::RedstoneLamp));
    w.updateBlock({5, 64, 4}, r.defaultState(blocks::TrappedChest)); // a new one there is unpowered
    int64_t t = 0;
    for (int i = 0; i < 6; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(r.value(w.getBlock({5, 64, 5}), "lit") == "false");
    // Dust beside a trapped chest points at it.
    w.updateBlock({4, 64, 6}, r.defaultState(blocks::TrappedChest));
    w.updateBlock({5, 64, 6}, r.defaultState(blocks::RedstoneWire));
    CHECK(r.value(w.getBlock({5, 64, 6}), "west") != "none");
    // A dispenser turned into a dropper (or a chest into a trapped chest) changes its kind.
    w.setBlock({7, 64, 7}, r.defaultState(blocks::Dispenser));
    w.setBlock({7, 64, 7}, r.defaultState(blocks::Dropper));
    CHECK(w.chunk({0, 0})->dispenser(7, 64, 7)->dropper);
    w.setBlock({8, 64, 8}, r.defaultState(blocks::Chest));
    w.setBlock({8, 64, 8}, r.defaultState(blocks::TrappedChest));
    CHECK(w.chunk({0, 0})->chest(8, 64, 8)->trapped);
}

TEST_CASE("M29 review: eyes in a bubble column don't lose air; in plain water they do") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    w.setBlock({4, 64, 4}, r.defaultState(blocks::Water));
    w.setBlock({5, 64, 4}, r.defaultState(blocks::BubbleColumn));
    CHECK(eyesUnderWater(w, {4.5, 64.5, 4.5}));
    CHECK_FALSE(eyesUnderWater(w, {5.5, 64.5, 4.5}));
}

TEST_CASE("M29 review: comparators read shelves (left 1, middle 2, right 4) and statue poses (1-4)") {
    const auto& r = blockRegistry();
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    w.setBlock({4, 64, 4}, r.defaultState(blocks::Shelf));
    CHECK(u.containerSignal({4, 64, 4}) == 0);
    w.chunk({0, 0})->chest(4, 64, 4)->items[0] = {*itemRegistry().find("apple"), 1};
    w.chunk({0, 0})->chest(4, 64, 4)->items[2] = {*itemRegistry().find("apple"), 1};
    CHECK(u.containerSignal({4, 64, 4}) == 5);
    w.setBlock({6, 64, 4}, *r.with(r.defaultState(blocks::CopperGolemStatue), "copper_golem_pose", "running"));
    CHECK(u.containerSignal({6, 64, 4}) == 3);
}
