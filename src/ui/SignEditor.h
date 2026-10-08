#pragma once

#include "rendering/GuiBatch.h"
#include "world/BlockEntity.h"
#include "world/Chunk.h"

#include <string_view>

namespace mc::ui {

// The sign editing screen (M23.3c; vanilla: opens after placing a sign or using one):
// four lines over a board, the cursor on the active line; typing adds characters while
// the line fits the board (90 font pixels, vanilla), Enter or Down goes to the next
// line, Up back; Esc or Done saves. GL-free: main hands it input and copies the text
// into the sign's block entity when it closes.
class SignEditor {
public:
    static constexpr int kMaxWidth = 90; // font pixels per line (vanilla)

    bool isOpen() const { return m_open; }
    void open(const world::BlockPos& pos, const world::SignData::Side& text) {
        m_open = true;
        m_pos = pos;
        m_text = text;
        m_line = 0;
    }
    const world::BlockPos& pos() const { return m_pos; }
    const world::SignData::Side& text() const { return m_text; }
    // Typed characters (and '\b'), in order; `advance` gives a character's width.
    void type(std::string_view typed, const gfx::FontMetrics& font);
    void nextLine() { m_line = (m_line + 1) % world::SignData::kLines; }
    void previousLine() { m_line = (m_line + world::SignData::kLines - 1) % world::SignData::kLines; }
    void close() { m_open = false; }
    int line() const { return m_line; }
    // The screen (dim, the board, the lines, a blinking cursor, the Done button);
    // returns true when Done was clicked.
    bool draw(gfx::GuiBatch& batch, int guiWidth, int guiHeight, double mx, double my, bool click, int64_t timeMs) const;

private:
    bool m_open = false;
    world::BlockPos m_pos{};
    world::SignData::Side m_text{};
    int m_line = 0;
};

} // namespace mc::ui
