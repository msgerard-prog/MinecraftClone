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
