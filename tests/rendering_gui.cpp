// GL-free GUI geometry: font metrics, text layout, item icons.
#include "rendering/GuiBatch.h"

#include <doctest/doctest.h>

#include <vector>

using namespace mc::gfx;

namespace {

// A 128x128 font sheet where glyph 'A' is 5 px wide and 'i' 1 px wide.
std::vector<uint8_t> fakeFont() {
    std::vector<uint8_t> px(128 * 128 * 4, 0);
    auto ink = [&](int code, int width) {
        const int ox = (code % 16) * 8, oy = (code / 16) * 8;
        for (int x = 0; x < width; ++x)
            px[((oy + 3) * 128 + ox + x) * 4 + 3] = 255;
    };
    ink('A', 5);
    ink('i', 1);
    return px;
}

} // namespace

TEST_CASE("font advance = ink width + 1, space is 4 (vanilla glyph sizing)") {
    const auto px = fakeFont();
    const FontMetrics m = FontMetrics::fromImage(px.data(), 128);
    CHECK(m.advance['A'] == 6);
    CHECK(m.advance['i'] == 2);
    CHECK(m.advance[' '] == 4);
    CHECK(m.advance['B'] == 0); // no ink: no glyph
    GuiBatch b;
    b.setFont(m);
    CHECK(b.textWidth("Ai A") == 6 + 2 + 4 + 6);
}

TEST_CASE("text draws a shadow quad under each glyph and no quads for spaces") {
    const auto px = fakeFont();
    GuiBatch b;
    b.setFont(FontMetrics::fromImage(px.data(), 128));
    b.reserveQuads(64);
    const int w = b.text("A A", 10, 20, argb(0xFFFFFFFF));
    CHECK(w == 16);
    REQUIRE(b.vertices().size() == 4 * 6); // 2 glyphs x (shadow + text)
    // Shadow first, offset by one pixel, colour / 4.
    CHECK(b.vertices()[0].x == 11.0f);
    CHECK(b.vertices()[0].y == 21.0f);
    CHECK((b.vertices()[0].color & 0xFF) == 0x3F);
    CHECK(b.vertices()[12].x == 10.0f);
    // clear() keeps capacity: building the next frame doesn't allocate.
    const auto cap = b.vertices().capacity();
    b.clear();
    b.text("A A", 0, 0, argb(0xFFFFFFFF));
    CHECK(b.vertices().capacity() == cap);
}

TEST_CASE("block icons: cubes show 3 shaded faces, box models a flat sprite") {
    GuiBatch b;
    b.setAtlas({4, 16});
    BakedModel cube;
    cube.visible = true;
    for (auto& f : cube.variants[0].faces)
        f.sprite = 5; // column 1, row 1 -> texels (16, 16)
    b.blockIcon(cube, 0, 0, rgba(255, 255, 255));
    REQUIRE(b.vertices().size() == 3 * 6);
    CHECK((b.vertices()[0].color & 0xFF) == 255);       // top
    CHECK((b.vertices()[6].color & 0xFF) == 204);       // south: 0.8
    CHECK((b.vertices()[12].color & 0xFF) == 153);      // east: 0.6
    CHECK(b.vertices()[0].u >= 16.0f);
    CHECK(b.vertices()[0].u <= 32.0f);
    for (const auto& v : b.vertices()) { // inside the 16x16 slot
        CHECK(v.x >= 0.0f);
        CHECK(v.x <= 16.0f);
        CHECK(v.y >= 0.0f);
        CHECK(v.y <= 16.0f);
    }
    b.clear();
    BakedModel torch;
    torch.visible = true;
    torch.boxCount = 1;
    b.blockIcon(torch, 0, 0, 0);
    CHECK(b.vertices().size() == 6);
    b.clear();
    b.blockIcon(BakedModel{}, 0, 0, 0); // invisible (air): nothing
    CHECK(b.vertices().empty());
}
