// Menus (M22.5): title, world list, create world, options, pause; options.txt; world
// folders and seeds.
#include "core/Options.h"
#include "ui/Menus.h"
#include "world/LevelData.h"
#include "world/WorldList.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace mc;
using namespace mc::ui;

namespace {

// A 320x240 GUI (scale 1 at 320x240): title buttons start at y = 240 / 4 + 48 = 108.
struct Ui {
    gfx::GuiBatch batch;
    Menu menu;
    MenuState state;
    GameOptions options;
    MenuAction frame(MenuInput in = {}) {
        batch.clear();
        menu.begin(batch, 320, 240, in);
        return drawMenu(menu, state, options, 0, "test");
    }
    MenuAction click(double x, double y) {
        MenuInput in;
        in.mx = x;
        in.my = y;
        in.click = in.mouseDown = true;
        const MenuAction a = frame(in);
        frame(); // (released)
        return a;
    }
};

} // namespace

TEST_CASE("menus: Singleplayer opens the world list; Quit Game quits") {
    Ui ui;
    CHECK(ui.frame() == MenuAction::None);
    ui.click(160, 118); // Singleplayer
    CHECK(ui.state.screen == MenuScreen::WorldList);
    ui.state.screen = MenuScreen::Title;
    CHECK(ui.click(210, 142) == MenuAction::Quit); // Quit Game (right of Options...)
}

TEST_CASE("menus: creating a world - type a name, switch the mode, create") {
    Ui ui;
    ui.state.screen = MenuScreen::WorldList;
    ui.click(240, 198); // Create New World (240 - 52 + 10)
    REQUIRE(ui.state.screen == MenuScreen::CreateWorld);
    CHECK(ui.state.newSurvival); // vanilla's default game mode
    ui.click(160, 62);           // the name field
    MenuInput typing;
    typing.typed = "\b\b\b\b\b\b\b\b\b\bMy Base";
    ui.frame(typing);
    CHECK(ui.state.newName == "My Base");
    ui.click(160, 142); // Game Mode
    CHECK_FALSE(ui.state.newSurvival);
    MenuInput enter;
    enter.enter = true;
    CHECK(ui.frame(enter) == MenuAction::CreateWorld);
}

TEST_CASE("menus: an empty world list can't play or delete; Esc goes back") {
    Ui ui;
    ui.state.screen = MenuScreen::WorldList;
    CHECK(ui.click(80, 198) == MenuAction::None); // Play Selected World (disabled)
    MenuInput esc;
    esc.escape = true;
    ui.frame(esc);
    CHECK(ui.state.screen == MenuScreen::Title);
}

TEST_CASE("menus: double-clicking a world plays it; deleting asks first") {
    Ui ui;
    ui.state.worlds.push_back({"w1", "World One", 0, true, false});
    ui.state.worlds.push_back({"w2", "World Two", 0, false, true});
    ui.state.screen = MenuScreen::WorldList;
    MenuInput in;
    in.mx = 160;
    in.my = 32 + 36 + 10; // the second row
    in.click = true;
    in.timeMs = 1000;
    CHECK(ui.frame(in) == MenuAction::None);
    CHECK(ui.state.selected == 1);
    in.timeMs = 1200;
    CHECK(ui.frame(in) == MenuAction::PlayWorld);
    ui.click(80, 222); // Delete
    REQUIRE(ui.state.screen == MenuScreen::ConfirmDelete);
    CHECK(ui.click(80, 130) == MenuAction::DeleteWorld);
}

TEST_CASE("menus: options sliders and buttons change the settings") {
    Ui ui;
    ui.state.screen = MenuScreen::Options;
    // Drag the FOV slider (left column, x 5..155, y 40) to its right end: 110.
    MenuInput in;
    in.mx = 154;
    in.my = 50;
    in.click = in.mouseDown = true;
    CHECK(ui.frame(in) == MenuAction::OptionsChanged);
    CHECK(ui.options.fov == doctest::Approx(110.0f));
    ui.click(80, 98); // GUI Scale: Auto -> 1
    CHECK(ui.options.guiScale == 1);
    ui.click(80, 122); // Clouds
    CHECK_FALSE(ui.options.clouds);
    CHECK(ui.click(160, 222) == MenuAction::OptionsClosed); // Done
    CHECK(ui.state.screen == MenuScreen::Title);
}

