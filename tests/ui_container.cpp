// Container screens: slot clicks and crafting (wiki: Inventory › Controls, Crafting).
#include "ui/ContainerScreen.h"
#include "gameplay/Grindstone.h"
#include "gameplay/Smithing.h"
#include "gameplay/Stonecutter.h"
#include "world/ArmorTrims.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/Enchantments.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace mc;
using namespace mc::ui;
using namespace mc::world;

namespace {

ItemStack I(const char* name, int n = 1) { return {*itemRegistry().find(name), static_cast<uint8_t>(n)}; }

// GUI 400x300: panel at ((400-176)/2, (300-166)/2) = (112, 67).
constexpr int kGw = 400, kGh = 300;
double sx(int x) { return 112 + x + 8; }
double sy(int y) { return 67 + y + 8; }
double invX(int col) { return sx(8 + col * 18); }
double hotbarY() { return sy(142); }
double mainY(int row) { return sy(84 + row * 18); }

struct Fixture {
    Inventory inv;
    ContainerScreen screen;
    std::vector<ItemStack> drops;
    Fixture() {
        for (int i = 0; i < Inventory::kSlots; ++i)
            inv.setSlot(i, {});
    }
    void left(double x, double y, bool shift = false) {
        screen.click(x, y, ContainerScreen::Button::Left, shift, kGw, kGh, inv, drops);
    }
    void right(double x, double y) { screen.click(x, y, ContainerScreen::Button::Right, false, kGw, kGh, inv, drops); }
};

} // namespace

TEST_CASE("slots: pick up, split with right click, place one, merge, swap") {
    Fixture f;
    f.inv.setSlot(0, I("cobblestone", 10));
    f.inv.setSlot(1, I("dirt", 3));
    f.screen.open(ContainerScreen::Type::Inventory);
    f.right(invX(0), hotbarY()); // take half: 5
    CHECK(f.screen.carried().count == 5);
    CHECK(f.inv.slot(0).count == 5);
    f.right(invX(2), hotbarY()); // place one in an empty slot
    CHECK(f.inv.slot(2).count == 1);
    f.left(invX(0), hotbarY()); // merge the remaining 4 back
    CHECK(f.inv.slot(0).count == 9);
    CHECK(f.screen.carried().empty());
    f.left(invX(1), hotbarY()); // pick up dirt
    f.left(invX(0), hotbarY()); // swap with the cobblestone
    CHECK(itemRegistry().item(f.inv.slot(0).item).id == "minecraft:dirt");
    CHECK(itemRegistry().item(f.screen.carried().item).id == "minecraft:cobblestone");
}

TEST_CASE("2x2 crafting: log -> planks; taking the result uses one of each ingredient") {
    Fixture f;
    f.inv.setSlot(0, I("oak_log", 2));
    f.screen.open(ContainerScreen::Type::Inventory);
    f.left(invX(0), hotbarY());   // carry 2 logs
    f.left(sx(98), sy(18));       // into grid slot 0
    CHECK(itemRegistry().item(f.screen.result().item).id == "minecraft:oak_planks");
    f.left(sx(154), sy(28));      // take 4 planks
    CHECK(f.screen.carried().count == 4);
    CHECK(f.screen.grid(0).count == 1);
    f.left(sx(154), sy(28));      // again: 8 planks carried, grid empty
    CHECK(f.screen.carried().count == 8);
    CHECK(f.screen.grid(0).empty());
    CHECK(f.screen.result().empty());
}

TEST_CASE("crafting table: shift-click crafts as many as fit; closing returns the grid") {
    Fixture f;
    f.inv.setSlot(0, I("oak_planks", 6));
    f.screen.open(ContainerScreen::Type::Crafting);
    f.left(invX(0), hotbarY());
    f.right(sx(30), sy(17)); // one plank in the top-left
    f.right(sx(30), sy(35)); // one below it -> sticks
    CHECK(itemRegistry().item(f.screen.result().item).id == "minecraft:stick");
    f.left(invX(0), hotbarY()); // put the remaining 4 back
    f.left(sx(124), sy(35), true); // shift: craft once (each slot had 1)
    CHECK(f.inv.slot(1).count == 4);
    f.screen.close(f.inv, f.drops);
    CHECK(f.drops.empty());
}

TEST_CASE("clicking outside drops the carried stack; closing gives the carried back") {
    Fixture f;
    f.inv.setSlot(0, I("dirt", 5));
    f.screen.open(ContainerScreen::Type::Inventory);
    f.left(invX(0), hotbarY());
    f.right(5, 5); // right outside: drop one
    REQUIRE(f.drops.size() == 1);
    CHECK(f.drops[0].count == 1);
    f.screen.close(f.inv, f.drops);
    CHECK(f.inv.slot(0).count == 4); // the rest returned on close
}

