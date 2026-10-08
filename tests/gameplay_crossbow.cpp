// Crossbows (M28.4a; wiki: Crossbow, Quick Charge, Multishot, Piercing).
#include "gameplay/Inventory.h"
#include "gameplay/Projectiles.h"
#include "world/ChunkSerializer.h"
#include "world/Enchantments.h"
#include "world/ItemExtras.h"
#include "world/LevelData.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace mc;
using namespace mc::world;

TEST_CASE("crossbows load in 25 ticks (5 fewer per Quick Charge level), using an arrow") {
    ItemStack bow{*itemRegistry().find("crossbow"), 1};
    CHECK(crossbowChargeTicks(bow) == 25);
    setEnchantment(bow, Enchantment::QuickCharge, 3);
    CHECK(crossbowChargeTicks(bow) == 10);

    Inventory inv;
    inv.setSlot(0, {*itemRegistry().find("crossbow"), 1});
    CHECK_FALSE(loadCrossbow(inv, true)); // (no arrow)
    inv.setSlot(5, {*itemRegistry().find("arrow"), 2});
    CHECK(loadCrossbow(inv, true));
    CHECK(inv.slot(0).state == kCrossbowArrow);
    CHECK(inv.slot(5).count == 1);
    CHECK_FALSE(loadCrossbow(inv, true)); // (already loaded)
}

TEST_CASE("crossbows fire one arrow, Multishot three (two not picked up), Piercing sets the pass count") {
    Xoroshiro rng{3};
    Projectiles shots;
    Inventory inv;
    ItemStack bow{*itemRegistry().find("crossbow"), 1};
    setEnchantment(bow, Enchantment::Multishot, 1);
    bow.state = kCrossbowArrow;
    inv.setSlot(0, bow);
    REQUIRE(fireCrossbow(inv, true, {0, 64, 0}, {0, 0, 1}, shots, rng));
    REQUIRE(shots.items().size() == 3);
    int pickups = 0;
    for (const Projectile& p : shots.items()) {
        pickups += p.pickup;
        CHECK(glm::length(p.vel) == doctest::Approx(3.15).epsilon(0.05));
    }
    CHECK(pickups == 1);
    CHECK(inv.slot(0).state == 0);
    CHECK(inv.slot(0).damage == 3); // (Multishot wears it 3)
    CHECK_FALSE(fireCrossbow(inv, true, {0, 64, 0}, {0, 0, 1}, shots, rng)); // (empty)

    ItemStack pb{*itemRegistry().find("crossbow"), 1};
    setEnchantment(pb, Enchantment::Piercing, 4);
    pb.state = kCrossbowArrow;
    inv.setSlot(0, pb);
    Projectiles one;
    REQUIRE(fireCrossbow(inv, false, {0, 64, 0}, {0, 0, 1}, one, rng));
    CHECK(one.items()[0].pierce == 4);
    CHECK(conflicts(Enchantment::Multishot, Enchantment::Piercing));
}

TEST_CASE("a loaded crossbow is saved as charged_projectiles; written books keep their kind in level.dat") {
    ItemStack bow{*itemRegistry().find("crossbow"), 1};
    bow.state = kCrossbowArrow;
    const nbt::Compound n = itemToNbt(bow, 0);
    REQUIRE(n.compound("components")->list("minecraft:charged_projectiles"));
    CHECK(itemFromNbtPublic(n).state == kCrossbowArrow);

    // (M28.4a regression: level.dat items were written as item 1, so a written book's
    // pages came back as a book and quill's)
    const auto dir = std::filesystem::temp_directory_path() / "mc_written_book_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    BookContent book;
    book.title = "Log";
    book.author = "Player";
    book.pages = {"day one"};
    LevelData l;
    LevelData::SavedItem it;
    it.slot = 0;
    it.id = "minecraft:written_book";
    it.extra = addBook(book);
    l.inventory.push_back(it);
    LevelData::SavedItem cb;
    cb.slot = 1;
    cb.id = "minecraft:crossbow";
    cb.itemState = kCrossbowArrow;
    l.inventory.push_back(cb);
    REQUIRE(l.save(dir));
    const auto back = LevelData::load(dir);
    REQUIRE(back);
    REQUIRE(back->inventory.size() == 2);
    const auto b = bookContent(back->inventory[0].extra);
    REQUIRE(b);
    CHECK(b->title == "Log");
    CHECK(back->inventory[1].itemState == kCrossbowArrow);
    std::filesystem::remove_all(dir);
}

