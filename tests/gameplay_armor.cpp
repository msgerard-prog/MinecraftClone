// Armor and shields (wiki: Armor, Shield).
#include "gameplay/Inventory.h"
#include "gameplay/Vitals.h"
#include "world/Items.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {
ItemStack I(const char* id) { return {*itemRegistry().find(id), 1}; }
} // namespace

TEST_CASE("armor reduction: damage x (1 - min(20, max(armor/5, armor - 4 dmg/(toughness+8)))/25)") {
    // Full diamond: 20 points, 8 toughness; a 10-damage hit -> 17.5 -> x0.3.
    CHECK(Vitals::armorReduced(10.0f, 20, 8.0f) == doctest::Approx(3.0f));
    // Full iron (15, 0) against 5: 12.5 -> x0.5.
    CHECK(Vitals::armorReduced(5.0f, 15, 0.0f) == doctest::Approx(2.5f));
    // A huge hit: never below armor/5 (here 4 -> x0.84).
    CHECK(Vitals::armorReduced(100.0f, 20, 0.0f) == doctest::Approx(84.0f));
    CHECK(Vitals::armorReduced(7.0f, 0, 0.0f) == doctest::Approx(7.0f));
}

TEST_CASE("inventory armor: points, toughness, equipping from the hand, wear breaks pieces") {
    Inventory inv;
    inv.select(0);
    inv.setSlot(0, I("diamond_chestplate"));
    CHECK(inv.equipSelected());
    CHECK(inv.armor(1).item == I("diamond_chestplate").item);
    CHECK(inv.slot(0).empty());
    inv.setArmor(0, I("iron_helmet"));
    CHECK(inv.armorPoints() == 8 + 2);
    CHECK(inv.armorToughness() == doctest::Approx(2.0f));
    inv.setSlot(0, I("golden_helmet")); // swaps with the worn helmet
    CHECK(inv.equipSelected());
    CHECK(inv.slot(0).item == I("iron_helmet").item);
    Xoroshiro rng(1);
    inv.wearArmor(76, rng); // golden helmet: 77 uses
    CHECK_FALSE(inv.armor(0).empty());
    inv.wearArmor(1, rng);
    CHECK(inv.armor(0).empty());
    inv.setSlot(1, I("stick"));
    inv.select(1);
    CHECK_FALSE(inv.equipSelected());
}

TEST_CASE("attacks: armor reduces them and wears the pieces; a raised shield blocks the front") {
    Vitals v;
    v.setArmor(15, 0.0f);
    const glm::dvec3 zombie{0.0, 64.0, 3.0};
    CHECK(v.attacked(5.0f, &zombie));
    CHECK(v.health() == doctest::Approx(17.5f));
    CHECK(v.takeArmorWear() == 1); // floor(5/4)
    v.tick(64.0, true, false, false); // (invulnerability wears off over 10 ticks)
    for (int i = 0; i < 10; ++i)
        v.tick(64.0, true, false, false);
    v.setShield(true, {0.0, 65.6, 0.0}, {0.0, 0.0, 1.0}); // facing +Z, the zombie's side
    CHECK_FALSE(v.attacked(5.0f, &zombie));
    CHECK(v.takeShieldWear() == 6); // 1 + floor(5)
    const glm::dvec3 behind{0.0, 64.0, -3.0};
    CHECK(v.attacked(5.0f, &behind)); // the shield doesn't cover the back
}
