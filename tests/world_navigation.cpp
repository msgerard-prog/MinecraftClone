// Navigation items (M28.2; wiki: Compass, Lodestone, Recovery Compass, Clock).
#include "gameplay/Recipes.h"
#include "rendering/ItemIcons.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/Dimension.h"
#include "world/ItemExtras.h"
#include "world/LevelData.h"

#include <doctest/doctest.h>

#include <array>
#include <filesystem>

using namespace mc;
using namespace mc::world;
using gfx::ItemIcons;

TEST_CASE("compass needle: straight ahead 0, right 8, behind 16, left 24 (of 32)") {
    const glm::dvec3 p(0.0, 64.0, 0.0);
    // Facing south (yaw 0, +Z): ahead is +Z, the right hand is west (-X).
    CHECK(ItemIcons::compassFrame(p, 0.0f, {0.0, 64.0, 100.0}) == 0);
    CHECK(ItemIcons::compassFrame(p, 0.0f, {-100.0, 64.0, 0.0}) == 8);
    CHECK(ItemIcons::compassFrame(p, 0.0f, {0.0, 64.0, -100.0}) == 16);
    CHECK(ItemIcons::compassFrame(p, 0.0f, {100.0, 64.0, 0.0}) == 24);
    // Turning to face west (yaw 90) brings a western target straight ahead.
    CHECK(ItemIcons::compassFrame(p, 90.0f, {-100.0, 64.0, 0.0}) == 0);
    CHECK(ItemIcons::compassFrame(p, -270.0f, {-100.0, 64.0, 0.0}) == 0);
}

TEST_CASE("clock: noon frame 0, midnight half way round") {
    CHECK(ItemIcons::clockFrame(0.0) == 0);
    CHECK(ItemIcons::clockFrame(0.5) == 32);
    CHECK(ItemIcons::clockFrame(0.999) == 0);
}

TEST_CASE("lodestone compasses save vanilla's lodestone_tracker") {
    ItemStack c{*itemRegistry().find("compass"), 1};
    c.extra = addLodestoneTarget({{10, 70, -5}, uint8_t(Dimension::Nether)});
    const nbt::Compound n = itemToNbt(c, 0);
    const nbt::Compound* comps = n.compound("components");
    REQUIRE(comps);
    const nbt::Compound* tracker = comps->compound("minecraft:lodestone_tracker");
    REQUIRE(tracker);
    REQUIRE(tracker->compound("target"));
    CHECK(*tracker->compound("target")->string("dimension") == "minecraft:the_nether");
    const ItemStack back = itemFromNbtPublic(n);
    const auto t = lodestoneTarget(back.extra);
    REQUIRE(t);
    CHECK(t->hasTarget);
    CHECK(t->pos == BlockPos{10, 70, -5});
    CHECK(t->dimension == uint8_t(Dimension::Nether));
    // Two compasses bound to the same lodestone separately don't stack (different entries)
    // but a copy of one does.
    ItemStack copy = back;
    CHECK(copy.sameKind(back));
    CHECK_FALSE(back.sameKind({*itemRegistry().find("compass"), 1}));
}

TEST_CASE("the last death is saved as LastDeathLocation") {
    const auto dir = std::filesystem::temp_directory_path() / "mc_lastdeath_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    LevelData l;
    l.hasLastDeath = true;
    l.lastDeath[0] = 5, l.lastDeath[1] = -20, l.lastDeath[2] = 7;
    l.lastDeathDimension = int(Dimension::End);
    REQUIRE(l.save(dir));
    const auto back = LevelData::load(dir);
    REQUIRE(back);
    CHECK(back->hasLastDeath);
    CHECK(back->lastDeath[1] == -20);
    CHECK(back->lastDeathDimension == int(Dimension::End));
    std::filesystem::remove_all(dir);
}

TEST_CASE("compass, clock, recovery compass and lodestone recipes") {
    auto stack = [](const char* id) { return ItemStack{*itemRegistry().find(id), 1}; };
    std::array<ItemStack, 9> g{};
    g[1] = g[3] = g[5] = g[7] = stack("iron_ingot");
    g[4] = stack("redstone");
    auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:compass");
    g[1] = g[3] = g[5] = g[7] = stack("gold_ingot");
    r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:clock");
    g.fill(stack("echo_shard"));
    g[4] = stack("compass");
    r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:recovery_compass");
    g.fill(stack("chiseled_stone_bricks"));
    g[4] = stack("netherite_ingot");
    r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:lodestone");
}
