// Banners and the loom (M28.3d; wiki: Banner, Loom, Banner Pattern).
#include "gameplay/Loom.h"
#include "gameplay/Recipes.h"
#include "world/Blocks.h"
#include "world/BlockUpdates.h"
#include "world/ChunkSerializer.h"
#include "world/ItemExtras.h"
#include "world/World.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {
ItemStack stack(const char* id) { return {*itemRegistry().find(id), 1}; }
}

TEST_CASE("banners: 16 colours, standing and on walls; wall banners drop the standing item") {
    const auto& r = blockRegistry();
    const auto red = r.findBlock("red_banner"), redWall = r.findBlock("red_wall_banner");
    REQUIRE(red);
    REQUIRE(redWall);
    CHECK(r.kind(*red) == BlockKind::Banner);
    CHECK(itemRegistry().blockItem(*redWall) == itemRegistry().blockItem(*red));
    CHECK(itemRegistry().item(itemRegistry().blockItem(*red)).maxStack == 16);
    CHECK(bannerColour("minecraft:light_gray_wall_banner") == 8);
    CHECK(bannerColour("minecraft:gray_banner") == 7);

    World world;
    world.createChunk({0, 0});
    world.setBlock({5, 64, 5}, r.defaultState(blocks::Stone));
    // On the stone's side: a wall banner facing away from it; on top: turned to the player.
    const auto wall = BlockUpdates::placement(world, r.defaultState(*red), {6, 64, 5}, Direction::East, 0.0f, 0.0f, 0.5);
    REQUIRE(wall);
    CHECK(r.blockOf(*wall) == *redWall);
    const auto top = BlockUpdates::placement(world, r.defaultState(*red), {5, 65, 5}, Direction::Up, 90.0f, 0.0f, 0.5);
    REQUIRE(top);
    CHECK(r.get(*top, properties::rotation16) == 12); // (yaw 90 + 180 -> 270 degrees)
}

TEST_CASE("the loom weaves a pattern in the dye's colour, 6 at most; the pattern item stays its own") {
    std::array<int, 48> pats{};
    const int plain = loomPatterns({}, pats);
    CHECK(plain == 32);
    CHECK(loomPatterns(stack("creeper_banner_pattern"), pats) == 1);
    CHECK(std::string(kBannerPatterns[size_t(pats[0])].name) == "creeper");

    ItemStack banner = stack("white_banner");
    for (int i = 0; i < kMaxLoomLayers; ++i) {
        const ItemStack woven = loomResult(banner, stack("red_dye"), i);
        REQUIRE_FALSE(woven.empty());
        banner = woven;
    }
    const auto layers = bannerLayers(banner.extra);
    REQUIRE(layers);
    CHECK(layers->count == 6);
    CHECK(layers->colour[0] == 14); // red
    CHECK(loomResult(banner, stack("red_dye"), 0).empty()); // (full)
    CHECK(loomResult(stack("white_wool"), stack("red_dye"), 0).empty());
    // Identical banners share their layers (they stack).
    CHECK(loomResult(stack("white_banner"), stack("blue_dye"), 3).extra ==
          loomResult(stack("white_banner"), stack("blue_dye"), 3).extra);
}

TEST_CASE("banners save their layers: block entity patterns and the item's banner_patterns") {
    World world;
    Chunk& c = world.createChunk({0, 0});
    const BlockId red = *blockRegistry().findBlock("red_banner");
    world.setBlock({3, 64, 3}, blockRegistry().defaultState(red));
    BannerLayers* l = c.banner(3, 64, 3);
    REQUIRE(l);
    l->count = 2;
    l->pattern[0] = uint8_t(*findBannerPattern("cross"));
    l->colour[0] = 15;
    l->pattern[1] = uint8_t(*findBannerPattern("border"));
    l->colour[1] = 4;
    const nbt::Compound n = chunkToNbt(ChunkSnapshot::of(c));
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(n, back));
    const BannerLayers* b = back.banner(3, 64, 3);
    REQUIRE(b);
    CHECK(*b == *l);

    ItemStack item = stack("red_banner");
    item.extra = addBannerLayers(*l);
    const nbt::Compound in = itemToNbt(item, 0);
    REQUIRE(in.compound("components")->list("minecraft:banner_patterns"));
    CHECK(itemFromNbtPublic(in).extra == item.extra);

    // A banner whose stone goes pops with its layers.
    world.setBlock({3, 63, 3}, blockRegistry().defaultState(blocks::Stone));
    world.setBlock({3, 64, 3}, blockRegistry().defaultState(red));
    *c.banner(3, 64, 3) = *l;
    BlockUpdates updates(world);
    world.updateBlock({3, 63, 3}, 0);
    REQUIRE(updates.drops().size() == 1);
    CHECK(updates.drops()[0].stack.extra == item.extra);
    CHECK(world.getBlock({3, 64, 3}) == 0);

    std::array<ItemStack, 9> g{};
    for (int i : {0, 1, 2, 3, 4, 5}) g[size_t(i)] = stack("lime_wool");
    g[7] = stack("stick");
    const auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:lime_banner");
}