TEST_CASE("furnace screen: shift-click sends smeltables to input and fuel to fuel") {
    Fixture f;
    Furnace furnace;
    f.inv.setSlot(9, I("raw_iron", 3));
    f.inv.setSlot(10, I("coal", 2));
    f.screen.open(ContainerScreen::Type::Furnace, &furnace);
    f.left(invX(0), mainY(0), true);
    f.left(invX(1), mainY(0), true);
    CHECK(furnace.input.count == 3);
    CHECK(furnace.fuel.count == 2);
}

TEST_CASE("furnace fuel slot takes only fuel; shift-click merges into the furnace") {
    Fixture f;
    Furnace furnace;
    furnace.input = I("raw_iron", 2);
    f.inv.setSlot(0, I("dirt", 4));
    f.inv.setSlot(9, I("raw_iron", 3));
    f.screen.open(ContainerScreen::Type::Furnace, &furnace);
    f.left(invX(0), hotbarY());   // carry dirt
    f.left(sx(56), sy(53));       // fuel slot refuses it
    CHECK(furnace.fuel.empty());
    CHECK(f.screen.carried().count == 4);
    f.left(invX(0), hotbarY());   // put it back
    f.left(invX(0), mainY(0), true); // shift raw iron: merges with the 2 in the input
    CHECK(furnace.input.count == 5);
}

TEST_CASE("chest screen: 3 rows (double: 6), shift-click moves stacks in and out") {
    Fixture f;
    ChestData a, b;
    f.inv.setSlot(0, I("cobblestone", 20));
    f.screen.openChest(&a, nullptr);
    CHECK(f.screen.chestRows() == 3);
    CHECK(f.screen.height() == 168);
    // Panel at ((400-176)/2, (300-168)/2) = (112, 66); hotbar at y 32 + 54 + 58 = 144.
    const double hotY = 66 + 144 + 8, chestY0 = 66 + 18 + 8;
    f.left(112 + 8 + 8, hotY, true); // shift-click the hotbar stack into the chest
    CHECK(f.inv.slot(0).empty());
    CHECK(a.items[0].count == 20);
    f.left(112 + 8 + 8, chestY0, true); // and back out
    CHECK(a.items[0].empty());
    CHECK(f.inv.slot(0).count == 20);
    f.screen.close(f.inv, f.drops);
    f.screen.openChest(&a, &b);
    CHECK(f.screen.chestRows() == 6);
    CHECK(f.screen.height() == 222);
}

TEST_CASE("inventory screen: armor slots take only their piece; shift-click puts armor on") {
    Fixture f;
    f.inv.setSlot(0, I("iron_boots"));
    f.inv.setSlot(1, I("dirt", 5));
    f.screen.open(ContainerScreen::Type::Inventory);
    f.left(invX(0), hotbarY(), true); // shift: boots go on
    CHECK(f.inv.armor(3).item == I("iron_boots").item);
    CHECK(f.inv.slot(0).empty());
    f.left(invX(1), hotbarY()); // carry dirt
    f.left(sx(8), sy(8));       // the helmet slot refuses it
    CHECK(f.inv.armor(0).empty());
    CHECK_FALSE(f.screen.carried().empty());
    f.left(sx(77), sy(62)); // the offhand takes anything
    CHECK(f.inv.offhand().count == 5);
}

TEST_CASE("stonecutter: recipes from families and conversions; choosing one cuts one input each (M23.5)") {
    auto ids = [](const char* input) {
        std::vector<std::string> out;
        for (const ItemStack& s : stonecutterRecipes(I(input).item))
            out.push_back(itemRegistry().item(s.item).id.substr(10) + "x" + std::to_string(s.count));
        return out;
    };
    const auto stone = ids("stone");
    CHECK(stone == std::vector<std::string>{"chiseled_stone_bricksx1", "stone_brick_slabx2", "stone_brick_stairsx1",
                                            "stone_brick_wallx1", "stone_bricksx1", "stone_slabx2", "stone_stairsx1"});
    const auto copper = ids("copper_block");
    CHECK(std::find(copper.begin(), copper.end(), "cut_copper_slabx8") != copper.end());
    CHECK(ids("oak_planks").empty()); // wood isn't cut here
    Fixture f;
    f.inv.setSlot(0, I("stone", 3));
    f.screen.open(ContainerScreen::Type::Stonecutter);
    f.left(invX(0), hotbarY()); // carry the stone
    f.left(sx(20), sy(33));     // into the input
    CHECK(f.screen.result().empty()); // nothing chosen yet
    f.left(sx(52 + 16 + 4) - 8, sy(14 + 4) - 8); // the second button: stone brick slabs
    CHECK(f.screen.stonecutterChoice() == 1);
    CHECK(f.screen.result().count == 2);
    f.left(sx(143), sy(33), true); // shift: cut all
    CHECK(f.screen.grid(0).empty());
    int slabs = 0;
    for (int i = 0; i < Inventory::kSlots; ++i)
        if (f.inv.slot(i).item == I("stone_brick_slab").item) slabs += f.inv.slot(i).count;
    CHECK(slabs == 6);
}

