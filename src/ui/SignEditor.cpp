#include "ui/SignEditor.h"

#include <cstring>

namespace mc::ui {

using gfx::argb;
using gfx::rgba;

void SignEditor::type(std::string_view typed, const gfx::FontMetrics& font) {
    auto& line = m_text.lines[size_t(m_line)];
    for (const char c : typed) {
        size_t n = std::strlen(line.data());
        if (c == '\b') {
            if (n > 0) line[n - 1] = 0;
            continue;
        }
        if (c < 32 || c >= 127 || n >= size_t(world::SignData::kChars)) continue;
        int width = font.advance[uint8_t(c)];
        for (size_t i = 0; i < n; ++i)
            width += font.advance[uint8_t(line[i])];
        if (width > kMaxWidth) continue; // the line is full
        line[n] = c;
        line[n + 1] = 0;
    }
}

bool SignEditor::draw(gfx::GuiBatch& batch, int guiWidth, int guiHeight, double mx, double my, bool click,
                      int64_t timeMs) const {
    batch.fill(0, 0, float(guiWidth), float(guiHeight), rgba(16, 16, 16, 160));
    const float cx = float(guiWidth) / 2.0f;
    batch.text("Edit Sign Message", cx - float(batch.textWidth("Edit Sign Message")) / 2.0f, 30.0f, argb(0xFFFFFFFF));
    // The board (planks-brown) with its four lines, 10 pixels apart (vanilla).
    const float bw = 104.0f, bh = 56.0f, bx = cx - bw / 2.0f, by = 50.0f;
    batch.fill(bx - 1, by - 1, bw + 2, bh + 2, argb(0xFF3C2A16));
    batch.fill(bx, by, bw, bh, argb(0xFFB08850));
    batch.fill(cx - 3.0f, by + bh, 6.0f, 30.0f, argb(0xFF6E5030)); // the post
    for (int i = 0; i < world::SignData::kLines; ++i) {
        const char* text = m_text.lines[size_t(i)].data();
        const float w = float(batch.textWidth(text));
        const float y = by + 9.0f + float(i) * 10.0f;
        batch.text(text, cx - w / 2.0f, y, argb(0xFF000000), false);
        if (i == m_line && (timeMs / 300) % 2 == 0) // the cursor at the line's end
            batch.text("_", cx + w / 2.0f, y, argb(0xFF000000), false);
    }
    // Done (vanilla: under the sign).
    const float dx = cx - 100.0f, dy = float(guiHeight) / 4.0f * 3.0f + 20.0f;
    const bool hot = mx >= dx && mx < dx + 200.0f && my >= dy && my < dy + 20.0f;
    batch.fill(dx, dy, 200.0f, 20.0f, hot ? argb(0xFFFFFFFF) : argb(0xFF000000));
    batch.fill(dx + 1, dy + 1, 198.0f, 18.0f, hot ? argb(0xFF7F86A8) : argb(0xFF6F6F6F));
    batch.text("Done", cx - float(batch.textWidth("Done")) / 2.0f, dy + 6.0f, hot ? argb(0xFFFFFFA0) : argb(0xFFE0E0E0));
    return hot && click;
}

} // namespace mc::ui
