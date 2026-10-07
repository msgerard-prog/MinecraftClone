// Container screens: slot clicks and crafting (wiki: Inventory › Controls, Crafting).
#include "ui/ContainerScreen.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

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