TEST_CASE("menus: the game menu resumes or saves and quits") {
    Ui ui;
    ui.state.screen = MenuScreen::Pause;
    CHECK(ui.click(160, 78) == MenuAction::Resume);         // Back to Game (y 68)
    CHECK(ui.click(160, 150) == MenuAction::SaveAndQuit);   // (y 140)
    ui.click(160, 126);                                      // Options... (y 116)
    CHECK(ui.state.screen == MenuScreen::Options);
    CHECK(ui.state.optionsBack == MenuScreen::Pause);
}

TEST_CASE("menus: Statistics opens from the game menu with the session's counters") {
    Ui ui;
    ui.state.screen = MenuScreen::Pause;
    ui.click(210, 102); // Statistics (right half of the row at y 92): disabled without counters
    CHECK(ui.state.screen == MenuScreen::Pause);
    mc::world::Statistics stats;
    stats.add(mc::world::Stat::Jump, 5);
    ui.state.stats = &stats;
    ui.click(210, 102);
    CHECK(ui.state.screen == MenuScreen::Statistics);
    ui.click(160, 32); // the Items tab
    CHECK(ui.state.statsTab == 1);
    MenuInput esc;
    esc.escape = true;
    ui.frame(esc);
    CHECK(ui.state.screen == MenuScreen::Pause);
}

TEST_CASE("options.txt round-trips in vanilla's format") {
    const auto file = std::filesystem::temp_directory_path() / "mc_options_test.txt";
    GameOptions o;
    o.fov = 90.0f;
    o.renderDistance = 8;
    o.sensitivity = 0.75f;
    o.guiScale = 3;
    o.masterVolume = 0.25f;
    o.clouds = false;
    o.vsync = false;
    o.bobView = false;
    REQUIRE(o.save(file));
    GameOptions back;
    REQUIRE(back.load(file));
    CHECK(back.fov == doctest::Approx(90.0f)); // stored as (90 - 70) / 40 = 0.5
    CHECK(back.renderDistance == 8);
    CHECK(back.sensitivity == doctest::Approx(0.75f));
    CHECK(back.guiScale == 3);
    CHECK(back.masterVolume == doctest::Approx(0.25f));
    CHECK_FALSE(back.clouds);
    CHECK_FALSE(back.vsync);
    CHECK_FALSE(back.bobView); // (M30.1)
    std::filesystem::remove(file);
    GameOptions missing;
    CHECK_FALSE(missing.load(file)); // defaults stay
    CHECK(missing.fov == 70.0f);
}

TEST_CASE("seeds from text: numbers as is, words by Java's hashCode, blank random") {
    using mc::world::seedFromText;
    CHECK(seedFromText("12345", 7) == 12345u);
    CHECK(seedFromText("-5", 7) == uint64_t(int64_t(-5)));
    CHECK(seedFromText("hello", 7) == 99162322u); // "hello".hashCode()
    CHECK(seedFromText("Minecraft", 7) == uint64_t(int64_t(int32_t(0xa0e0198du)))); // negative hash, sign-extended
    CHECK(seedFromText("   ", 7) == 7u);
}

TEST_CASE("world folders: bad characters replaced, taken names numbered") {
    const auto dir = std::filesystem::temp_directory_path() / "mc_worldlist_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    using mc::world::folderForWorld;
    CHECK(folderForWorld("My: World?", dir) == "My_ World_");
    CHECK(folderForWorld("", dir) == "New World");
    CHECK(folderForWorld("con", dir) == "_con_"); // (a device name on Windows)
    CHECK(folderForWorld("LPT1.txt", dir) == "_LPT1.txt_");
    std::filesystem::create_directories(dir / "New World");
    CHECK(folderForWorld("New World", dir) == "New World (1)");
    // A world with a level.dat shows in the list; a bare folder doesn't.
    mc::world::LevelData l;
    l.name = "Listed";
    std::filesystem::create_directories(dir / "listed");
    REQUIRE(l.save(dir / "listed"));
    const auto worlds = mc::world::listWorlds(dir);
    REQUIRE(worlds.size() == 1);
    CHECK(worlds[0].name == "Listed");
    CHECK(worlds[0].folder == "listed");
    CHECK(worlds[0].lastPlayed > 0);
    std::filesystem::remove_all(dir);
}
