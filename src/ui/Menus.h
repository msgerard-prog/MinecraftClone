#pragma once

#include "core/Options.h"
#include "ui/Menu.h"
#include "world/Advancements.h"
#include "world/Statistics.h"
#include "world/WorldList.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mc::ui {

// The menu screens (M22.5; vanilla: title, Select World, Create New World, delete
// confirmation, Options, Game Menu). GL-free: they draw into a GuiBatch through Menu
// and report what the player chose; main acts on it.
enum class MenuScreen {
    None,
    Title,
    WorldList,
    CreateWorld,
    ConfirmDelete,
    Options,
    Pause,
    Statistics,
    Advancements,
    CommandBlock // (M29.7) a command block's settings (creative)
};

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
    CommandBlockDone, // (M29.7) apply the command block screen's settings
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
    int newDifficulty = 2;   // Normal (vanilla's default; M28.1b)
    bool newFlat = false;
    std::string splash = "Made from scratch!"; // the title's yellow line (ours)
    // Statistics (M28.1d, from the Game Menu): the session's counters, the tab (General,
    // Items, Mobs) and its scroll.
    const world::Statistics* stats = nullptr;
    int statsTab = 0, statsScroll = 0;
    // Advancements (M28.5c, from the Game Menu): the player's, the tab and its scroll.
    const world::Advancements* adv = nullptr;
    int advTab = 0, advScroll = 0;
    // Command block (M29.7; vanilla "Set Console Command for Block"): the command, the
    // mode (0 impulse, 1 chain, 2 repeat), conditional, Always Active, the last output.
    std::string command, commandOutput;
    int commandMode = 0;
    bool commandConditional = false, commandAlways = false;
};

// Draws the current screen and handles its input. `dirtSprite`: the atlas cell tiled
// behind menus that have no world behind them; `version`: shown on the title screen.
MenuAction drawMenu(Menu& menu, MenuState& state, GameOptions& options, uint16_t dirtSprite,
                    const char* version);

// (v1.5.2; vanilla's level loading screen) the dirt background with "Loading terrain..."
// and how much of the spawn area is ready, while a world loads. Call between Menu::begin
// and the GUI draw; it takes no input.
void drawLoadingScreen(Menu& menu, uint16_t dirtSprite, int percent);

// Our splash texts (vanilla shows a random yellow line on the title screen).
const char* splashText(uint32_t random);

} // namespace mc::ui
