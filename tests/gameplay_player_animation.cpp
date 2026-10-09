// M30.1: the player's body animation and the third-person camera.
#include "gameplay/PlayerAnimation.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

TEST_CASE("player animation: walking swings the limbs and turns the body to the walking direction") {
    PlayerAnimation a;
    glm::dvec3 p(0.0);
    for (int t = 0; t < 20; ++t) { // walking west (-X) at 0.2 a tick, looking west (yaw 90)
        const glm::dvec3 prev = p;
        p.x -= 0.2;
        a.tick(p, prev, 90.0f, true, 0);
    }
    CHECK(a.limbAmount(1.0f) > 0.7f);
    CHECK(a.limbSwing(1.0f) > 10.0f);
    CHECK(a.bodyYaw(1.0f) == doctest::Approx(90.0f).epsilon(0.05));
    CHECK(a.bobAmount(1.0f) > 0.08f); // (on the ground)
    // Standing still, the limbs settle; the body follows a head turned past 50 degrees.
    for (int t = 0; t < 30; ++t) a.tick(p, p, 180.0f, true, 0);
    CHECK(a.limbAmount(1.0f) < 0.01f);
    CHECK(a.bodyYaw(1.0f) == doctest::Approx(130.0f).epsilon(0.01));
    PlayerAnimation fresh; // the body starts facing the head's way
    fresh.tick(p, p, 200.0f, true, 0);
    CHECK(fresh.bodyYaw(1.0f) == doctest::Approx(200.0f));
}

TEST_CASE("player animation: a swing runs 6 ticks; switching items dips the hand") {
    PlayerAnimation a;
    const glm::dvec3 p(0.0);
    a.tick(p, p, 0.0f, true, 1);
    for (int t = 0; t < 4; ++t) a.tick(p, p, 0.0f, true, 1);
    CHECK(a.equip(1.0f) == doctest::Approx(1.0f));
    CHECK(a.swing(1.0f) == 0.0f);
    a.startSwing();
    a.tick(p, p, 0.0f, true, 1);
    CHECK(a.swing(1.0f) > 0.0f);
    for (int t = 0; t < PlayerAnimation::kSwingTicks; ++t) a.tick(p, p, 0.0f, true, 1);
    CHECK(a.swing(1.0f) == 0.0f);
    a.tick(p, p, 0.0f, true, 2); // another item
    CHECK(a.equip(1.0f) == 0.0f);
    a.tick(p, p, 0.0f, true, 2);
    a.tick(p, p, 0.0f, true, 2);
    a.tick(p, p, 0.0f, true, 2);
    CHECK(a.equip(1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("third person: the camera backs up to 4 blocks, stopping short of walls") {
    World w;
    w.createChunk({0, 0});
    const glm::dvec3 eye(8.5, 65.6, 8.5);
    CHECK(thirdPersonDistance(w, eye, {0.0, 0.0, -1.0}) == doctest::Approx(4.0));
    w.setBlock({8, 65, 6}, blockRegistry().defaultState(blocks::Stone)); // a wall 2 blocks behind
    w.setBlock({8, 66, 6}, blockRegistry().defaultState(blocks::Stone));
    w.setBlock({7, 65, 6}, blockRegistry().defaultState(blocks::Stone));
    w.setBlock({9, 65, 6}, blockRegistry().defaultState(blocks::Stone));
    const double d = thirdPersonDistance(w, eye, {0.0, 0.0, -1.0});
    CHECK(d < 1.5);
    CHECK(d > 1.0);
}
