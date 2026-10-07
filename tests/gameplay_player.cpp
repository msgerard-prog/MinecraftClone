// Player physics against the wiki's published numbers (blocks per second etc.).
#include "gameplay/Player.h"
#include "world/Blocks.h"
#include "world/Rotation.h"

#include <doctest/doctest.h>

using doctest::Approx;
using namespace mc;
using mc::world::World;

namespace {

constexpr int kFloorY = 64; // top surface of the floor is y = 65

// 13x13 chunks (+-104 blocks: room for a few seconds of sprint-flight) with a stone
// floor at y = 64.
World floorWorld() {
    World w;
    const auto stone = world::blockRegistry().defaultState(world::blocks::Stone);
    for (int cz = -6; cz <= 6; ++cz)
        for (int cx = -6; cx <= 6; ++cx) {
            auto& c = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    c.set(x, kFloorY, z, stone);
        }
    return w;
}

Player standingPlayer(const World& w) {
    Player p;
    p.setPosition({0.5, kFloorY + 1.0, 0.5});
    // Like vanilla, "on ground" is set by a downward move that collides: settle first.
    for (int i = 0; i < 3; ++i)
        p.tick(w, {});
    return p;
}

// Average horizontal speed (blocks/s) over ticks 60..80 of holding `in` (steady state).
double steadySpeed(const World& w, Player p, PlayerInput in) {
    for (int i = 0; i < 60; ++i)
        p.tick(w, in);
    const glm::dvec3 a = p.position();
    for (int i = 0; i < 20; ++i)
        p.tick(w, in);
    const glm::dvec3 d = p.position() - a;
    return std::sqrt(d.x * d.x + d.z * d.z); // over 20 ticks = 1 second
}

} // namespace

TEST_CASE("falls under gravity and lands on the floor") {
    const World w = floorWorld();
    Player p;
    p.setPosition({0.5, kFloorY + 10.0, 0.5});
    for (int i = 0; i < 60; ++i)
        p.tick(w, {});
    CHECK(p.onGround());
    CHECK(p.position().y == Approx(kFloorY + 1.0));
    CHECK(p.velocity().y == Approx(-0.0784).epsilon(0.01)); // one tick of gravity, re-zeroed
}

TEST_CASE("ground speeds match the wiki: walk 4.317, sprint 5.612, sneak 1.295 m/s") {
    const World w = floorWorld();
    const Player p = standingPlayer(w);
    PlayerInput walk;
    walk.forward = 1;
    CHECK(steadySpeed(w, p, walk) == Approx(4.317).epsilon(0.005));
    PlayerInput sprint = walk;
    sprint.sprint = true;
    CHECK(steadySpeed(w, p, sprint) == Approx(5.612).epsilon(0.005));
    PlayerInput sneak = walk;
    sneak.sneak = true;
    CHECK(steadySpeed(w, p, sneak) == Approx(1.295).epsilon(0.01));
}

TEST_CASE("a jump peaks about 1.25 blocks high (wiki: Jumping)") {
    const World w = floorWorld();
    Player p = standingPlayer(w);
    PlayerInput jump;
    jump.jump = true;
    double top = 0;
    p.tick(w, jump);
    PlayerInput none;
    for (int i = 0; i < 20; ++i) {
        top = std::max(top, p.position().y - (kFloorY + 1.0));
        p.tick(w, none);
    }
    CHECK(top == Approx(1.2522).epsilon(0.01));
    CHECK(p.onGround());
}

TEST_CASE("walls stop movement; one-block ledges are not stepped onto") {
    World w = floorWorld();
    const auto stone = world::blockRegistry().defaultState(world::blocks::Stone);
    for (int x = -3; x <= 3; ++x)
        w.setBlock({x, kFloorY + 1, 3}, stone); // wall at z = 3
    Player p = standingPlayer(w);
    PlayerInput walk;
    walk.forward = 1; // yaw 0 = south (+z)
    for (int i = 0; i < 60; ++i)
        p.tick(w, walk);
    CHECK(p.position().z == Approx(3.0 - Player::kWidth / 2));
    CHECK(p.position().y == Approx(kFloorY + 1.0)); // didn't climb a full block
}

TEST_CASE("sneaking keeps you from walking off an edge") {
    World w = floorWorld();
    for (int x = -8; x < 8; ++x)
        for (int z = 3; z < 16; ++z)
            w.setBlock({x, kFloorY, z}, 0); // floor ends at z = 3
    Player p = standingPlayer(w);
    PlayerInput sneakWalk;
    sneakWalk.forward = 1;
    sneakWalk.sneak = true;
    for (int i = 0; i < 100; ++i)
        p.tick(w, sneakWalk);
    CHECK(p.onGround());
    CHECK(p.position().y == Approx(kFloorY + 1.0));
    CHECK(p.position().z < 3.0 + Player::kWidth / 2); // still overlapping the last block
    CHECK(p.position().z > 2.5);
    PlayerInput walk;
    walk.forward = 1;
    for (int i = 0; i < 20; ++i)
        p.tick(w, walk);
    CHECK(p.position().y < kFloorY + 1.0); // without sneaking: off the edge
}

TEST_CASE("creative flight: double-tap jump, 10.92 / 7.5 b/s, landing ends it") {
    const World w = floorWorld();
    Player p = standingPlayer(w);
    PlayerInput jump;
    jump.jump = true;
    PlayerInput none;
    p.tick(w, jump);
    p.tick(w, none);
    p.tick(w, jump); // second press within 7 ticks
    CHECK(p.flying());
    // Rise for a second: vertical flight ~7.5 b/s (wiki: Transportation 7.49).
    for (int i = 0; i < 20; ++i)
        p.tick(w, jump);
    const double y0 = p.position().y;
    for (int i = 0; i < 20; ++i)
        p.tick(w, jump);
    CHECK(p.position().y - y0 == Approx(7.5).epsilon(0.01));
    PlayerInput fly;
    fly.forward = 1;
    CHECK(steadySpeed(w, p, fly) == Approx(10.92).epsilon(0.01));
    fly.sprint = true;
    CHECK(steadySpeed(w, p, fly) == Approx(21.6).epsilon(0.02));
    PlayerInput down;
    down.sneak = true;
    for (int i = 0; i < 200 && p.flying(); ++i)
        p.tick(w, down);
    CHECK_FALSE(p.flying());
    CHECK(p.onGround());
}

TEST_CASE("player waits while its chunk is not loaded") {
    World empty;
    Player p;
    p.setPosition({0.5, 100.0, 0.5});
    for (int i = 0; i < 20; ++i)
        p.tick(empty, {});
    CHECK(p.position().y == Approx(100.0));
}

TEST_CASE("lookVector follows vanilla yaw/pitch conventions") {
    CHECK(mc::world::lookVector(0, 0).z == Approx(1.0));
    CHECK(mc::world::lookVector(90, 0).x == Approx(-1.0));
    CHECK(mc::world::lookVector(-90, 0).x == Approx(1.0));
    CHECK(mc::world::lookVector(0, 90).y == Approx(-1.0));
    CHECK(mc::world::rightFlat(0).x == Approx(-1.0));
}

TEST_CASE("mouse sensitivity curve (0.15 deg/px at the default 0.5)") {
    CHECK(mc::degreesPerPixel(0.5) == Approx(0.15));
    CHECK(mc::degreesPerPixel(0.0) == Approx(0.0096));
    CHECK(mc::degreesPerPixel(1.0) == Approx(0.6144));
    Player p;
    p.turn(0, 100000);
    CHECK(p.pitch() == Approx(90.0f));
}
