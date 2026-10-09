#include "core/Gamepad.h"

#include <doctest/doctest.h>

using mc::GamepadMapper;
using mc::GamepadState;

namespace {
GamepadState pad() {
    GamepadState s;
    s.connected = true;
    return s;
}
} // namespace

TEST_CASE("v1.5.4 controller: Bedrock's in-game layout") {
    GamepadMapper m;
    GamepadState s = pad();
    CHECK_FALSE(m.update(s, true, 0.016, 1080).jump);
    s.buttons[GamepadState::A] = 1;
    auto in = m.update(s, true, 0.016, 1080);
    CHECK(in.jump); // A: jump (an edge)
    CHECK(in.jumpHeld);
    CHECK_FALSE(m.update(s, true, 0.016, 1080).jump); // held: no second press
    s = pad();
    s.buttons[GamepadState::B] = 1;
    CHECK(m.update(s, true, 0.016, 1080).sneakHeld); // B: sneak
    s = pad();
    s.buttons[GamepadState::Y] = 1;
    CHECK(m.update(s, true, 0.016, 1080).inventory); // Y: inventory
    s = pad();
    s.buttons[GamepadState::X] = 1;
    CHECK(m.update(s, true, 0.016, 1080).crafting); // X: crafting
    s = pad();
    s.axes[GamepadState::RT] = 1.0f;
    in = m.update(s, true, 0.016, 1080);
    CHECK(in.attackHeld); // RT: attack / destroy
    CHECK(in.leftClick);
    s = pad();
    s.axes[GamepadState::LT] = 1.0f;
    CHECK(m.update(s, true, 0.016, 1080).useHeld); // LT: use / place
    s = pad();
    s.buttons[GamepadState::RB] = 1;
    CHECK(m.update(s, true, 0.016, 1080).scroll == -1); // RB: next slot (a wheel step down)
    s = pad();
    s.buttons[GamepadState::LB] = 1;
    CHECK(m.update(s, true, 0.016, 1080).scroll == 1);
    s = pad();
    s.buttons[GamepadState::DUp] = 1;
    CHECK(m.update(s, true, 0.016, 1080).perspective);
    s = pad();
    s.buttons[GamepadState::DDown] = 1;
    CHECK(m.update(s, true, 0.016, 1080).drop);
    s = pad();
    s.buttons[GamepadState::Start] = 1;
    CHECK(m.update(s, true, 0.016, 1080).escape); // Menu: pause
}

TEST_CASE("v1.5.4 controller: sticks - dead zone, analog movement, look, sprint until the stick lets go") {
    GamepadMapper m;
    GamepadState s = pad();
    s.axes[GamepadState::LY] = -0.1f; // inside the dead zone
    CHECK(m.update(s, true, 0.016, 1080).moveForward == 0.0f);
    s.axes[GamepadState::LY] = -1.0f; // full forward (stick up is -1)
    auto in = m.update(s, true, 0.016, 1080);
    CHECK(in.moveForward == doctest::Approx(1.0f));
    s.axes[GamepadState::LY] = -0.6f;
    CHECK(m.update(s, true, 0.016, 1080).moveForward == doctest::Approx(0.5f)); // rescaled past the zone
    s.buttons[GamepadState::L3] = 1;
    CHECK(m.update(s, true, 0.016, 1080).sprintHeld);
    s.buttons[GamepadState::L3] = 0;
    CHECK(m.update(s, true, 0.016, 1080).sprintHeld); // stays on while moving
    s.axes[GamepadState::LY] = 0.0f;
    CHECK_FALSE(m.update(s, true, 0.016, 1080).sprintHeld); // the stick let go
    s.axes[GamepadState::RX] = 1.0f;
    in = m.update(s, true, 0.5, 1080);
    CHECK(in.lookDx == doctest::Approx(GamepadMapper::kLookPixelsPerSecond * 0.5));
    CHECK(in.lookDy == 0.0);
}

TEST_CASE("v1.5.4 controller: on screens the stick moves the cursor; A/X/Y click, B goes back") {
    GamepadMapper m;
    GamepadState s = pad();
    s.axes[GamepadState::LX] = 1.0f;
    auto in = m.update(s, false, 1.0, 1000);
    CHECK(in.cursorDx == doctest::Approx(1100.0));
    CHECK(in.moveStrafe == 0.0f); // (no walking behind a screen)
    s = pad();
    s.buttons[GamepadState::A] = 1;
    CHECK(m.update(s, false, 0.016, 1000).leftClick);
    s = pad();
    s.buttons[GamepadState::X] = 1;
    CHECK(m.update(s, false, 0.016, 1000).rightClick);
    s = pad();
    s.buttons[GamepadState::Y] = 1;
    CHECK(m.update(s, false, 0.016, 1000).quickMove);
    s = pad();
    s.buttons[GamepadState::B] = 1;
    in = m.update(s, false, 0.016, 1000);
    CHECK(in.escape);
    CHECK_FALSE(in.sneakHeld);
    GamepadState none; // disconnected: nothing
    CHECK_FALSE(m.update(none, true, 0.016, 1000).escape);
}
