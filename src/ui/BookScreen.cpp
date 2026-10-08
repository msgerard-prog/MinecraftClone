#include "ui/BookScreen.h"

#include <algorithm>
#include <cstdio>

namespace mc::ui {

using gfx::argb;

void BookScreen::openEdit(const world::BookContent& c, int slot) {
    m_open = m_edit = true;
    m_signing = false;
    m_book = c;
    if (m_book.pages.empty()) m_book.pages.emplace_back();
    m_slot = slot;
    m_page = 0;
}

void BookScreen::openRead(const world::BookContent& c) {
    m_open = true;
    m_edit = m_signing = false;
    m_book = c;
    if (m_book.pages.empty()) m_book.pages.emplace_back();
    m_slot = -1;
    m_page = 0;
}

bool BookScreen::fits(const std::string& page, const gfx::FontMetrics& font) const {
    return page.size() <= size_t(kPageChars) && lineCount(page, font, kPageWidth) <= kPageLines;
}

void BookScreen::type(std::string_view typed, const gfx::FontMetrics& font) {
    if (!m_edit) return;
    std::string& text = m_signing ? m_book.title : m_book.pages[size_t(m_page)];
    for (const char c : typed) {
        if (c == '\b') {
            if (!text.empty()) text.pop_back();
            continue;
        }
        if (c < 32 || c >= 127) continue;
        if (m_signing) {
            if (text.size() < size_t(kTitleChars)) text.push_back(c);
            continue;
        }
        text.push_back(c);
        if (!fits(text, font)) text.pop_back(); // the page is full
    }
}

void BookScreen::newline(const gfx::FontMetrics& font) {
    if (!m_edit || m_signing) return;
    std::string& text = m_book.pages[size_t(m_page)];
    text.push_back('\n');
    if (!fits(text, font)) text.pop_back();
}

void BookScreen::turn(int pages) {
    const int last = int(m_book.pages.size()) - 1;
    int to = m_page + pages;
    if (m_edit && to > last && last + 1 < kMaxPages && !m_book.pages[size_t(last)].empty()) {
        m_book.pages.emplace_back(); // (vanilla adds a page when turning past the last written one)
        to = last + 1;
    }
    m_page = std::clamp(to, 0, int(m_book.pages.size()) - 1);
}

BookScreen::Action BookScreen::draw(gfx::GuiBatch& batch, int guiWidth, int guiHeight, double mx, double my,
                                    bool click, int64_t timeMs) {
    const float cx = float(guiWidth) / 2.0f;
    // The book: a leather cover with a parchment page (ours; vanilla's is a 192x192 texture).
    const float bw = 146.0f, bh = 180.0f, bx = cx - bw / 2.0f;
    const float by = std::max(2.0f, (float(guiHeight) - bh - 30.0f) / 2.0f); // (vanilla: near the top)
    batch.fill(bx - 2, by - 2, bw + 4, bh + 4, argb(0xFF4A2A12));
    batch.fill(bx, by, bw, bh, argb(0xFF7A4A24));
    batch.fill(bx + 6, by + 6, bw - 12, bh - 12, argb(0xFFF4ECD2));
    auto button = [&](const char* label, float x, float y, float w) {
        const bool hot = mx >= x && mx < x + w && my >= y && my < y + 20.0f;
        batch.fill(x, y, w, 20.0f, hot ? argb(0xFFFFFFFF) : argb(0xFF000000));
        batch.fill(x + 1, y + 1, w - 2, 18.0f, hot ? argb(0xFF7F86A8) : argb(0xFF6F6F6F));
        batch.text(label, x + (w - float(batch.textWidth(label))) / 2.0f, y + 6.0f,
                   hot ? argb(0xFFFFFFA0) : argb(0xFFE0E0E0));
        return hot && click;
    };
    const float tx = bx + 16.0f, ty = by + 30.0f;
    const bool blink = (timeMs / 300) % 2 == 0;
    if (m_signing) {
        // Signing (vanilla: "Enter Book Title:", the title, "by <name>", a warning).
        const char* head = "Enter Book Title:";
        batch.text(head, cx - float(batch.textWidth(head)) / 2.0f, ty, argb(0xFF000000), false);
        const float w = float(batch.textWidth(m_book.title));
        batch.text(m_book.title, cx - w / 2.0f, ty + 16.0f, argb(0xFF000000), false);
        if (blink) batch.text("_", cx + w / 2.0f, ty + 16.0f, argb(0xFF000000), false);
        const char* by2 = "by Player";
        batch.text(by2, cx - float(batch.textWidth(by2)) / 2.0f, ty + 28.0f, argb(0xFF505050), false);
        const char* note1 = "Note! When you sign the";
        const char* note2 = "book, it will no longer";
        const char* note3 = "be editable.";
        for (int i = 0; const char* n : {note1, note2, note3})
            batch.text(n, cx - float(batch.textWidth(n)) / 2.0f, ty + 60.0f + float(i++) * 9.0f, argb(0xFF000000), false);
        const float y = by + bh + 6.0f;
        if (button("Sign and Close", cx - 100.0f, y, 98.0f) && !m_book.title.empty()) return Action::Signed;
        if (button("Cancel", cx + 2.0f, y, 98.0f)) m_signing = false;
        return Action::None;
    }
    char head[32];
    std::snprintf(head, sizeof(head), "Page %d of %d", m_page + 1, int(m_book.pages.size()));
    batch.text(head, bx + bw - 16.0f - float(batch.textWidth(head)), by + 14.0f, argb(0xFF000000), false);
    const std::string& page = m_book.pages[size_t(m_page)];
    float lastX = tx, lastY = ty;
    wrap(page, batch.font(), kPageWidth, [&](size_t start, size_t len) {
        const std::string_view line(page.data() + start, len);
        batch.text(line, tx, lastY, argb(0xFF000000), false);
        lastX = tx + float(batch.textWidth(line));
        lastY += 9.0f;
    });
    if (m_edit && blink) batch.text("_", lastX, lastY - 9.0f, argb(0xFF000000), false);
    // Page arrows (vanilla: bottom corners of the page).
    const float ay = by + bh - 26.0f;
    if (m_page > 0 && button("<", bx + 12.0f, ay, 20.0f)) turn(-1);
    if ((m_edit || m_page + 1 < int(m_book.pages.size())) && button(">", bx + bw - 32.0f, ay, 20.0f)) turn(1);
    const float y = by + bh + 6.0f;
    if (m_edit) {
        if (button("Sign", cx - 100.0f, y, 98.0f)) m_signing = true;
        if (button("Done", cx + 2.0f, y, 98.0f)) return Action::Done;
    } else if (button("Done", cx - 100.0f, y, 200.0f)) {
        return Action::Done;
    }
    return Action::None;
}

} // namespace mc::ui
