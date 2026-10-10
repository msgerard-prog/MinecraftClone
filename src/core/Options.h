#pragma once

#include <filesystem>

namespace mc {

// A monitor resolution (v1.5.5): one of its video modes, or 0x0 = the monitor's own.
struct DisplayResolution {
    int width = 0, height = 0, refresh = 0;
    bool operator==(const DisplayResolution&) const = default;
};

// How the game window is shown (v1.5.5). Vanilla Java has windowed and fullscreen (F11, its
// "fullscreen" option and Fullscreen Resolution); borderless is ours, by the user's request.
enum class DisplayMode : int { Windowed, Borderless, Fullscreen };

// Player settings (M22.5), kept in options.txt in vanilla's "key:value" format and key
// names (wiki: Options.txt), so the meaning of each value matches vanilla's.
struct GameOptions {
    float fov = 70.0f;           // degrees, 30..110 (vanilla stores (fov - 70) / 40)
    int renderDistance = 16;     // chunks, 2..32 (our default "Far", as the command line)
    int simulationDistance = 12; // chunks, 5..32
    float sensitivity = 0.5f;    // 0..1 (shown as 0..200%; 0.5 = 100%)
    int guiScale = 0;            // 0 = auto (largest that fits), else 1..the largest that fits
    float masterVolume = 1.0f;   // 0..1 (soundCategory_master)
    bool clouds = true;          // renderClouds "true" (fancy) / "false"
    bool vsync = true;           // enableVsync
    bool bobView = true;         // bobView: the camera and hand sway with each step (M30.1)
    // (v1.5.3, the user's choice; ADR 0009) the key number 1-9 in each hotbar slot's corner.
    // Ours: vanilla has no such option or label (OFF gives vanilla's look). Saved as
    // "clone_hotbarNumbers" (an unknown key to vanilla, which ignores it).
    bool hotbarNumbers = true;
    // (v1.5.5) display: vanilla's "fullscreen" and "fullscreenResolution" keys plus ours,
    // "clone_displayMode" (windowed / fullscreen). The resolution is the window's size
    // (windowed) or the video mode (fullscreen); 0x0 = the monitor's own.
    DisplayMode displayMode = DisplayMode::Windowed;
    DisplayResolution resolution;
    // (v1.5.6, the user's request) a windowed game without the title bar and frame
    // ("clone_borderlessWindow"); at the Native resolution it covers the whole monitor.
    bool borderlessWindow = false;
    // What the window shows: fullscreen, or windowed with or without its frame.
    DisplayMode effectiveDisplay() const {
        return displayMode == DisplayMode::Fullscreen ? DisplayMode::Fullscreen
               : borderlessWindow                     ? DisplayMode::Borderless
                                                      : DisplayMode::Windowed;
    }

    // Missing or unreadable file: defaults. Unknown keys are ignored.
    bool load(const std::filesystem::path& file);
    bool save(const std::filesystem::path& file) const;
};

} // namespace mc
