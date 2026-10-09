#pragma once

#include "gameplay/Inventory.h"
#include "rendering/ItemIcons.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::ui {

// Grass block item colour (temperature 0.5, downfall 1.0; wiki: Grass block).
inline constexpr uint32_t kIconGrassTint = gfx::rgba(0x7C, 0xBD, 0x6B);

// The hotbar (wiki: Heads-up display): 182x22 bar centred at the bottom, the
// selection frame around the selected slot, block icons 16x16 in each slot.
void drawHotbar(gfx::GuiBatch& batch, const Inventory& inventory, const gfx::ItemIcons& icons,
                const gfx::BlockModels& models, int guiWidth, int guiHeight);

// Survival HUD (wiki: Heads-up display): 10 hearts above the hotbar's left half,
// 10 hunger shanks on the right half (right to left). Values in halves (0..20).
// Air bubbles show above the hunger bar while breath is below full (wiki: Drowning).
// The experience bar above the hotbar and the level number (wiki: Experience › HUD).
void drawExperience(gfx::GuiBatch& batch, int level, float progress, int guiWidth, int guiHeight);
// The jump bar in its place while riding a steerable mount (M26.2; wiki: Horse ›
// Riding): how far the held jump has charged, 0..1.
void drawJumpBar(gfx::GuiBatch& batch, float charge, int guiWidth, int guiHeight);
// (M29.2a) maxHealth over 20 adds heart rows above (Health Boost); absorption is drawn as
// golden hearts after them.
void drawVitals(gfx::GuiBatch& batch, float health, int food, int guiWidth, int guiHeight, int air = 300,
                int armor = 0, float maxHealth = 20.0f, float absorption = 0.0f);

// A boss bar (wiki: Boss bar): its name centred at the top, a 182x5 bar below it
// filled by `fraction` (the ender dragon's is pink).
void drawBossBar(gfx::GuiBatch& batch, std::string_view name, float fraction, uint32_t color, int guiWidth);

// The death screen: red tint, "You died!" and how to respawn.
void drawDeathScreen(gfx::GuiBatch& batch, int guiWidth, int guiHeight);

// What the F3 debug screen shows (filled by main.cpp each frame).
struct DebugInfo {
    const char* version = ""; // the build (version + git revision)
    int fps = 0;
    glm::dvec3 feet{0.0};
    float yaw = 0.0f, pitch = 0.0f;
    bool hasTarget = false;
    glm::ivec3 target{0};
    const char* targetName = ""; // registry string of the targeted state
    int skyLight = 0, blockLight = 0; // at the feet block
    const char* biome = "";          // at the feet block (vanilla id)
    int64_t dayTime = 0;
    int64_t gameTime = 0;
    int renderDistance = 0;
    int sectionsDrawn = 0, sectionsTotal = 0;
    int pendingChunks = 0, pendingLight = 0, pendingMeshes = 0;
    int hostileMobs = 0;
    int width = 0, height = 0;
    double gpuMs = 0.0;
};

// The F3 screen, vanilla layout: left column of game info, right column of system
// info, each line on a translucent grey background. Lines are formatted into fixed
// buffers (no allocation).
class DebugScreen {
public:
    void draw(gfx::GuiBatch& batch, const DebugInfo& info, int guiWidth);
    // Vanilla facing names: yaw 0 = south (+Z), 90 = west, 180 = north, 270 = east.
    static const char* facingName(float yaw);

private:
    void line(const char* fmt, ...); // printf-style into the next fixed buffer

    std::array<std::array<char, 128>, 24> m_lines{};
    int m_count = 0;
};

} // namespace mc::ui
