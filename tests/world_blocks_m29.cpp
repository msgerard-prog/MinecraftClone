// Blocks of M29 (completeness; wiki pages of each block).
#include "gameplay/Buckets.h"
#include "gameplay/Mining.h"
#include "gameplay/Vitals.h"
#include "gameplay/Mobs.h"
#include "gameplay/Recipes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Potions.h"
#include "world/World.h"

#include <doctest/doctest.h>

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
    // Cutting the line detaches the hooks.
    w.updateBlock({5, 64, 4}, 0);
    CHECK(r.value(w.getBlock({3, 64, 4}), "attached") == "false");
    CHECK(r.value(w.getBlock({4, 64, 4}), "attached") == "false");
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
