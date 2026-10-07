#pragma once

#include "gameplay/Inventory.h"
#include "rendering/ItemIcons.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace mc::ui {

// Grass block item colour (temperature 0.5, downfall 1.0; wiki: Grass block).
inline constexpr uint32_t kIconGrassTint = gfx::rgba(0x7C, 0xBD, 0x6B);

// The hotbar (wiki: Heads-up display): 182x22 bar centred at the bottom, the
// selection frame around the selected slot, block icons 16x16 in each slot.
void drawHotbar(gfx::GuiBatch& batch, const Inventory& inventory, const gfx::ItemIcons& icons,
                const gfx::BlockModels& models, int guiWidth, int guiHeight);

// Survival HUD (wiki: Heads-up display): 10 hearts above the hotbar's left half,
// 10 hunger shanks on the right half (right to left). Values in halves (0..20).
void drawVitals(gfx::GuiBatch& batch, float health, int food, int guiWidth, int guiHeight);

// The death screen: red tint, "You died!" and how to respawn.
void drawDeathScreen(gfx::GuiBatch& batch, int guiWidth, int guiHeight);

// What the F3 debug screen shows (filled by main.cpp each frame).
struct DebugInfo {
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
