#include "rendering/SpriteImage.h"

#include <algorithm>
#include <charconv>

namespace mc::gfx {

Image upscaleNearest(const Image& src, int factor) {
    Image out{src.width * factor, src.height * factor, {}};
    out.pixels.resize(size_t(out.width) * out.height * 4);
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const uint8_t* s = src.at(x / factor, y / factor);
            std::copy_n(s, 4, &out.pixels[(size_t(y) * out.width + x) * 4]);
        }
    }
    return out;
}

Image halve(const Image& src) {
    Image out{std::max(1, src.width / 2), std::max(1, src.height / 2), {}};
    out.pixels.resize(size_t(out.width) * out.height * 4);
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            for (int c = 0; c < 4; ++c) {
                int sum = 0;
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) {
                        const int sx = std::min(src.width - 1, x * 2 + dx);
                        const int sy = std::min(src.height - 1, y * 2 + dy);
                        sum += src.at(sx, sy)[c];
                    }
                out.pixels[(size_t(y) * out.width + x) * 4 + c] =
                    static_cast<uint8_t>((sum + 2) / 4);
            }
        }
    }
    return out;
}

Image stripFrame(const Image& strip, int index) {
    const int size = strip.width;
    Image out{size, size, {}};
    const auto begin =
        strip.pixels.begin() + static_cast<std::ptrdiff_t>(size_t(index) * size * size * 4);
    out.pixels.assign(begin, begin + static_cast<std::ptrdiff_t>(size_t(size) * size * 4));
    return out;
}

std::optional<AnimationMeta> parseAnimationMeta(std::string_view text) {
    // Minimal scan instead of a JSON parser: is there an "animation" object, and does
    // it set "frametime"? (Good enough for vanilla's .mcmeta files.)
    if (text.find("\"animation\"") == std::string_view::npos) return std::nullopt;
    AnimationMeta meta;
    const size_t key = text.find("\"frametime\"");
    if (key != std::string_view::npos) {
        size_t p = text.find(':', key);
        if (p != std::string_view::npos) {
            ++p;
            while (p < text.size() &&
                   (text[p] == ' ' || text[p] == '\t' || text[p] == '\n' || text[p] == '\r'))
                ++p;
            int value = 0;
            const auto [ptr, ec] =
                std::from_chars(text.data() + p, text.data() + text.size(), value);
            if (ec == std::errc{} && value > 0) meta.frametime = value;
        }
    }
    return meta;
}

} // namespace mc::gfx
