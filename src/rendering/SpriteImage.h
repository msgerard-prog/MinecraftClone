#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace mc::gfx {

// RGBA8 image, row 0 = top. GL-free helpers for building atlas sprites.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // width * height * 4

    const uint8_t* at(int x, int y) const { return &pixels[(size_t(y) * width + x) * 4]; }
};

// Nearest-neighbour upscale by an integer factor (keeps the pixel-art look when a
// 16px sprite shares an atlas with 32px+ resource-pack sprites).
Image upscaleNearest(const Image& src, int factor);

// Vanilla "mean" mipmap: each output pixel averages a 2x2 block (colour and alpha).
Image halve(const Image& src);

// Frame `index` of a vertical animation strip of square frames (width x width each).
Image stripFrame(const Image& strip, int index);

// Animation settings from a sprite's .png.mcmeta (vanilla `animation` section).
// Only `frametime` is read (default 1 tick); custom `frames` order and `interpolate`
// are not supported yet (known deviation).
struct AnimationMeta {
    int frametime = 1;
};
std::optional<AnimationMeta> parseAnimationMeta(std::string_view mcmetaText);

} // namespace mc::gfx
