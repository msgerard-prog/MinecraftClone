#pragma once

#include <array>
#include <cstdint>

namespace mc {

// Controller support (v1.5.4, the user's choice; ADR 0010). Vanilla Java Edition has none;
// ours follows Bedrock Edition's default Xbox layout (wiki: Controls):
//   in game                          on screens (inventory, menus)
//   left stick   move (analog)       moves the cursor
//   L3 (click)   sprint (held until the stick lets go, as Bedrock's toggle)
//   right stick  look                scrolls (lists, creative tabs)
//   R3           fly down slow (sneak)
//   A            jump / fly up       click (pick up / place a stack)
//   B            sneak / fly down    back (Esc)
//   X            crafting (the inventory's grid)   right click (half / one)
//   Y            inventory           quick move (shift-click)
//   LB / RB      hotbar left / right
//   LT / RT      use, place / attack, destroy
//   D-pad        up perspective, down drop, right chat (left: emote - we have none)
//   Menu         pause (Esc)         View: unassigned (Bedrock: the effects drawer)
// The mapping is a pure function of the pad's state (testable without a controller);
// core/Window feeds its result into the same paths keyboard and mouse use.

// One controller's state, as GLFW's standard gamepad mapping reports it: axes -1..1
// (stick y down = +1; triggers -1 released .. +1 pulled), buttons 0/1.
struct GamepadState {
    enum Button : uint8_t { A, B, X, Y, LB, RB, Back, Start, Guide, L3, R3, DUp, DRight, DDown, DLeft, ButtonCount };
    enum Axis : uint8_t { LX, LY, RX, RY, LT, RT, AxisCount };
    bool connected = false;
    std::array<float, AxisCount> axes{};
    std::array<uint8_t, ButtonCount> buttons{};
    GamepadState() { axes[LT] = axes[RT] = -1.0f; }
};

// What the pad means this frame, in the game's own input terms.
struct GamepadInput {
    // Edges this frame (counted like key/mouse presses).
    bool jump = false, inventory = false, crafting = false, drop = false, chat = false, perspective = false;
    bool escape = false, leftClick = false, rightClick = false, quickMove = false;
    int scroll = 0; // hotbar: +1 = previous slot (like a wheel step up), -1 = next
    // Held this frame.
    bool jumpHeld = false, sneakHeld = false, sprintHeld = false, attackHeld = false, useHeld = false;
    float moveForward = 0.0f, moveStrafe = 0.0f; // analog, after the dead zone
    double lookDx = 0.0, lookDy = 0.0;           // camera turn in mouse pixels this frame
    double cursorDx = 0.0, cursorDy = 0.0;       // screen cursor movement, framebuffer pixels
    double screenScroll = 0.0;                   // wheel steps on screens (right stick)
    bool anyPressed = false;                     // any button or trigger went down this frame
};

class GamepadMapper {
public:
    // `inWorld`: the game has the mouse (no screen open). `dt`: seconds since the last call.
    // `screenHeight`: framebuffer height (the cursor crosses the screen in about a second).
    GamepadInput update(const GamepadState& pad, bool inWorld, double dt, int screenHeight);

    static constexpr float kDeadZone = 0.2f;   // sticks: radial, rescaled to 0..1 beyond it
    static constexpr float kTrigger = 0.5f;    // triggers count as pulled past half way
    // Full right-stick deflection turns the view like ~1800 mouse pixels a second (about
    // 270 degrees a second at the default sensitivity); the curve is squared for aiming.
    static constexpr double kLookPixelsPerSecond = 1800.0;

private:
    GamepadState m_prev;
    bool m_sprintLatched = false;
    double m_scrollAccum = 0.0;
};

} // namespace mc