TEST_CASE("grindstone: strips enchantments for experience; merges two worn tools (+5%) (M23.5)") {
    ItemStack sword = I("iron_sword");
    sword.damage = 200;
    setEnchantment(sword, Enchantment::Sharpness, 3);
    sword.repairCost = 3;
    const GrindResult one = grind(sword, {});
    CHECK(one.out.item == sword.item);
    CHECK_FALSE(isEnchanted(one.out));
    CHECK(one.out.repairCost == 0);
    CHECK(one.out.damage == 200);
    CHECK(one.xpCost == enchantmentInfo(Enchantment::Sharpness).minBase + 2 * enchantmentInfo(Enchantment::Sharpness).minPerLevel);
    Xoroshiro rng(3);
    const int xp = grindExperience(one.xpCost, rng);
    CHECK(xp >= (one.xpCost + 1) / 2);
    CHECK(xp < 2 * ((one.xpCost + 1) / 2));
    CHECK(grind(I("iron_sword"), {}).out.empty()); // nothing to remove
    ItemStack book = I("enchanted_book");
    setEnchantment(book, Enchantment::Efficiency, 1);
    CHECK(grind({}, book).out.item == I("book").item);
    ItemStack worn = I("iron_sword");
    worn.damage = 240; // iron: 250 uses
    const GrindResult two = grind(sword, worn);
    CHECK(two.out.damage == 250 - (50 + 10 + 12)); // remaining 50 + 10 + 5% of 250
    CHECK(grind(sword, I("diamond_sword")).out.empty()); // must be the same item
    // Through the screen: taking the result uses both inputs and reports the cost.
    Fixture f;
    f.inv.setSlot(0, sword);
    f.screen.open(ContainerScreen::Type::Grindstone);
    f.left(invX(0), hotbarY());
    f.left(sx(49), sy(19));
    REQUIRE_FALSE(f.screen.result().empty());
    f.left(sx(129), sy(34));
    CHECK(f.screen.grid(0).empty());
    CHECK_FALSE(isEnchanted(f.screen.carried()));
    CHECK(f.screen.takeGrindCost() == one.xpCost);
}

TEST_CASE("smithing: netherite upgrades keep enchantments and wear; trims go on armor (M23.6)") {
    ItemStack pick = I("diamond_pickaxe");
    pick.damage = 300;
    setEnchantment(pick, Enchantment::Efficiency, 4);
    const ItemStack up = smith(I("netherite_upgrade_smithing_template"), pick, I("netherite_ingot"));
    CHECK(up.item == I("netherite_pickaxe").item);
    CHECK(up.damage == 300);
    CHECK(enchantLevel(up, Enchantment::Efficiency) == 4);
    CHECK(smith(I("netherite_upgrade_smithing_template"), I("iron_pickaxe"), I("netherite_ingot")).empty());
    CHECK(smith(I("netherite_upgrade_smithing_template"), pick, I("diamond")).empty());
    CHECK(tierInfo(itemRegistry().item(up.item).tier).level == 4);
    const ItemStack trimmed = smith(I("coast_armor_trim_smithing_template"), I("iron_chestplate"), I("emerald"));
    REQUIRE_FALSE(trimmed.empty());
    CHECK(int(trimmed.trim >> 8) == *findTrimPattern("coast"));
    const int trimMaterial = trimmed.trim & 0xFF;
    CHECK(trimMaterial == *findTrimMaterial("minecraft:emerald"));
    CHECK(smith(I("coast_armor_trim_smithing_template"), trimmed, I("emerald")).empty()); // already that trim
    CHECK(smith(I("coast_armor_trim_smithing_template"), I("iron_sword"), I("emerald")).empty()); // not armor
    const ItemStack back = itemFromNbtPublic(itemToNbt(trimmed, 0));
    CHECK(back.trim == trimmed.trim);
    CHECK(itemRegistry().item(I("netherite_sword").item).fireResistant);
    // Through the screen: each input is used once.
    Fixture f;
    f.inv.setSlot(0, I("netherite_upgrade_smithing_template", 2));
    f.inv.setSlot(1, pick);
    f.inv.setSlot(2, I("netherite_ingot", 3));
    f.screen.open(ContainerScreen::Type::Smithing);
    for (int i = 0; i < 3; ++i) {
        f.left(invX(i), hotbarY());
        f.left(sx(8 + 18 * i), sy(48));
    }
    REQUIRE(f.screen.result().item == I("netherite_pickaxe").item);
    f.left(sx(98), sy(48));
    CHECK(f.screen.carried().item == I("netherite_pickaxe").item);
    CHECK(f.screen.grid(0).count == 1);
    CHECK(f.screen.grid(1).empty());
    CHECK(f.screen.grid(2).count == 2);
}

