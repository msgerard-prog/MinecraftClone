#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mc {

// Options accepted by MinecraftClone.exe. Documented in CLAUDE.md › Run.
struct LaunchOptions {
    int width = 1280;
    int height = 720;
    uint64_t seed = 0;
    std::string screenshotPath; // empty = normal interactive run
    int screenshotFrames = 60;  // frames to render before capturing
    bool hidden = false;        // no visible window (screenshot runs)
    bool mute = false;          // --mute: no sound (also implied by --hidden and --screenshot)
    bool sound = false;         // --sound: sound even in hidden/screenshot runs (audio checks)
    std::string menu;           // --menu title|worlds|create|options|pause: show that screen (screenshots)
    bool vsync = true;          // --no-vsync: measure real frame cost
    std::string resourcePacks;  // --resourcepacks DIR (default: <repo>/resourcepacks)
    bool flat = false;          // --flat: the M2 superflat test world instead of terrain
    int renderDistance = 16; // --render-distance N (chunks, 2..32; 1.21.11 default "Far", 16)
    bool renderDistanceSet = false; // given on the command line (overrides options.txt)
    bool autoFly = false;  // --auto-fly: fly forward at 4x sprint speed (streaming benchmark)
    int maxFps = 0;        // --max-fps N: sleep to cap the frame rate (0 = uncapped)
    bool demoEdit = false; // --demo-edit: scripted break/place after loading (visual test)
    bool hasPos = false;   // --pos x,y,z  camera position
    glm::dvec3 pos{0.0};
    bool hasLook = false; // --look yaw,pitch  vanilla degrees (yaw 0 = south, +pitch = down)
    float yaw = 0.0f;
    float pitch = 0.0f;
    int64_t time = 0; // --time T: day time in ticks (0 sunrise, 6000 noon, 18000 midnight)
    bool debugScreen = false;          // --f3: start with the F3 debug screen open
    bool inventory = false;            // --inventory: start with the creative inventory open
    bool trade = false;                // --trade: open the nearest employed villager's trades (screenshots)
    bool hasOpenBlock = false;         // --open-block x,y,z: open that block's screen (chest...)
    int openBlock[3] = {0, 0, 0};
    std::vector<std::string> commands; // --command CMD (repeatable): run as chat lines at start
    std::string world;                 // --world NAME: saves/<NAME> (created if missing)
    std::string worldTitle;            // a new world's LevelName (menus; default: the folder)
    bool survival = false;             // new worlds start in survival (menus; default creative)
    bool noSave = false;               // --no-save: don't load or save a world
    bool printVersion = false;         // --version: print the build and exit
    std::string generator = "overworld4"; // --generator overworld4|overworld3|overworld2|overworld|terrain (new worlds; newest default)
    std::string dimension;               // --dimension overworld|nether|end (start there)
};

// Parses argv (without argv[0]). Returns nullopt and fills `error` on bad input.
std::optional<LaunchOptions> parseCommandLine(std::span<const char* const> args,
                                              std::string& error);

} // namespace mc
