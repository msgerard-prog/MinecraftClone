#include "gameplay/FlyController.h"
#include "world/Rotation.h"

#include <doctest/doctest.h>

using doctest::Approx;

TEST_CASE("lookVector follows vanilla yaw/pitch conventions") {
    const auto south = mc::world::lookVector(0, 0);
    CHECK(south.z == Approx(1.0));
    const auto west = mc::world::lookVector(90, 0);
    CHECK(west.x == Approx(-1.0));
    const auto north = mc::world::lookVector(180, 0);
    CHECK(north.z == Approx(-1.0));
    const auto down = mc::world::lookVector(0, 90);
    CHECK(down.y == Approx(-1.0));
}

TEST_CASE("facing south, west is on the player's right") {
    const auto right = mc::world::rightFlat(0);
    CHECK(right.x == Approx(-1.0));
    CHECK(right.z == Approx(0.0).epsilon(1e-6));
}

TEST_CASE("vanilla mouse sensitivity: default 0.5 is 0.15 degrees per pixel") {
    CHECK(mc::degreesPerPixel(0.5) == Approx(0.15));
}

TEST_CASE("flying forward for 20 ticks covers one second of fly speed") {
    mc::FlyController fly;
    fly.setRotation(0, 0); // facing south (+Z)
    mc::MoveInput in;
    in.forward = 1;
    for (int i = 0; i < 20; ++i)
        fly.tick(in);
    CHECK(fly.position().z == Approx(mc::FlyController::kFlySpeed));
    CHECK(fly.position().x == Approx(0.0).epsilon(1e-9));
}

TEST_CASE("diagonal flight is not faster than straight flight") {
    mc::FlyController fly;
    mc::MoveInput in;
    in.forward = 1;
    in.strafe = 1;
    fly.tick(in);
    CHECK(glm::length(fly.position()) == Approx(mc::FlyController::kFlySpeed / 20.0));
}

TEST_CASE("render position interpolates between ticks") {
    mc::FlyController fly;
    mc::MoveInput in;
    in.up = 1;
    fly.tick(in);
    const double step = mc::FlyController::kVerticalSpeed / 20.0;
    CHECK(fly.renderPosition(0.0).y == Approx(0.0));
    CHECK(fly.renderPosition(0.5).y == Approx(step / 2));
    CHECK(fly.renderPosition(1.0).y == Approx(step));
}

TEST_CASE("pitch is clamped to straight up/down") {
    mc::FlyController fly;
    fly.turn(0, 100000);
    CHECK(fly.pitch() == Approx(90.0f));
    fly.turn(0, -1000000);
    CHECK(fly.pitch() == Approx(-90.0f));
}

TEST_CASE("lookVector: yaw -90 is east, pitch -90 is straight up") {
    const auto east = mc::world::lookVector(-90, 0);
    CHECK(east.x == Approx(1.0));
    const auto up = mc::world::lookVector(0, -90);
    CHECK(up.y == Approx(1.0));
}

TEST_CASE("mouse sensitivity curve at its extremes") {
    CHECK(mc::degreesPerPixel(0.0) == Approx(0.0096));
    CHECK(mc::degreesPerPixel(1.0) == Approx(0.6144));
}

TEST_CASE("sprint-flying forward covers 21.6 blocks per second (wiki: Flying)") {
    mc::FlyController fly;
    mc::MoveInput in;
    in.forward = 1;
    in.sprint = true;
    for (int i = 0; i < 20; ++i)
        fly.tick(in);
    CHECK(fly.position().z == Approx(21.6));
}

TEST_CASE("sprint has no effect when flying backwards or sideways (wiki: Sprinting)") {
    mc::MoveInput in;
    in.sprint = true;
    in.forward = -1;
    mc::FlyController back;
    back.tick(in);
    CHECK(glm::length(back.position()) == Approx(mc::FlyController::kFlySpeed / 20.0));

    in.forward = 0;
    in.strafe = 1;
    mc::FlyController side;
    side.tick(in);
    CHECK(glm::length(side.position()) == Approx(mc::FlyController::kFlySpeed / 20.0));
}

TEST_CASE("vertical flight is 7.49 blocks per second (wiki: Transportation)") {
    mc::FlyController fly;
    mc::MoveInput in;
    in.up = 1;
    for (int i = 0; i < 20; ++i)
        fly.tick(in);
    CHECK(fly.position().y == Approx(7.49));
}
