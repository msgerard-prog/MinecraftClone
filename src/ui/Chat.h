#pragma once

#include "rendering/GuiBatch.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::ui {

// Chat (wiki: Chat): message history shown bottom-left, an input line opened with T
// (or / pre-filled), sent-message history on Up/Down. GL-free; fixed buffers, so
// typing and drawing never allocate.
class Chat {
public:
    static constexpr int kMaxInput = 256;   // vanilla's chat input limit
    static constexpr int kHistory = 100;    // received lines kept (vanilla: 100)
    static constexpr int kSentHistory = 50;
    static constexpr int kWidth = 320;      // GUI px (vanilla default chat width)
    static constexpr int kVisibleTicks = 200; // messages fade after 10 s when closed

    bool isOpen() const { return m_open; }
    void open(std::string_view initial = {});
    void close() { m_open = false; }

    // Typing (while open).
    void type(std::string_view text);
    void backspace();
    // Up (-1) / Down (+1) through sent messages.
    void browseSent(int step);
    // Enter: closes the chat and returns the input line (empty if nothing typed).
    // The view stays valid until the next submit.
    std::string_view submit();

    // A received line (wrapped to the chat width with `batch`'s font).
    void addMessage(std::string_view text, uint32_t color, int64_t gameTick,
                    const gfx::GuiBatch& batch);

    std::string_view input() const { return {m_input.data(), size_t(m_inputLength)}; }
    int lineCount() const { return m_count; }
    std::string_view line(int newestFirst) const;

    void draw(gfx::GuiBatch& batch, int guiWidth, int guiHeight, int64_t gameTick) const;

private:
    struct Line {
        std::array<char, 128> text{};
        uint8_t length = 0;
        uint32_t color = 0;
        int64_t tick = 0;
    };
    void pushLine(std::string_view text, uint32_t color, int64_t tick);

    bool m_open = false;
    std::array<char, kMaxInput> m_input{};
    int m_inputLength = 0;
    std::array<Line, kHistory> m_lines{}; // ring, m_next = slot for the next line
    int m_next = 0;
    int m_count = 0;
    std::array<std::array<char, kMaxInput>, kSentHistory> m_sent{};
    std::array<int, kSentHistory> m_sentLength{};
    int m_sentCount = 0;
    int m_browse = 0; // 0 = editing a new line; k = k-th most recent sent message
    std::array<char, kMaxInput> m_submitted{};
};

} // namespace mc::ui
