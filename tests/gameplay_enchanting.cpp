// Enchanting table and anvil (wiki: Enchanting mechanics, Anvil mechanics).
#include "gameplay/Anvil.h"
#include "gameplay/Enchanting.h"
#include "ui/ContainerScreen.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {
ItemStack I(const char* id, int n = 1) { return {*itemRegistry().find(id), uint8_t(n)}; }
} // namespace

TEST_CASE("bookshelves count when 2 blocks out with air between, up to 15") {
    World w;
    w.createChunk({0, 0});
    const BlockPos t{8, 64, 8};
    w.setBlock(t, blockRegistry().defaultState(blocks::EnchantingTable));
    for (int x = 6; x <= 10; ++x)
        for (int y = 64; y <= 65; ++y) {
            w.setBlock({x, y, 6}, blockRegistry().defaultState(blocks::Bookshelf));
            w.setBlock({x, y, 10}, blockRegistry().defaultState(blocks::Bookshelf));
        }
    CHECK(countBookshelves(w, t) == 15); // 20 placed, capped
    for (int x = 6; x <= 10; ++x) // a wall of stone between the table and the far row
        for (int y = 64; y <= 65; ++y)
            w.setBlock({x, y, 9}, blockRegistry().defaultState(blocks::Stone));
    CHECK(countBookshelves(w, t) == 10);
}

TEST_CASE("offers: 15 bookshelves make the bottom slot 30; the same seed gives the same offers") {
    const auto a = enchantOffers(I("diamond_pickaxe"), 15, 1234);
    const auto b = enchantOffers(I("diamond_pickaxe"), 15, 1234);
    CHECK(a[2].cost == 30);
    for (int i = 0; i < 3; ++i) {
        CHECK(a[size_t(i)].cost == b[size_t(i)].cost);
        CHECK(a[size_t(i)].hint == b[size_t(i)].hint);
        if (a[size_t(i)].cost) CHECK(canEnchant(I("diamond_pickaxe").item, a[size_t(i)].hint));
    }
    CHECK(enchantOffers(I("stick"), 15, 1)[0].cost == 0); // not enchantable
    const auto pick = pickEnchantments(I("book"), 30, 77, 2);
    REQUIRE(pick.count > 0);
    const ItemStack book = applyEnchantments(I("book"), pick);
    CHECK(itemRegistry().item(book.item).id == "minecraft:enchanted_book");
    CHECK(isEnchanted(book));
}

TEST_CASE("anvil: repair with material (25% each), combine (+12%, levels merge), apply a book") {
    ItemStack worn = I("iron_pickaxe");
    worn.damage = 200; // of 250
    AnvilResult r = anvilCombine(worn, I("iron_ingot", 5), false);
    CHECK(r.out.damage == 0); // 200 -> 138 -> 76 -> 14 -> 0 (4 ingots of 5)
    CHECK(r.materialUsed == 4);
    CHECK(r.cost == 4);
    CHECK(r.out.repairCost == 1);
    ItemStack a = I("diamond_sword"), b = I("diamond_sword");
    setEnchantment(a, Enchantment::Sharpness, 3);
    setEnchantment(b, Enchantment::Sharpness, 3);
    r = anvilCombine(a, b, false);
    CHECK(enchantLevel(r.out, Enchantment::Sharpness) == 4);
    ItemStack book = I("enchanted_book");
    setEnchantment(book, Enchantment::Smite, 2); // clashes with Sharpness: skipped
    setEnchantment(book, Enchantment::Looting, 3);
    r = anvilCombine(a, book, false);
    CHECK(enchantLevel(r.out, Enchantment::Looting) == 3);
    CHECK(enchantLevel(r.out, Enchantment::Smite) == 0);
    ItemStack pricey = a;
    pricey.repairCost = 63;
    CHECK(anvilCombine(pricey, book, false).tooExpensive);
    CHECK_FALSE(anvilCombine(pricey, book, true).tooExpensive);
}

TEST_CASE("enchanting screen: an offer uses levels and lapis and enchants the item") {
    using ui::ContainerScreen;
    Inventory inv;
    for (int i = 0; i < Inventory::kSlots; ++i)
        inv.setSlot(i, {});
    inv.setSlot(0, I("iron_sword"));
    inv.setSlot(1, I("lapis_lazuli", 10));
    ContainerScreen s;
    std::vector<ItemStack> drops;
    s.openEnchanting(15, 42);
    s.setPlayer(40, false, 42);
    // Panel at (112, 67) in a 400x300 GUI: hotbar y 67+142, item slot (15,47), lapis (35,47).
    auto click = [&](int x, int y, bool shiftClick = false) {
        s.click(112 + x + 8, 67 + y + 8, ContainerScreen::Button::Left, shiftClick, 400, 300, inv, drops);
    };
    click(8, 142);  // pick up the sword
    click(15, 47);  // into the table
    click(8 + 18, 142); // lapis
    click(35, 47);
    click(60 + 50, 14 + 2 * 19 + 5 - 8); // the bottom offer (button area; -8 undoes the +8)
    CHECK(s.takeLevelsSpent() == 3);
    CHECK(s.takeEnchanted());
    CHECK(s.grid(1).count == 7);
    CHECK(isEnchanted(s.grid(0)));
}
