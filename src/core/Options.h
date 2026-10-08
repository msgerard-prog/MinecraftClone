#pragma once

#include <filesystem>

namespace mc {

// Player settings (M22.5), kept in options.txt in vanilla's "key:value" format and key
// names (wiki: Options.txt), so the meaning of each value matches vanilla's.
struct GameOptions {
    float fov = 70.0f;           // degrees, 30..110 (vanilla stores (fov - 70) / 40)
    int renderDistance = 16;     // chunks, 2..32 (our default "Far", as the command line)
    int simulationDistance = 12; // chunks, 5..32
    float sensitivity = 0.5f;    // 0..1 (shown as 0..200%; 0.5 = 100%)
    int guiScale = 0;            // 0 = auto (largest that fits), else 1..4
    float masterVolume = 1.0f;   // 0..1 (soundCategory_master)
    bool clouds = true;          // renderClouds "true" (fancy) / "false"
    bool vsync = true;           // enableVsync

    // Missing or unreadable file: defaults. Unknown keys are ignored.
    bool load(const std::filesystem::path& file);
    bool save(const std::filesystem::path& file) const;
};

} // namespace mc
