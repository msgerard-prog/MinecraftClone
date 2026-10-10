#pragma once

#include "rendering/GuiBatch.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace mc::ui {

// One frame of menu input, in GUI pixels (screen / GUI scale).
struct MenuInput {
    double mx = -1.0, my = -1.0;
    bool click = false;     // left button pressed this frame
    bool mouseDown = false; // left button held
    double wheel = 0.0;
    std::string_view typed; // printable characters and '\b' (backspace), in order
    bool enter = false, escape = false;
    int64_t timeMs = 0;     // for double clicks and the text cursor's blink
};

// Immediate-mode menu widgets in vanilla's look (M22.5): 20-pixel buttons with a bevel
// and a white border when hovered, sliders with a handle, text fields. Each frame a
// screen calls begin() and then its widgets in a fixed order (sliders and text fields
// are told apart by that order). GL-free: it only fills a GuiBatch.
class Menu {
public:
    void begin(gfx::GuiBatch& batch, int guiWidth, int guiHeight, const MenuInput& input);
    int width() const { return m_w; }
    int height() const { return m_h; }
    const MenuInput& input() const { return m_in; }
    gfx::GuiBatch& batch() { return *m_batch; }

    // True when clicked this frame.
    bool button(std::string_view label, float x, float y, float w, bool enabled = true, float h = 20.0f);
    // `value` 0..1, set from the mouse while dragged; true when it changed.
    bool slider(std::string_view label, float x, float y, float w, float& value);
    // A one-line field; clicking focuses it, the focused one takes typing. True when
    // the text changed.
    bool textField(std::string& text, float x, float y, float w, size_t maxLength = 32);
    void text(std::string_view s, float x, float y, uint32_t color = 0xFFFFFFFF, bool centred = false,
              float scale = 1.0f);
    // Backgrounds: a dark tiled block texture (menus over no world) or a dim veil
    // over the game (pause).
    void tiledBackground(uint16_t sprite, uint32_t tint = gfx::rgba(64, 64, 64));
    void dim();
    bool hovered(float x, float y, float w, float h) const;

    // Replaces this frame's input and returns the previous one (v1.5.6: an open drop-down
    // list draws the screen under it with no input, then takes the real input itself).
    MenuInput swapInput(const MenuInput& in) {
        const MenuInput old = m_in;
        m_in = in;
        return old;
    }

    // Focus moves with clicks; screens can set the first field focused.
    void setFocus(int field) { m_focus = field; }

private:
    void frame(float x, float y, float w, float h, bool hot, bool enabled);
    gfx::GuiBatch* m_batch = nullptr;
    MenuInput m_in;
    int m_w = 0, m_h = 0;
    int m_sliderId = 0, m_fieldId = 0;
    int m_dragging = -1; // slider held since the press
    int m_focus = -1;
};

} // namespace mc::ui