#include "world/ItemContainers.h"

TEST_CASE("closing a screen with a full inventory drops whole stacks: shulker contents kept (M23 review)") {
    Fixture f;
    for (int i = 0; i < Inventory::kSlots; ++i)
        f.inv.setSlot(i, I("stone", 64));
    ItemContents slots{};
    slots[0] = I("diamond", 9);
    ItemStack box = I("red_shulker_box");
    box.contents = addItemContents(slots);
    ItemStack sword = I("iron_sword");
    setEnchantment(sword, Enchantment::Sharpness, 2);
    f.screen.open(ContainerScreen::Type::Crafting);
    f.inv.setSlot(0, box);
    f.left(invX(0), hotbarY());   // carry the box
    f.left(sx(30), sy(17));       // into the grid
    f.inv.setSlot(1, sword);
    f.left(invX(1), hotbarY());   // carry the sword
    f.inv.setSlot(0, I("stone", 64));
    f.inv.setSlot(1, I("stone", 64));
    f.screen.close(f.inv, f.drops);
    REQUIRE(f.drops.size() == 2);
    bool boxOk = false, swordOk = false;
    for (const ItemStack& d : f.drops) {
        boxOk = boxOk || (d.item == box.item && itemContents(d.contents)[0].count == 9);
        swordOk = swordOk || (d.item == sword.item && enchantLevel(d, Enchantment::Sharpness) == 2);
    }
    CHECK(boxOk);
    CHECK(swordOk);
}

#include "world/Trades.h"

TEST_CASE("trading: villagers get trades by level; choosing a trade fills the payment; trades level the villager (M24.2)") {
    MobData v;
    v.type = MobType::Villager;
    v.profession = uint8_t(Profession::Librarian);
    Xoroshiro rng(5);
    addLevelTrades(v, rng);
    CHECK(v.offerCount == 2);
    CHECK(villagerLevelFor(0) == 1);
    CHECK(villagerLevelFor(10) == 2);
    CHECK(villagerLevelFor(250) == 5);
    // Our own trade for the test: 24 paper -> 1 emerald, 16 uses, 2 xp.
    v.offerCount = 0;
    TradeOffer paper;
    paper.buyA = I("paper").item;
    paper.buyACount = 24;
    paper.sell = I("emerald").item;
    paper.sellCount = 1;
    paper.maxUses = 2;
    paper.xp = 5;
    v.offers[v.offerCount++] = paper;
    Fixture f;
    // (the trading panel is 222 high: its top sits 28 GUI px above the usual one)
    auto ty = [](int y) { return sy(y) - 28; };
    f.inv.setSlot(0, I("paper", 64));
    f.screen.openTrading(&v);
    f.left(sx(8 + 4), ty(16 + 4)); // the first offer: 24 paper moves into the first slot
    CHECK(f.screen.grid(0).count == 24);
    CHECK(f.inv.slot(0).count == 40);
    REQUIRE(f.screen.result().item == I("emerald").item);
    f.left(sx(120), ty(112)); // trade
    CHECK(f.screen.carried().item == I("emerald").item);
    CHECK(v.offers[0].uses == 1);
    CHECK(v.villagerXp == 5);
    CHECK(f.screen.takeTradeExperience() >= 3);
    // A second trade levels it up (10 xp): apprentice trades appear.
    f.left(invX(5), ty(198)); // (put the emerald down)
    f.left(sx(8 + 4), ty(16 + 4));
    f.left(sx(120), ty(112));
    CHECK(v.villagerLevel == 2);
    CHECK(v.offerCount >= 2);
    // Out of stock now (2 uses): no result until it restocks; demand then raises the price.
    f.left(sx(8 + 4), ty(16 + 4));
    CHECK(f.screen.result().empty());
    restock(v);
    CHECK(v.offers[0].uses == 0);
    CHECK(v.offers[0].demand == 2); // 0 + 2 used - 0 left
    CHECK(offerPrice(v.offers[0]) == 24 + int(24 * 0.05f * 2));
}
