#include "core/Gamepad.h"

#include <algorithm>
#include <cmath>

namespace mc {

namespace {

// Radial dead zone, rescaled so the response starts at 0 just past it.
void deadZone(float x, float y, float& ox, float& oy) {
    const float len = std::sqrt(x * x + y * y);
    if (len < GamepadMapper::kDeadZone) {
        ox = oy = 0.0f;
        return;
    }
    const float scaled = std::min(1.0f, (len - GamepadMapper::kDeadZone) / (1.0f - GamepadMapper::kDeadZone));
    ox = x / len * scaled;
    oy = y / len * scaled;
}

} // namespace

GamepadInput GamepadMapper::update(const GamepadState& pad, bool inWorld, double dt, int screenHeight) {
    GamepadInput in;
    if (!pad.connected) {
        m_prev = GamepadState{};
        m_sprintLatched = false;
        return in;
    }
    using B = GamepadState;
    auto down = [&](int b) { return pad.buttons[size_t(b)] != 0; };
    auto pressed = [&](int b) { return down(b) && m_prev.buttons[size_t(b)] == 0; };
    auto trigger = [&](int a, const GamepadState& s) { return s.axes[size_t(a)] > kTrigger * 2.0f - 1.0f; };
    const bool rt = trigger(B::RT, pad), lt = trigger(B::LT, pad);
    const bool rtPressed = rt && !trigger(B::RT, m_prev), ltPressed = lt && !trigger(B::LT, m_prev);
    float lx, ly, rx, ry;
    deadZone(pad.axes[B::LX], pad.axes[B::LY], lx, ly);
    deadZone(pad.axes[B::RX], pad.axes[B::RY], rx, ry);

    if (pressed(B::Start)) in.escape = true; // Menu: pause / back, everywhere
    if (pressed(B::DRight)) in.chat = true;
    if (inWorld) {
        in.moveForward = -ly; // (stick up is -1)
        in.moveStrafe = lx;
        // Sprint (L3) stays on while the stick is pushed, as Bedrock's sprint toggle.
        if (pressed(B::L3)) m_sprintLatched = true;
        if (std::abs(lx) + std::abs(ly) < 0.01f) m_sprintLatched = false;
        in.sprintHeld = m_sprintLatched;
        in.jump = pressed(B::A);
        in.jumpHeld = down(B::A);
        in.sneakHeld = down(B::B) || down(B::R3);
        in.attackHeld = rt;
        in.useHeld = lt;
        in.leftClick = rtPressed;
        in.rightClick = ltPressed;
        in.inventory = pressed(B::Y);
        in.crafting = pressed(B::X);
        in.drop = pressed(B::DDown);
        in.perspective = pressed(B::DUp);
        if (pressed(B::LB)) in.scroll += 1;
        if (pressed(B::RB)) in.scroll -= 1;
        auto curve = [](float v) { return double(v) * std::abs(double(v)); };
        in.lookDx = curve(rx) * kLookPixelsPerSecond * dt;
        in.lookDy = curve(ry) * kLookPixelsPerSecond * dt;
    } else {
        // Screens: the left stick drives the cursor (squared for fine placement); A, X and
        // Y click as Bedrock's controller inventory does; B goes back.
        auto curve = [](float v) { return double(v) * std::abs(double(v)); };
        const double speed = double(std::max(screenHeight, 240)) * 1.1; // pixels a second at full tilt
        in.cursorDx = curve(lx) * speed * dt;
        in.cursorDy = curve(ly) * speed * dt;
        in.leftClick = pressed(B::A) || rtPressed;
        in.rightClick = pressed(B::X) || ltPressed;
        in.quickMove = pressed(B::Y);
        if (pressed(B::B)) in.escape = true;
        in.drop = pressed(B::DDown);
        m_scrollAccum += -double(ry) * 8.0 * dt; // (stick up = wheel up), about 8 steps a second
        const double steps = std::trunc(m_scrollAccum);
        in.screenScroll = steps;
        m_scrollAccum -= steps;
        if (pressed(B::LB)) in.scroll += 1; // tabs or the hotbar, like a wheel step
        if (pressed(B::RB)) in.scroll -= 1;
        m_sprintLatched = false;
    }
    m_prev = pad;
    return in;
}

} // namespace mc
