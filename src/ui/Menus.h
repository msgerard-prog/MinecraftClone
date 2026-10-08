#pragma once

#include "core/Options.h"
#include "ui/Menu.h"
#include "world/WorldList.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mc::ui {

// The menu screens (M22.5; vanilla: title, Select World, Create New World, delete
// confirmation, Options, Game Menu). GL-free: they draw into a GuiBatch through Menu
// and report what the player chose; main acts on it.
enum class MenuScreen { None, Title, WorldList, CreateWorld, ConfirmDelete, Options, Pause };

enum class MenuAction {
    None,
    Quit,           // title: Quit Game
    PlayWorld,      // world list: the selected world (state.worlds[selected])
    CreateWorld,    // create screen: name/seed/mode/type in the state
    DeleteWorld,    // confirmed: state.worlds[selected]
    Resume,         // pause: Back to Game (or Esc)
    SaveAndQuit,    // pause: Save and Quit to Title
    OptionsChanged, // a setting changed (apply it; saved when the screen closes)
    OptionsClosed,  // Done: save options.txt
};

struct MenuState {
    MenuScreen screen = MenuScreen::Title;
    MenuScreen optionsBack = MenuScreen::Title; // where Options returns to
    std::vector<world::WorldSummary> worlds;    // refreshed by main when the list opens
    int selected = -1;
    int scroll = 0; // first row shown
    int64_t lastClickMs = 0;
    int lastClickRow = -1;
    // Create New World.
    std::string newName = "New World", newSeed;
    bool newSurvival = true; // vanilla's default game mode
    bool newFlat = false;
    std::string splash = "Made from scratch!"; // the title's yellow line (ours)
};

// Draws the current screen and handles its input. `dirtSprite`: the atlas cell tiled
// behind menus that have no world behind them; `version`: shown on the title screen.
MenuAction drawMenu(Menu& menu, MenuState& state, GameOptions& options, uint16_t dirtSprite, const char* version);

// Our splash texts (vanilla shows a random yellow line on the title screen).
const char* splashText(uint32_t random);

} // namespace mc::ui
