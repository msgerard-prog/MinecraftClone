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
