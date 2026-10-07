// Player physics against the wiki's published numbers (blocks per second etc.).
#include "gameplay/Player.h"
#include "world/BlockUpdates.h"
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

TEST_CASE("ground speeds: walk 4.317, sprint 5.612 (wiki), sneak 1.295 (wiki rounds to 1.3)") {
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

TEST_CASE(
    "creative flight: double-tap jump, 10.92 b/s (wiki), 7.5 b/s up (derived), landing ends it") {
    const World w = floorWorld();
    Player p = standingPlayer(w);
    PlayerInput jump;
    jump.jump = true;
    PlayerInput press = jump;
    press.jumpPresses = 1;
    PlayerInput none;
    p.tick(w, press);
    p.tick(w, none);
    p.tick(w, press); // second press within 7 ticks
    CHECK(p.flying());
    // Rise for a second: 7.5 b/s, derived from our (unverified) flight constants.
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

TEST_CASE("walls hold from any fractional or negative starting position (collision epsilon)") {
    World w = floorWorld();
    const auto stone = world::blockRegistry().defaultState(world::blocks::Stone);
    for (int x = -40; x <= 40; ++x) {
        for (int y = kFloorY + 1; y <= kFloorY + 3; ++y) {
            w.setBlock({x, y, -2}, stone); // wall: z in [-2, -1)
            w.setBlock({x, y, 20}, stone); // wall: z in [20, 21)
        }
    }
    uint64_t seed = 12345;
    auto rnd = [&]() { // tiny LCG: deterministic test positions
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(seed >> 11) * 0x1.0p-53;
    };
    for (int trial = 0; trial < 200; ++trial) {
        Player p;
        const double x = -30.0 + 60.0 * rnd();
        const double z = 2.0 + 10.0 * rnd();
        p.setPosition({x, kFloorY + 1.0, z});
        const bool north = trial % 2 == 0;
        p.setRotation(north ? 180.0f : 0.0f, 0.0f); // walk -z or +z into a wall
        PlayerInput run;
        run.forward = 1;
        run.sprint = true;
        for (int i = 0; i < 120; ++i)
            p.tick(w, run);
        INFO("trial ", trial, " start ", x, ", ", z);
        if (north) {
            CHECK(p.position().z >= -1.0 + Player::kWidth / 2 - 1e-6);
        } else {
            CHECK(p.position().z <= 20.0 - Player::kWidth / 2 + 1e-6);
        }
    }
}

TEST_CASE("releasing sneak only stands up when there is headroom") {
    // With full blocks only, a 1.5-1.8 gap needs fractional feet: put the sneaking
    // player at y 65.25 (as if on a 1/4 slab) under a roof at y 67 (1.75 headroom).
    World w = floorWorld();
    const auto stone = world::blockRegistry().defaultState(world::blocks::Stone);
    w.setBlock({0, kFloorY + 1, 0}, stone); // the "slab" support (top at 66)
    w.setBlock({0, kFloorY + 3, 0}, stone); // roof bottom at y 67
    Player p;
    p.setPosition({0.5, kFloorY + 2.0, 0.5}); // feet on the support, roof 1.0 above
    PlayerInput sneak;
    sneak.sneak = true;
    for (int i = 0; i < 3; ++i)
        p.tick(w, sneak);
    REQUIRE(p.sneaking());
    p.tick(w, {}); // let go: 1.0 headroom < 1.8, must stay crouched
    CHECK(p.sneaking());
    w.setBlock({0, kFloorY + 3, 0}, 0); // remove the roof
    p.tick(w, {});
    CHECK_FALSE(p.sneaking());
}

TEST_CASE("falling into a one-block ledge does not climb it") {
    World w = floorWorld();
    const auto stone = world::blockRegistry().defaultState(world::blocks::Stone);
    for (int x = -3; x <= 3; ++x)
        w.setBlock({x, kFloorY + 1, 3}, stone);
    Player p;
    p.setPosition({0.5, kFloorY + 1.7, 2.0}); // falling, pressed against the ledge
    PlayerInput walk;
    walk.forward = 1;
    for (int i = 0; i < 40; ++i)
        p.tick(w, walk);
    CHECK(p.position().y == Approx(kFloorY + 1.0));
    CHECK(p.position().z <= 3.0 - Player::kWidth / 2 + 1e-6);
}

TEST_CASE("diagonal sneaking is faster (wiki: Transportation, 1.83 m/s)") {
    const World w = floorWorld();
    const Player p = standingPlayer(w);
    PlayerInput in;
    in.forward = 1;
    in.strafe = 1;
    in.sneak = true;
    CHECK(steadySpeed(w, p, in) == Approx(1.83).epsilon(0.01));
}

TEST_CASE("momentum below 0.003 stops completely; holding jump re-jumps every 10 ticks") {
    const World w = floorWorld();
    Player p = standingPlayer(w);
    PlayerInput walk;
    walk.forward = 1;
    for (int i = 0; i < 20; ++i)
        p.tick(w, walk);
    for (int i = 0; i < 20; ++i)
        p.tick(w, {});
    CHECK(p.velocity().x == 0.0);
    CHECK(p.velocity().z == 0.0);
    // Under a 2-block roof each jump lands quickly; with jump held, the next jump
    // waits for the 10-tick delay.
    World roofed = floorWorld();
    const auto stone = world::blockRegistry().defaultState(world::blocks::Stone);
    roofed.setBlock({0, kFloorY + 3, 0}, stone);
    Player q;
    q.setPosition({0.5, kFloorY + 1.0, 0.5});
    for (int i = 0; i < 3; ++i)
        q.tick(roofed, {});
    PlayerInput jump;
    jump.jump = true;
    int jumps = 0;
    bool wasGround = true;
    for (int i = 0; i < 30; ++i) {
        q.tick(roofed, jump);
        if (wasGround && !q.onGround()) ++jumps;
        wasGround = q.onGround();
    }
    CHECK(jumps <= 3); // without the delay it would jump every few ticks
}

TEST_CASE("sprinting survives glancing wall contact but stops on a head-on hit") {
    World w = floorWorld();
    const auto stone = world::blockRegistry().defaultState(world::blocks::Stone);
    for (int z = -40; z <= 40; ++z)
        w.setBlock({2, kFloorY + 1, z}, stone); // wall at x = 2
    auto run = [&](float yaw) {
        Player p;
        p.setPosition({1.69, kFloorY + 1.0, 0.5}); // touching the wall (x max = 1.99)
        for (int i = 0; i < 3; ++i)
            p.tick(w, {});
        p.setRotation(yaw, 0.0f);
        PlayerInput in;
        in.forward = 1;
        in.sprint = true;
        for (int i = 0; i < 10; ++i)
            p.tick(w, in);
        return p.sprinting();
    };
    CHECK(run(-5.0f));        // south, 5 degrees into the wall (east): keeps sprinting
    CHECK_FALSE(run(-20.0f)); // 20 degrees into the wall: stops
}

TEST_CASE("sprinting into a sneak keeps the sprint: a faster sneak (1.21.5+)") {
    const World w = floorWorld();
    Player p;
    p.setPosition({0.5, kFloorY + 1.0, 0.5});
    PlayerInput run;
    run.forward = 1;
    run.sprint = true;
    for (int i = 0; i < 20; ++i)
        p.tick(w, run);
    REQUIRE(p.sprinting());
    PlayerInput sneakRun = run;
    sneakRun.sneak = true;
    CHECK(steadySpeed(w, p, sneakRun) == Approx(5.612 * 0.3).epsilon(0.02));
}

TEST_CASE("hunger ends a sprint that is already running (wiki: Sprinting - food 6 or less)") {
    const World w = floorWorld();
    Player p;
    p.setPosition({0.5, kFloorY + 1.0, 0.5});
    PlayerInput run;
    run.forward = 1;
    run.sprint = true;
    for (int i = 0; i < 5; ++i)
        p.tick(w, run);
    REQUIRE(p.sprinting());
    run.canSprint = false;
    p.tick(w, run);
    CHECK_FALSE(p.sprinting());
}

namespace {
// A 3-deep pool of still water (sources) over the floor, 40 x 40 around the origin.
World poolWorld() {
    World w = floorWorld();
    const auto water = world::blockRegistry().defaultState(world::blocks::Water);
    for (int z = -20; z < 20; ++z)
        for (int x = -20; x < 20; ++x)
            for (int y = kFloorY + 1; y <= kFloorY + 3; ++y)
                w.setBlock({x, y, z}, water);
    return w;
}
} // namespace

TEST_CASE("swimming: about 1.96 b/s through water (wiki 1.97), jump rises, the player sinks otherwise") {
    const World w = poolWorld();
    Player p;
    p.setPosition({0.5, kFloorY + 1.5, 0.5});
    p.setCreative(false);
    PlayerInput swim;
    swim.forward = 1;
    CHECK(steadySpeed(w, p, swim) == Approx(1.96).epsilon(0.05));
    p.tick(w, {});
    CHECK(p.inWater());
    PlayerInput up;
    up.jump = true;
    const double y0 = p.position().y;
    for (int i = 0; i < 10; ++i)
        p.tick(w, up);
    CHECK(p.position().y > y0);
    const double y1 = p.position().y;
    for (int i = 0; i < 10; ++i)
        p.tick(w, {});
    CHECK(p.position().y < y1);
}

TEST_CASE("a water current pushes the player toward lower water") {
    World w = floorWorld();
    // A strip of flowing water from level 1 (west) to 7 (east): the current runs east.
    for (int x = 0; x < 7; ++x)
        w.setBlock({x, kFloorY + 1, 0}, world::BlockUpdates::fluidState(world::blocks::Water, 7 - x, false));
    Player p;
    p.setPosition({2.5, kFloorY + 1.0, 0.5});
    for (int i = 0; i < 20; ++i)
        p.tick(w, {});
    CHECK(p.position().x > 2.6);
}

TEST_CASE("swimming into a ledge hops out of the water") {
    World w = poolWorld();
    for (int z = -20; z < 20; ++z) // the pool's east side is solid: a ledge, top at y 68
        for (int x = 3; x < 20; ++x)
            for (int y = kFloorY + 1; y <= kFloorY + 3; ++y)
                w.setBlock({x, y, z}, world::blockRegistry().defaultState(world::blocks::Stone));
    Player p;
    p.setPosition({1.5, kFloorY + 3.2, 0.5}); // at the surface
    p.setRotation(-90.0f, 0.0f);              // facing east (+X)
    PlayerInput swim;
    swim.forward = 1;
    swim.jump = true;
    for (int i = 0; i < 60; ++i)
        p.tick(w, swim);
    CHECK(p.position().x > 3.0);
    CHECK(p.position().y >= kFloorY + 4.0 - 1e-6);
}
