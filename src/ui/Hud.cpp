#include "ui/Hud.h"

#include "world/Coords.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <utility>

namespace mc::ui {

void drawHotbar(gfx::GuiBatch& batch, const Inventory& inventory, const gfx::ItemIcons& icons,
                const gfx::BlockModels& models, int guiWidth, int guiHeight) {
    const float left = static_cast<float>(guiWidth / 2 - 91);
    const float top = static_cast<float>(guiHeight - 22);
    batch.sprite(gfx::GuiTexture::Hotbar, left, top, 182, 22, 0, 0, 182, 22);
    batch.sprite(gfx::GuiTexture::Selection, left - 1 + static_cast<float>(inventory.selected() * 20),
                 top - 1, 24, 23, 0, 0, 24, 23);
    for (int i = 0; i < Inventory::kHotbar; ++i)
        icons.draw(batch, models, inventory.slot(i), left + 3 + static_cast<float>(i * 20), top + 3,
                   kIconGrassTint);
}

void drawVitals(gfx::GuiBatch& batch, float health, int food, int guiWidth, int guiHeight, int air, int armor) {
    using gfx::HudIcon;
    auto icon = [&](HudIcon i, float x, float y) {
        batch.sprite(gfx::GuiTexture::Icons, x, y, 9, 9, static_cast<float>(int(i) * 9), 0, 9, 9);
    };
    const float y = static_cast<float>(guiHeight - 39);
    const int hp = static_cast<int>(std::ceil(health));
    for (int i = 0; i < 10; ++i) {
        const float x = static_cast<float>(guiWidth / 2 - 91 + i * 8);
        icon(HudIcon::HeartContainer, x, y);
        if (hp >= 2 * i + 2) icon(HudIcon::HeartFull, x, y);
        else if (hp == 2 * i + 1) icon(HudIcon::HeartHalf, x, y);
    }
    for (int i = 0; i < 10; ++i) {
        const float x = static_cast<float>(guiWidth / 2 + 91 - 9 - i * 8);
        icon(HudIcon::FoodEmpty, x, y);
        if (food >= 2 * i + 2) icon(HudIcon::FoodFull, x, y);
        else if (food == 2 * i + 1) icon(HudIcon::FoodHalf, x, y);
    }
    if (armor > 0) { // armor bar above the hearts, shown only when wearing armor (wiki: HUD)
        for (int i = 0; i < 10; ++i) {
            const float x = static_cast<float>(guiWidth / 2 - 91 + i * 8);
            icon(armor >= 2 * i + 2 ? HudIcon::ArmorFull : armor == 2 * i + 1 ? HudIcon::ArmorHalf : HudIcon::ArmorEmpty,
                 x, y - 10.0f);
        }
    }
    if (air < 300) { // vanilla: full bubbles, then one bursting as it runs out
        const int full = std::max(0, int(std::ceil((air - 2) * 10.0 / 300.0)));
        const int shown = std::max(0, int(std::ceil(air * 10.0 / 300.0)));
        for (int i = 0; i < shown && i < 10; ++i) {
            const float x = static_cast<float>(guiWidth / 2 + 91 - 9 - i * 8);
            icon(i < full ? HudIcon::Air : HudIcon::AirBursting, x, y - 10.0f);
        }
    }
}

void drawExperience(gfx::GuiBatch& batch, int level, float progress, int guiWidth, int guiHeight) {
    // 182 x 5 bar 29 pixels above the bottom, green fill; the level in green above it.
    const float x = float(guiWidth / 2 - 91), y = float(guiHeight - 29);
    batch.fill(x, y, 182, 5, gfx::rgba(20, 20, 20));
    batch.fill(x + 1, y + 1, 180, 3, gfx::rgba(40, 60, 30));
    if (progress > 0.0f) batch.fill(x + 1, y + 1, 180.0f * std::min(1.0f, progress), 3, gfx::rgba(128, 255, 32));
    if (level > 0) {
        char text[12];
        const int n = std::snprintf(text, sizeof(text), "%d", level);
        const float w = float(batch.textWidth(std::string_view(text, size_t(n))));
        const float tx = float(guiWidth) / 2.0f - w / 2.0f, ty = y - 7.0f;
        for (const auto& [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
            batch.text(std::string_view(text, size_t(n)), tx + float(dx), ty + float(dy), gfx::rgba(0, 0, 0), false);
        batch.text(std::string_view(text, size_t(n)), tx, ty, gfx::rgba(128, 255, 32), false);
    }
}

void drawDeathScreen(gfx::GuiBatch& batch, int guiWidth, int guiHeight) {
    batch.fill(0, 0, static_cast<float>(guiWidth), static_cast<float>(guiHeight), gfx::argb(0x80700000));
    const std::string_view title = "You died!";
    const float w = static_cast<float>(batch.textWidth(title)) * 2.0f;
    batch.text(title, (guiWidth - w) / 2.0f, guiHeight / 4.0f, gfx::argb(0xFFFFFFFF), true, 2.0f);
    const std::string_view hint = "Press Enter to respawn";
    batch.text(hint, (guiWidth - batch.textWidth(hint)) / 2.0f, guiHeight / 4.0f + 40.0f,
               gfx::argb(0xFFE0E0E0));
}

const char* DebugScreen::facingName(float yaw) {
    const float y = std::fmod(std::fmod(yaw, 360.0f) + 360.0f, 360.0f);
    const int q = static_cast<int>(std::floor((y + 45.0f) / 90.0f)) % 4;
    static constexpr const char* kNames[4] = {"south (Towards positive Z)",
                                              "west (Towards negative X)",
                                              "north (Towards negative Z)",
                                              "east (Towards positive X)"};
    return kNames[q];
}

void DebugScreen::line(const char* fmt, ...) {
    if (m_count >= static_cast<int>(m_lines.size())) return;
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(m_lines[size_t(m_count++)].data(), 128, fmt, args);
    va_end(args);
}

void DebugScreen::draw(gfx::GuiBatch& batch, const DebugInfo& d, int guiWidth) {
    using world::blockToChunk;
    using world::blockToLocal;
    const int bx = static_cast<int>(std::floor(d.feet.x));
    const int by = static_cast<int>(std::floor(d.feet.y));
    const int bz = static_cast<int>(std::floor(d.feet.z));
    m_count = 0;
    // Left: game state.
    line("MinecraftClone %s (1.21.11 replica)", d.version);
    line("%d fps  RD %d", d.fps, d.renderDistance);
    line("C: %d/%d sections  queued: chunks %d, light %d, meshes %d", d.sectionsDrawn,
         d.sectionsTotal, d.pendingChunks, d.pendingLight, d.pendingMeshes);
    line("E: %d hostile mobs", d.hostileMobs);
    line("");
    line("XYZ: %.3f / %.5f / %.3f", d.feet.x, d.feet.y, d.feet.z);
    line("Block: %d %d %d", bx, by, bz);
    line("Chunk: %d %d %d in %d %d %d", blockToLocal(bx), blockToLocal(by), blockToLocal(bz),
         blockToChunk(bx), blockToChunk(by), blockToChunk(bz));
    float yaw = std::fmod(d.yaw, 360.0f);
    if (yaw >= 180.0f) yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;
    line("Facing: %s (%.1f / %.1f)", facingName(d.yaw), yaw, d.pitch);
    if (*d.biome) line("Biome: %s", d.biome);
    line("Client Light: %d (%d sky, %d block)", d.skyLight > d.blockLight ? d.skyLight : d.blockLight,
         d.skyLight, d.blockLight);
    line("Day %lld, time %lld (game time %lld)", static_cast<long long>(d.dayTime / 24000),
         static_cast<long long>(d.dayTime % 24000), static_cast<long long>(d.gameTime));
    const int leftCount = m_count;
    // Right: system, then the targeted block (id, one property per line).
    line("Display: %dx%d", d.width, d.height);
    line("GPU: %.2f ms/frame", d.gpuMs);
    line("OpenGL 4.6 core");
    if (d.hasTarget) {
        line("");
        line("Targeted Block: %d, %d, %d", d.target.x, d.target.y, d.target.z);
        const std::string_view name(d.targetName);
        const size_t open = name.find('[');
        line("%.*s", int(std::min(open, name.size())), name.data());
        if (open != std::string_view::npos) {
            std::string_view props = name.substr(open + 1, name.size() - open - 2);
            while (!props.empty()) {
                const size_t comma = std::min(props.find(','), props.size());
                const std::string_view kv = props.substr(0, comma);
                const size_t eq = kv.find('=');
                if (eq != std::string_view::npos)
                    line("%.*s: %.*s", int(eq), kv.data(), int(kv.size() - eq - 1), kv.data() + eq + 1);
                props.remove_prefix(std::min(comma + 1, props.size()));
            }
        }
    }

    const uint32_t bg = gfx::argb(0x90505050), fg = gfx::argb(0xFFE0E0E0);
    for (int i = 0; i < m_count; ++i) {
        const std::string_view s(m_lines[size_t(i)].data());
        const bool right = i >= leftCount;
        const int row = right ? i - leftCount : i;
        const float y = static_cast<float>(2 + row * gfx::GuiBatch::kLineHeight);
        if (s.empty()) continue;
        const int w = batch.textWidth(s);
        const float x = right ? static_cast<float>(guiWidth - 2 - w) : 2.0f;
        batch.fill(x - 1, y - 1, static_cast<float>(w + 1), gfx::GuiBatch::kLineHeight, bg);
        batch.text(s, x, y, fg, /*shadow=*/false);
    }
}

void drawBossBar(gfx::GuiBatch& batch, std::string_view name, float fraction, uint32_t color, int guiWidth) {
    const float x = std::floor((float(guiWidth) - 182.0f) / 2.0f), y = 12.0f;
    const int w = batch.textWidth(name);
    batch.text(name, std::floor((float(guiWidth) - float(w)) / 2.0f), y - 9.0f, gfx::rgba(255, 255, 255));
    batch.fill(x, y, 182.0f, 5.0f, gfx::rgba(40, 20, 40, 200)); // (vanilla: the empty bar sprite)
    batch.fill(x, y, std::floor(182.0f * std::clamp(fraction, 0.0f, 1.0f)), 5.0f, color);
}

} // namespace mc::ui