#include "gameplay/Brewing.h"
#include "gameplay/Player.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/Potions.h"
#include "world/World.h"

TEST_CASE("arrows: the offhand's first, tipped arrows carry their potion, spectral ones glow") {
    Inventory inv;
    ItemStack tipped{*itemRegistry().find("tipped_arrow"), 3};
    tipped.potion = uint8_t(Potion::Poison);
    inv.setSlot(7, {*itemRegistry().find("arrow"), 5});
    inv.setSlot(4, tipped);
    CHECK(ammoSlot(inv) == 4); // (inventory order)
    inv.setOffhand({*itemRegistry().find("spectral_arrow"), 1});
    CHECK(ammoSlot(inv) == -1);

    inv.setSlot(0, {*itemRegistry().find("bow"), 1});
    Projectiles shots;
    Xoroshiro rng{4};
    REQUIRE(releaseBow(inv, 20, true, {0, 64, 0}, {0, 0, 1}, shots, rng));
    CHECK(shots.items().back().spectral);
    CHECK(inv.offhand().empty());
    REQUIRE(releaseBow(inv, 20, true, {0, 64, 0}, {0, 0, 1}, shots, rng));
    CHECK(shots.items().back().potion == uint8_t(Potion::Poison));
    CHECK(inv.slot(4).count == 2);

    // Crafting: 8 arrows around a lingering potion; 4 glowstone dust around an arrow.
    std::array<ItemStack, 9> g{};
    g.fill({*itemRegistry().find("arrow"), 1});
    g[4] = {*itemRegistry().find("lingering_potion"), 1};
    g[4].potion = uint8_t(Potion::Healing);
    auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:tipped_arrow");
    CHECK(r->count == 8);
    CHECK(r->potion == uint8_t(Potion::Healing));
    std::array<ItemStack, 9> s{};
    s[1] = s[3] = s[5] = s[7] = {*itemRegistry().find("glowstone_dust"), 1};
    s[4] = {*itemRegistry().find("arrow"), 1};
    r = craft(s, 3);
    REQUIRE(r);
    CHECK(r->count == 2);
}

TEST_CASE("lingering potions: brewed with dragon's breath, they leave a cloud of their effect") {
    ItemStack splash{*itemRegistry().find("splash_potion"), 1};
    splash.potion = uint8_t(Potion::Regeneration);
    const auto lingering = brewResult({*itemRegistry().find("dragon_breath"), 1}, splash);
    REQUIRE(lingering);
    CHECK(itemRegistry().item(lingering->item).id == "minecraft:lingering_potion");
    CHECK(lingering->potion == uint8_t(Potion::Regeneration));

    World world;
    world.createChunk({0, 0});
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) world.setBlock({x, 63, z}, blockRegistry().defaultState(blocks::Stone));
    Player player;
    player.setPosition({8.5, 64.0, 8.5});
    Vitals vitals;
    Inventory inv;
    inv.setSlot(0, *lingering);
    Projectiles shots;
    Xoroshiro rng{6};
    throwSplashPotion(inv, true, {8.5, 66.0, 8.5}, 0.0f, 90.0f, shots, rng); // (straight down)
    for (int t = 0; t < 40; ++t) shots.tick(world, player, &vitals, inv, true, rng);
    REQUIRE(shots.clouds().size() == 1);
    CHECK(shots.clouds()[0].potion == uint8_t(Potion::Regeneration));
    CHECK(vitals.effectLevel(Effect::Regeneration) > 0);
}
