#include "ui/Menu.h"

#include <algorithm>
#include <cmath>

namespace mc::ui {

using gfx::argb;
using gfx::rgba;

void Menu::begin(gfx::GuiBatch& batch, int guiWidth, int guiHeight, const MenuInput& input) {
    m_batch = &batch;
    m_w = guiWidth;
    m_h = guiHeight;
    m_in = input;
    m_sliderId = 0;
    m_fieldId = 0;
    if (!input.mouseDown) m_dragging = -1;
}

bool Menu::hovered(float x, float y, float w, float h) const {
    return m_in.mx >= x && m_in.mx < x + w && m_in.my >= y && m_in.my < y + h;
}

void Menu::frame(float x, float y, float w, float h, bool hot, bool enabled) {
    gfx::GuiBatch& b = *m_batch;
    // Our own bevelled button (vanilla's look: grey stone face, light top-left edge,
    // dark bottom-right; a white outline when hovered).
    b.fill(x, y, w, h, hot && enabled ? argb(0xFFFFFFFF) : argb(0xFF000000));
    const uint32_t face = !enabled ? argb(0xFF2C2C2C) : hot ? argb(0xFF7F86A8) : argb(0xFF6F6F6F);
    b.fill(x + 1, y + 1, w - 2, h - 2, face);
    if (enabled) {
        b.fill(x + 1, y + 1, w - 2, 1, hot ? argb(0xFFA9B0D8) : argb(0xFFA0A0A0));
        b.fill(x + 1, y + h - 3, w - 2, 2, hot ? argb(0xFF5A6080) : argb(0xFF4C4C4C));
    }
}

bool Menu::button(std::string_view label, float x, float y, float w, bool enabled, float h) {
    const bool hot = hovered(x, y, w, h);
    frame(x, y, w, h, hot, enabled);
    const uint32_t color = !enabled ? argb(0xFFA0A0A0) : hot ? argb(0xFFFFFFA0) : argb(0xFFE0E0E0);
    const int tw = m_batch->textWidth(label);
    m_batch->text(label, std::floor(x + (w - float(tw)) / 2.0f), std::floor(y + (h - 8.0f) / 2.0f), color);
    return enabled && hot && m_in.click;
}

bool Menu::slider(std::string_view label, float x, float y, float w, float& value) {
    const int id = m_sliderId++;
    const float h = 20.0f;
    const bool hot = hovered(x, y, w, h);
    if (hot && m_in.click) m_dragging = id;
    bool changed = false;
    if (m_dragging == id) {
        const float v = std::clamp(float((m_in.mx - (x + 4.0)) / (w - 8.0)), 0.0f, 1.0f);
        changed = v != value;
        value = v;
    }
    // The track (dark), then the handle (an 8-pixel button) at the value.
    gfx::GuiBatch& b = *m_batch;
    b.fill(x, y, w, h, argb(0xFF000000));
    b.fill(x + 1, y + 1, w - 2, h - 2, argb(0xFF3A3A3A));
    frame(x + value * (w - 8.0f), y, 8.0f, h, hot || m_dragging == id, true);
    const int tw = b.textWidth(label);
    b.text(label, std::floor(x + (w - float(tw)) / 2.0f), y + 6.0f,
           hot || m_dragging == id ? argb(0xFFFFFFA0) : argb(0xFFE0E0E0));
    return changed;
}

bool Menu::textField(std::string& text, float x, float y, float w, size_t maxLength) {
    const int id = m_fieldId++;
    const float h = 20.0f;
    if (m_in.click) {
        if (hovered(x, y, w, h)) m_focus = id;
        else if (m_focus == id) m_focus = -1;
    }
    bool changed = false;
    if (m_focus == id)
        for (const char c : m_in.typed) {
            if (c == '\b') {
                if (!text.empty()) {
                    text.pop_back();
                    changed = true;
                }
            } else if (text.size() < maxLength && c >= 32 && c < 127) {
                text += c;
                changed = true;
            }
        }
    gfx::GuiBatch& b = *m_batch;
    // Vanilla's field: black inside a grey (white when focused) border.
    b.fill(x, y, w, h, m_focus == id ? argb(0xFFFFFFFF) : argb(0xFFA0A0A0));
    b.fill(x + 1, y + 1, w - 2, h - 2, argb(0xFF000000));
    // Show the end of long text so the cursor stays visible.
    std::string_view shown = text;
    while (!shown.empty() && float(b.textWidth(shown)) > w - 12.0f)
        shown.remove_prefix(1);
    const int tw = b.text(shown, x + 4.0f, y + 6.0f, argb(0xFFE0E0E0));
    if (m_focus == id && (m_in.timeMs / 300) % 2 == 0) b.text("_", x + 4.0f + float(tw), y + 6.0f, argb(0xFFE0E0E0));
    return changed;
}

void Menu::text(std::string_view s, float x, float y, uint32_t color, bool centred, float scale) {
    if (centred) x -= std::floor(float(m_batch->textWidth(s)) * scale / 2.0f);
    m_batch->text(s, x, y, color, true, scale);
}

void Menu::tiledBackground(uint16_t sprite, uint32_t tint) {
    // (v1.5.2) vanilla tiles its menu background 32 GUI pixels to a tile (2 per texel), so
    // it scales with the GUI like everything else - at 16 it read as fine noise on 5K.
    for (int y = 0; y < m_h; y += 32)
        for (int x = 0; x < m_w; x += 32)
            m_batch->atlasSpriteCentred(sprite, float(x) + 16.0f, float(y) + 16.0f, 16.0f, 16.0f, tint);
}

void Menu::dim() { m_batch->fill(0, 0, float(m_w), float(m_h), rgba(16, 16, 16, 160)); }

} // namespace mc::ui
