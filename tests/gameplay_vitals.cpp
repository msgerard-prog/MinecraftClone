// Health, hunger and fall damage (wiki: Health, Hunger, Fall damage).
#include "gameplay/Vitals.h"

#include <doctest/doctest.h>

#include <initializer_list>

using namespace mc;

namespace {

// Falls from `height` blocks (airborne each tick, then lands).
float fall(Vitals& v, double height) {
    v.tick(100.0, true, false, false);
    float hurt = 0.0f;
    for (double y = 100.0; y > 100.0 - height; y -= 0.5)
        hurt += v.tick(y, false, false, false);
    hurt += v.tick(100.0 - height, true, false, false);
    return hurt;
}

} // namespace

TEST_CASE("fall damage: 1 per block beyond 3, none in water or flying") {
    Vitals a, b, c, d;
    CHECK(fall(a, 3.0) == 0.0f);
    CHECK(fall(b, 4.0) == 1.0f);
    CHECK(fall(c, 10.0) == 7.0f);
    CHECK(c.health() == 13.0f);
    // A jump on flat ground (up 1.25, back down) hurts nothing.
    d.tick(64.0, true, false, false);
    for (double y : {64.4, 65.0, 65.25, 64.9, 64.3})
        d.tick(y, false, false, false);
    CHECK(d.tick(64.0, true, false, false) == 0.0f);
    Vitals w;
    w.tick(100.0, true, false, false);
    for (double y = 100.0; y > 80.0; y -= 1.0)
        w.tick(y, false, y < 85.0, false); // lands in water
    CHECK(w.tick(80.0, true, false, false) == 0.0f);
}

TEST_CASE("exhaustion drains saturation, then food; 4 exhaustion = 1 point") {
    Vitals v; // 20 food, 5 saturation
    v.exhaust(4.0f * 5.0f);
    v.tick(64, true, false, false);
    CHECK(v.saturation() == 0.0f);
    CHECK(v.food() == 20);
    v.exhaust(4.0f * 3.0f);
    v.tick(64, true, false, false);
    CHECK(v.food() == 17);
    CHECK_FALSE(Vitals().canSprint() == false);
}

TEST_CASE("regeneration: fast at full food with saturation, slow at 18+, none below") {
    Vitals fast;
    fast.setState(10.0f, 20, 5.0f, 0.0f);
    for (int i = 0; i < 10; ++i)
        fast.tick(64, true, false, false);
    CHECK(fast.health() > 10.0f); // 10 ticks
    Vitals slow;
    slow.setState(10.0f, 18, 0.0f, 0.0f);
    for (int i = 0; i < 79; ++i)
        slow.tick(64, true, false, false);
    CHECK(slow.health() == 10.0f);
    slow.tick(64, true, false, false);
    CHECK(slow.health() == 11.0f); // 80 ticks
    Vitals hungry;
    hungry.setState(10.0f, 17, 0.0f, 0.0f);
    for (int i = 0; i < 200; ++i)
        hungry.tick(64, true, false, false);
    CHECK(hungry.health() == 10.0f);
}

TEST_CASE("starving hurts every 80 ticks down to half a heart (normal difficulty)") {
    Vitals v;
    v.setState(3.0f, 0, 0.0f, 0.0f);
    for (int i = 0; i < 80 * 5; ++i)
        v.tick(64, true, false, false);
    CHECK(v.health() == 1.0f);
    CHECK_FALSE(v.canSprint());
}

TEST_CASE("damage, invulnerability, death, eating and respawn") {
    Vitals v;
    CHECK(v.damage(5.0f));
    CHECK_FALSE(v.damage(5.0f)); // 10 invulnerable ticks
    for (int i = 0; i < 10; ++i)
        v.tick(64, true, false, false);
    CHECK(v.damage(30.0f));
    CHECK(v.dead());
    v.reset();
    CHECK(v.health() == 20.0f);
    v.setState(20.0f, 10, 0.0f, 0.0f);
    v.eat(4, 2.4f); // apple
    CHECK(v.food() == 14);
    CHECK(v.saturation() == doctest::Approx(2.4f));
}

TEST_CASE("a teleport or game-mode change is not a fall (resetFall)") {
    // Regression: /tp from high ground measured the fall from the old height.
    Vitals v;
    v.tick(200.0, true, false, false);
    v.resetFall();
    v.tick(70.5, false, false, false); // just above the new ground
    CHECK(v.tick(70.0, true, false, false) == 0.0f);
    CHECK(v.health() == 20.0f);
}
