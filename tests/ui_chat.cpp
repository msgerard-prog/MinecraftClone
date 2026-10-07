// Chat and HUD logic (GL-free).
#include "ui/Chat.h"
#include "ui/Hud.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

using namespace mc::ui;

namespace {

// A batch whose font gives every printable glyph a 6 px advance.
mc::gfx::GuiBatch fixedFont() {
    std::vector<uint8_t> px(128 * 128 * 4, 0);
    for (int code = 33; code < 127; ++code)
        for (int x = 0; x < 5; ++x)
            px[(((code / 16) * 8 + 2) * 128 + (code % 16) * 8 + x) * 4 + 3] = 255;
    mc::gfx::GuiBatch b;
    b.setFont(mc::gfx::FontMetrics::fromImage(px.data(), 128));
    return b;
}

} // namespace

TEST_CASE("chat input: typing, backspace, submit returns the line and closes") {
    Chat c;
    CHECK_FALSE(c.isOpen());
    c.open("/");
    CHECK(c.input() == "/");
    c.type("timx");
    c.backspace();
    c.type("e set day");
    CHECK(c.input() == "/time set day");
    CHECK(std::string(c.submit()) == "/time set day");
    CHECK_FALSE(c.isOpen());
    c.open();
    CHECK(c.submit().empty()); // nothing typed
}

TEST_CASE("chat applies backspaces in typing order (from the window's text stream)") {
    Chat c;
    c.open();
    c.type("a\bb"); // a, Backspace, b within one frame
    CHECK(c.input() == "b");
    c.type("\b\b\b"); // more than there is: stays empty
    CHECK(c.input().empty());
}

TEST_CASE("chat input is capped at 256 characters (vanilla)") {
    Chat c;
    c.open();
    c.type(std::string(300, 'a'));
    CHECK(c.input().size() == 256);
}

TEST_CASE("Up/Down walk through sent messages, newest first") {
    Chat c;
    for (const char* m : {"one", "two", "three"}) {
        c.open();
        c.type(m);
        c.submit();
    }
    c.open();
    c.browseSent(-1);
    CHECK(c.input() == "three");
    c.browseSent(-1);
    CHECK(c.input() == "two");
    c.browseSent(-1);
    c.browseSent(-1); // stays at the oldest
    CHECK(c.input() == "one");
    c.browseSent(1);
    CHECK(c.input() == "two");
    c.browseSent(1);
    c.browseSent(1); // back to a fresh line
    CHECK(c.input().empty());
}

TEST_CASE("long messages wrap at the chat width; history keeps the newest 100 lines") {
    Chat c;
    const auto b = fixedFont();
    // 6 px per glyph (space 4), 316 px usable: two lines, wrapped at a space.
    std::string msg;
    for (int i = 0; i < 20; ++i)
        msg += "word ";
    c.addMessage(msg, 0xFFFFFFFFu, 0, b);
    CHECK(c.lineCount() == 2);
    CHECK(b.textWidth(c.line(1)) <= Chat::kWidth - 4);
    CHECK(c.line(1).back() != ' ');
    for (int i = 0; i < 150; ++i)
        c.addMessage("x" + std::to_string(i), 0xFFFFFFFFu, 0, b);
    CHECK(c.lineCount() == 100);
    CHECK(c.line(0) == "x149");
}

TEST_CASE("closed chat shows messages for 10 seconds, then hides them") {
    Chat c;
    auto b = fixedFont();
    b.reserveQuads(256);
    c.addMessage("hi", 0xFFFFFFFFu, 0, b);
    c.draw(b, 400, 300, 100);
    CHECK_FALSE(b.vertices().empty());
    b.clear();
    c.draw(b, 400, 300, 200); // 10 s later: gone
    CHECK(b.vertices().empty());
    c.open(); // open chat shows history regardless of age
    c.draw(b, 400, 300, 5000);
    CHECK_FALSE(b.vertices().empty());
}

TEST_CASE("F3 facing names follow vanilla yaw (0 south, 90 west, 180 north, 270 east)") {
    CHECK(std::string(DebugScreen::facingName(0)).starts_with("south"));
    CHECK(std::string(DebugScreen::facingName(90)).starts_with("west"));
    CHECK(std::string(DebugScreen::facingName(-180)).starts_with("north"));
    CHECK(std::string(DebugScreen::facingName(-90)).starts_with("east"));
    CHECK(std::string(DebugScreen::facingName(44)).starts_with("south"));
    CHECK(std::string(DebugScreen::facingName(46)).starts_with("west"));
}

#include "ui/CreativeInventory.h"
#include "world/Blocks.h"

namespace {

mc::gfx::BlockModels visibleModels() {
    const auto& r = mc::world::blockRegistry();
    mc::gfx::BlockModels m;
    m.resize(r.stateCount());
    for (size_t s = 1; s < r.stateCount(); ++s)
        m.at(mc::world::BlockStateId(s)).visible = true;
    m.at(r.defaultState(mc::world::blocks::Water)).fluid = true;
    return m;
}

// GUI 400x300: panel at ((400-195)/2, (300-136)/2) = (102, 82).
constexpr int kGw = 400, kGh = 300;
double gridX(int col) { return 102 + 9 + col * 18 + 8; }
double gridY(int row) { return 82 + 18 + row * 18 + 8; }
double hotbarY() { return 82 + 112 + 8; }

} // namespace

TEST_CASE("creative inventory lists every item: visible blocks and tools, no air or fluids") {
    CreativeInventory inv;
    inv.build(visibleModels());
    const auto& items = mc::world::itemRegistry();
    REQUIRE_FALSE(inv.items().empty());
    bool pickaxe = false;
    for (const auto& s : inv.items()) {
        CHECK(s.item != mc::world::kNoItem);
        CHECK(items.item(s.item).block != mc::world::blocks::Water);
        pickaxe |= items.item(s.item).id == "minecraft:diamond_pickaxe";
    }
    CHECK(pickaxe);
}

TEST_CASE("creative inventory: take from the grid, put into the hotbar, drop outside") {
    CreativeInventory inv;
    inv.build(visibleModels());
    mc::Inventory hotbar;
    inv.open();
    const auto first = inv.items()[0].item;
    inv.click(gridX(0), gridY(0), kGw, kGh, hotbar);
    CHECK(inv.carried().item == first);
    CHECK(inv.carried().count == 64); // creative: a full stack
    const auto old = hotbar.slot(4).item;
    inv.click(gridX(4), hotbarY(), kGw, kGh, hotbar); // swap with hotbar slot 5
    CHECK(hotbar.slot(4).item == first);
    CHECK(inv.carried().item == old);
    inv.click(5, 5, kGw, kGh, hotbar); // outside the panel: dropped
    CHECK(inv.carried().empty());
    // Number key over a grid item copies a full stack into that hotbar slot.
    inv.numberKey(0, gridX(1), gridY(0), kGw, kGh, hotbar);
    CHECK(hotbar.slot(0).item == inv.items()[1].item);
    // Closing drops whatever is carried.
    inv.click(gridX(2), gridY(0), kGw, kGh, hotbar);
    inv.close();
    CHECK(inv.carried().empty());
}

TEST_CASE("creative inventory scrolling is clamped to the item rows") {
    CreativeInventory inv;
    inv.build(visibleModels());
    inv.scroll(-100); // wheel down
    CHECK(inv.scrollRow() == inv.maxScrollRow());
    inv.scroll(100);
    CHECK(inv.scrollRow() == 0);
}
