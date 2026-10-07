#include "ui/Chat.h"

#include <algorithm>
#include <cmath>

namespace mc::ui {

void Chat::open(std::string_view initial) {
    m_open = true;
    m_inputLength = 0;
    m_browse = 0;
    type(initial);
}

void Chat::type(std::string_view text) {
    for (char c : text) {
        if (c == '\b') {
            backspace();
            continue;
        }
        if (m_inputLength >= kMaxInput) break;
        m_input[size_t(m_inputLength++)] = c;
    }
}

void Chat::backspace() {
    if (m_inputLength > 0) --m_inputLength;
}

void Chat::browseSent(int step) {
    const int next = std::clamp(m_browse - step, 0, m_sentCount);
    if (next == m_browse) return;
    m_browse = next;
    m_inputLength = 0;
    if (m_browse > 0) {
        const int slot = m_sentCount - m_browse; // k-th most recent
        type({m_sent[size_t(slot)].data(), size_t(m_sentLength[size_t(slot)])});
    }
}

std::string_view Chat::submit() {
    m_open = false;
    const int n = m_inputLength;
    std::copy_n(m_input.begin(), n, m_submitted.begin());
    m_inputLength = 0;
    m_browse = 0;
    if (n == 0) return {};
    // Remember it for Up/Down: oldest first; when full, drop the oldest (rare, so
    // shifting 50 slots is fine).
    if (m_sentCount == kSentHistory) {
        std::rotate(m_sent.begin(), m_sent.begin() + 1, m_sent.end());
        std::rotate(m_sentLength.begin(), m_sentLength.begin() + 1, m_sentLength.end());
        --m_sentCount;
    }
    std::copy_n(m_submitted.begin(), n, m_sent[size_t(m_sentCount)].begin());
    m_sentLength[size_t(m_sentCount)] = n;
    ++m_sentCount;
    return {m_submitted.data(), size_t(n)};
}

void Chat::pushLine(std::string_view text, uint32_t color, int64_t tick) {
    Line& l = m_lines[size_t(m_next)];
    l.length = static_cast<uint8_t>(std::min(text.size(), l.text.size()));
    std::copy_n(text.begin(), l.length, l.text.begin());
    l.color = color;
    l.tick = tick;
    m_next = (m_next + 1) % kHistory;
    m_count = std::min(m_count + 1, kHistory);
}

void Chat::addMessage(std::string_view text, uint32_t color, int64_t gameTick,
                      const gfx::GuiBatch& batch) {
    // Word-wrap to the chat width (vanilla wraps at spaces, splitting long words).
    while (!text.empty()) {
        size_t fit = 0, lastSpace = std::string_view::npos;
        int width = 0;
        while (fit < text.size() && fit < 128) {
            const int w = batch.textWidth(text.substr(fit, 1));
            if (width + w > kWidth - 4) break;
            if (text[fit] == ' ') lastSpace = fit;
            width += w;
            ++fit;
        }
        size_t cut = fit;
        if (fit < text.size() && lastSpace != std::string_view::npos && lastSpace > 0) cut = lastSpace;
        if (cut == 0) cut = 1;
        pushLine(text.substr(0, cut), color, gameTick);
        text.remove_prefix(cut);
        while (!text.empty() && text.front() == ' ')
            text.remove_prefix(1);
    }
}

std::string_view Chat::line(int newestFirst) const {
    const Line& l = m_lines[size_t((m_next - 1 - newestFirst + 2 * kHistory) % kHistory)];
    return {l.text.data(), l.length};
}

void Chat::draw(gfx::GuiBatch& batch, int guiWidth, int guiHeight, int64_t gameTick) const {
    using gfx::argb;
    // Lines: newest at the bottom, above the hotbar. Open: up to 20 lines, all
    // opaque; closed: up to 10, each fading out over its last second of 10 s.
    const int maxLines = m_open ? 20 : 10;
    const int bottom = guiHeight - 40;
    for (int i = 0; i < std::min(m_count, maxLines); ++i) {
        const Line& l = m_lines[size_t((m_next - 1 - i + 2 * kHistory) % kHistory)];
        double opacity = 1.0;
        if (!m_open) {
            const double age = static_cast<double>(gameTick - l.tick);
            if (age >= kVisibleTicks) break; // older lines are older still
            const double t = std::clamp((1.0 - age / kVisibleTicks) * 10.0, 0.0, 1.0);
            opacity = t * t;
        }
        const float y = static_cast<float>(bottom - (i + 1) * gfx::GuiBatch::kLineHeight);
        const auto bgAlpha = static_cast<uint8_t>(std::lround(127.0 * opacity));
        const auto textAlpha = static_cast<uint8_t>(std::lround(255.0 * opacity));
        if (textAlpha < 4) continue;
        batch.fill(0, y, kWidth + 4, gfx::GuiBatch::kLineHeight, gfx::rgba(0, 0, 0, bgAlpha));
        batch.text({l.text.data(), l.length}, 2, y + 1,
                   (l.color & 0x00FFFFFFu) | (uint32_t(textAlpha) << 24));
    }
    if (m_open) {
        // Input box across the bottom with a blinking cursor (every 6 ticks, vanilla).
        const float y = static_cast<float>(guiHeight - 14);
        batch.fill(2, y, static_cast<float>(guiWidth - 4), 12, gfx::rgba(0, 0, 0, 127));
        const int w = batch.text(input(), 4, y + 2, argb(0xFFE0E0E0));
        if ((gameTick / 6) % 2 == 0) batch.text("_", static_cast<float>(4 + w), y + 2, argb(0xFFE0E0E0));
    }
}

} // namespace mc::ui
