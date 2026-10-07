// Enchantment effects (wiki: Efficiency, Silk Touch, Fortune, Unbreaking, Protection,
// Feather Falling, Infinity).
#include "gameplay/Mining.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/Enchantments.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {
ItemStack I(const char* id, int n = 1) { return {*itemRegistry().find(id), uint8_t(n)}; }
ItemStack with(ItemStack s, Enchantment e, int lvl) {
    setEnchantment(s, e, lvl);
    return s;
}
} // namespace

TEST_CASE("enchantments: lookup, fitting, stacking on an item") {
    CHECK(findEnchantment("sharpness") == Enchantment::Sharpness);
    CHECK(findEnchantment("minecraft:fortune") == Enchantment::Fortune);
    CHECK(canEnchant(I("diamond_sword").item, Enchantment::Sharpness));
    CHECK_FALSE(canEnchant(I("diamond_sword").item, Enchantment::Efficiency));
    CHECK(canEnchant(I("iron_boots").item, Enchantment::FeatherFalling));
    CHECK_FALSE(canEnchant(I("iron_helmet").item, Enchantment::FeatherFalling));
    CHECK(enchantability(I("golden_helmet").item) == 25);
    CHECK(enchantability(I("diamond_pickaxe").item) == 10);
    ItemStack s = with(I("iron_pickaxe"), Enchantment::Efficiency, 3);
    CHECK(enchantLevel(s, Enchantment::Efficiency) == 3);
    CHECK_FALSE(s.sameKind(I("iron_pickaxe")));
}

TEST_CASE("Efficiency mines faster; Silk Touch keeps the ore; Fortune multiplies drops") {
    const BlockStateId stone = blockRegistry().defaultState(blocks::Stone);
    const int plain = breakTicks(stone, I("iron_pickaxe"), true, false);
    const int fast = breakTicks(stone, with(I("iron_pickaxe"), Enchantment::Efficiency, 5), true, false);
    CHECK(fast < plain);
    Xoroshiro rng(1);
    std::vector<ItemStack> out;
    const BlockStateId ore = blockRegistry().defaultState(blocks::DiamondOre);
    blockDrops(ore, with(I("iron_pickaxe"), Enchantment::SilkTouch, 1), rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == itemRegistry().blockItem(blocks::DiamondOre));
    int total = 0;
    for (int i = 0; i < 200; ++i) {
        out.clear();
        blockDrops(ore, with(I("iron_pickaxe"), Enchantment::Fortune, 3), rng, out);
        total += out[0].count;
    }
    CHECK(total > 300); // Fortune III averages 2.2 per ore
}

TEST_CASE("Unbreaking makes tools last longer") {
    Xoroshiro rng(2);
    ItemStack a = I("iron_pickaxe"), b = with(I("iron_pickaxe"), Enchantment::Unbreaking, 3);
    for (int i = 0; i < 100; ++i) {
        a = wearItem(a, 1, rng);
        b = wearItem(b, 1, rng);
    }
    CHECK(a.damage == 100);
    CHECK(b.damage < 50); // 1 in 4 uses count
}

TEST_CASE("Protection: EPF 1 per level (capped 20); Feather Falling 3 per level on falls") {
    Vitals v;
    v.setProtection(16, 0, 0, 0, 0); // Protection IV on four pieces
    CHECK(v.protectionReduced(10.0f, Vitals::Hit::Generic, false) == doctest::Approx(10.0f * (1.0f - 16.0f / 25.0f)));
    v.setProtection(0, 0, 0, 0, 4); // Feather Falling IV
    CHECK(v.protectionReduced(10.0f, Vitals::Hit::Generic, true) == doctest::Approx(10.0f * (1.0f - 12.0f / 25.0f)));
    CHECK(v.protectionReduced(10.0f, Vitals::Hit::Generic, false) == doctest::Approx(10.0f));
    v.setProtection(20, 10, 0, 0, 0);
    CHECK(v.protectionReduced(10.0f, Vitals::Hit::Fire, false) == doctest::Approx(10.0f * (1.0f - 20.0f / 25.0f)));
}

TEST_CASE("Infinity: a bow needs an arrow but doesn't use it; Power hits harder") {
    Inventory inv;
    for (int i = 0; i < Inventory::kSlots; ++i)
        inv.setSlot(i, {});
    inv.select(0);
    inv.setSlot(0, with(with(I("bow"), Enchantment::Infinity, 1), Enchantment::Power, 5));
    inv.setSlot(4, I("arrow"));
    Projectiles p;
    Xoroshiro rng(3);
    CHECK(releaseBow(inv, 20, true, {0, 65, 0}, {0, 0, 1}, p, rng));
    CHECK(inv.slot(4).count == 1);
    REQUIRE(p.items().size() == 1);
    CHECK(p.items()[0].power == 5);
    CHECK_FALSE(p.items()[0].pickup);
}
