#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace mc {

// Options accepted by MinecraftClone.exe. Documented in CLAUDE.md › Run.
struct LaunchOptions {
    int width = 1280;
    int height = 720;
    uint64_t seed = 0;
    std::string screenshotPath; // empty = normal interactive run
    int screenshotFrames = 60;  // frames to render before capturing
    bool hidden = false;        // no visible window (screenshot runs)
    bool vsync = true;          // --no-vsync: measure real frame cost
    std::string resourcePacks;  // --resourcepacks DIR (default: <repo>/resourcepacks)
    bool hasPos = false;        // --pos x,y,z  camera position
    glm::dvec3 pos{0.0};
    bool hasLook = false; // --look yaw,pitch  vanilla degrees (yaw 0 = south, +pitch = down)
    float yaw = 0.0f;
    float pitch = 0.0f;
};

// Parses argv (without argv[0]). Returns nullopt and fills `error` on bad input.
std::optional<LaunchOptions> parseCommandLine(std::span<const char* const> args,
                                              std::string& error);

} // namespace mc
