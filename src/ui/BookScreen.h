#pragma once

#include "rendering/GuiBatch.h"
#include "world/ItemExtras.h"

#include <string_view>

namespace mc::ui {

// The book screen (M28.2c; wiki: Book and Quill, Written Book): a book and quill's pages
// to write on (up to 100 pages of 1024 characters, wrapped at 114 font pixels, 14 lines
// a page), signing it with a title (up to 32 characters), and reading a written book.
// GL-free: main hands it input and, when it closes, writes the pages into the held item.
class BookScreen {
public:
    static constexpr int kPageWidth = 114, kPageLines = 14, kMaxPages = 100, kPageChars = 1024, kTitleChars = 32;
    enum class Action { None, Done, Signed };

    bool isOpen() const { return m_open; }
    bool editing() const { return m_edit; }
    bool signing() const { return m_signing; }
    void openEdit(const world::BookContent& c, int slot);
    void openRead(const world::BookContent& c);
    void close() { m_open = false; }
    int slot() const { return m_slot; } // the inventory slot the book is in (editing)
    int page() const { return m_page; }
    const world::BookContent& content() const { return m_book; }

    // Typed characters and '\b' to the page (or the title while signing); Enter starts a
    // new line on the page.
    void type(std::string_view typed, const gfx::FontMetrics& font);
    void newline(const gfx::FontMetrics& font);
    void turn(int pages); // (editing: turning past the last page adds one, as vanilla)

    // Draws the book and its buttons; clicks turn pages, open the signing view, sign.
    Action draw(gfx::GuiBatch& batch, int guiWidth, int guiHeight, double mx, double my, bool click, int64_t timeMs);

    // Wraps `text` at `width` font pixels (words kept whole when they fit, '\n' breaks):
    // calls f(start, length) per line and returns the number of lines.
    template <typename F> static int wrap(std::string_view text, const gfx::FontMetrics& font, int width, F&& f) {
        const size_t n = text.size();
        int lines = 0;
        size_t start = 0;
        for (;;) {
            size_t end = start, lastSpace = std::string_view::npos;
            int w = 0;
            while (end < n && text[end] != '\n') {
                const int a = font.advance[uint8_t(text[end])];
                if (w + a > width && end > start) break;
                if (text[end] == ' ') lastSpace = end;
                w += a;
                ++end;
            }
            size_t next = n + 1; // (the text ends on this line)
            if (end < n && text[end] == '\n') {
                next = end + 1; // a line break
            } else if (end < n) { // full: break after the last word that fit
                if (lastSpace != std::string_view::npos && lastSpace > start) {
                    end = lastSpace;
                    next = lastSpace + 1;
                } else {
                    next = end;
                }
            }
            f(start, end - start);
            ++lines;
            if (next > n) break;
            start = next;
        }
        return lines;
    }
    static int lineCount(std::string_view text, const gfx::FontMetrics& font, int width) {
        return wrap(text, font, width, [](size_t, size_t) {});
    }

private:
    bool fits(const std::string& page, const gfx::FontMetrics& font) const;
    bool m_open = false, m_edit = false, m_signing = false;
    int m_slot = -1, m_page = 0;
    world::BookContent m_book;
};

} // namespace mc::ui
