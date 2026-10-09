// Effects and potions of M29.2a (wiki: Absorption, Health Boost, Saturation, Potion, Brewing).
#include "gameplay/Brewing.h"
#include "gameplay/Vitals.h"
#include "world/Items.h"
#include "world/Potions.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

TEST_CASE("absorption's golden health is taken first and goes when the effect ends") {
    Vitals v;
    v.addEffect(Effect::Absorption, 1, 40); // 8 golden health
    CHECK(v.absorption() == doctest::Approx(8.0f));
    CHECK(v.attacked(5.0f));
    CHECK(v.health() == doctest::Approx(20.0f));
    CHECK(v.absorption() == doctest::Approx(3.0f));
    for (int i = 0; i < 41; ++i) v.tickEffects();
    CHECK(v.absorption() == 0.0f);
}

TEST_CASE("health boost adds 4 a level; saturation fills food") {
    Vitals v;
    v.addEffect(Effect::HealthBoost, 1, 100);
    CHECK(v.maxHealth() == doctest::Approx(28.0f));
    v.setHealth(27.0f);
    CHECK(v.health() == doctest::Approx(27.0f));
    for (int i = 0; i < 101; ++i) v.tickEffects();
    CHECK(v.health() == doctest::Approx(20.0f)); // (its hearts go with it)
    Vitals h;
    h.setState(20.0f, 4, 0.0f, 0.0f);
    h.addEffect(Effect::Saturation, 0, 5);
    for (int i = 0; i < 5; ++i) h.tickEffects();
    CHECK(h.food() == 9);
}

TEST_CASE("turtle master: brewed from a turtle shell, two effects; the 1.21 potions brew too") {
    ItemStack awkward{*itemRegistry().find("potion"), 1};
    awkward.potion = uint8_t(Potion::Awkward);
    const auto tm = brewResult({*itemRegistry().find("turtle_helmet"), 1}, awkward);
    REQUIRE(tm);
    CHECK(tm->potion == uint8_t(Potion::TurtleMaster));
    const PotionInfo& info = potionInfo(Potion::TurtleMaster);
    CHECK(info.effect == Effect::Slowness);
    CHECK(info.amplifier == 3);
    CHECK(info.effect2 == Effect::Resistance);
    CHECK(info.amplifier2 == 2);
    for (const auto& [ingredient, potion] : {std::pair{"breeze_rod", Potion::WindCharging}, std::pair{"cobweb", Potion::Weaving},
                                             std::pair{"slime_block", Potion::Oozing}, std::pair{"stone", Potion::Infestation}}) {
        const auto r = brewResult({*itemRegistry().find(ingredient), 1}, awkward);
        REQUIRE(r);
        CHECK(r->potion == uint8_t(potion));
    }
    CHECK(findPotion("minecraft:luck") == Potion::Luck);
    CHECK(findEffect("minecraft:wind_charged") == Effect::WindCharged);
}

// M29.2b: enchantments and what they need (wiki: Frosted Ice, Ice, Mending...).
#include "gameplay/Player.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Enchantments.h"
#include "world/World.h"

TEST_CASE("the M29.2b enchantments fit their items; treasures never come from the table") {
    const auto& items = itemRegistry();
    CHECK(canEnchant(*items.find("diamond_boots"), Enchantment::DepthStrider));
    CHECK(canEnchant(*items.find("diamond_boots"), Enchantment::FrostWalker));
    CHECK(canEnchant(*items.find("iron_sword"), Enchantment::SweepingEdge));
    CHECK(canEnchant(*items.find("iron_pickaxe"), Enchantment::Mending));
    CHECK(canEnchant(*items.find("iron_helmet"), Enchantment::BindingCurse));
    CHECK_FALSE(canEnchant(*items.find("iron_sword"), Enchantment::BindingCurse));
    CHECK(enchantmentInfo(Enchantment::Mending).weight == 0);
    CHECK(findEnchantment("minecraft:vanishing_curse") == Enchantment::VanishingCurse);
}

TEST_CASE("ice and frosted ice leave water when broken; ice is slippery") {
    const auto& r = blockRegistry();
    CHECK(leftAfterBreaking(r.defaultState(blocks::Ice)) == r.defaultState(blocks::Water));
    CHECK(leftAfterBreaking(r.defaultState(blocks::FrostedIce)) == r.defaultState(blocks::Water));
    CHECK(leftAfterBreaking(r.defaultState(blocks::Stone)) == 0);
    CHECK(Player::slipperinessOf(blocks::Ice) == doctest::Approx(0.98));
    CHECK(Player::slipperinessOf(blocks::BlueIce) == doctest::Approx(0.989));
    CHECK(Player::slipperinessOf(blocks::Stone) == doctest::Approx(0.6));
    CHECK_FALSE(itemRegistry().find("frosted_ice").has_value()); // (no item, as vanilla)
}

TEST_CASE("frosted ice ages on its scheduled ticks and melts back into water") {
    World world;
    world.createChunk({0, 0});
    BlockUpdates updates(world);
    world.setListener(&updates);
    const BlockPos p{4, 64, 4};
    world.setBlock(p, blockRegistry().defaultState(blocks::FrostedIce));
    updates.schedule(p, blocks::FrostedIce, 1, 0);
    for (int t = 0; t < 2000 && blockRegistry().blockOf(world.getBlock(p)) == blocks::FrostedIce; ++t) {
        updates.setTime(t);
        updates.tick();
    }
    CHECK(blockRegistry().blockOf(world.getBlock(p)) == blocks::Water);
}
