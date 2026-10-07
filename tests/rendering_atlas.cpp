#include "rendering/TextureAtlas.h"

#include <doctest/doctest.h>

TEST_CASE("missing texture is a 2x2 magenta/black checker (wiki: Missing textures)") {
    const auto px = mc::gfx::TextureAtlas::missingSpritePixels();
    constexpr int n = mc::gfx::TextureAtlas::kMinCellSize;
    REQUIRE(px.size() == size_t(n * n * 4));
    auto at = [&](int x, int y) { return &px[(y * n + x) * 4]; };
    auto isMagenta = [](const uint8_t* p) { return p[0] == 248 && p[1] == 0 && p[2] == 248; };
    auto isBlack = [](const uint8_t* p) { return p[0] == 0 && p[1] == 0 && p[2] == 0; };
    CHECK(isBlack(at(0, 0)));
    CHECK(isMagenta(at(n - 1, 0)));
    CHECK(isMagenta(at(0, n - 1)));
    CHECK(isBlack(at(n - 1, n - 1)));
    CHECK(at(3, 3)[3] == 255);
}
