#include "ui/Hud.h"

#include "world/Coords.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace mc::ui {

void drawHotbar(gfx::GuiBatch& batch, const Hotbar& hotbar, const gfx::BlockModels& models,
                int guiWidth, int guiHeight) {
    const float left = static_cast<float>(guiWidth / 2 - 91);
    const float top = static_cast<float>(guiHeight - 22);
    batch.sprite(gfx::GuiTexture::Hotbar, left, top, 182, 22, 0, 0, 182, 22);
    batch.sprite(gfx::GuiTexture::Selection, left - 1 + static_cast<float>(hotbar.selected() * 20),
                 top - 1, 24, 23, 0, 0, 24, 23);
    for (int i = 0; i < Hotbar::kSlots; ++i) {
        const world::BlockStateId state = hotbar.slot(i);
        if (state == 0) continue;
        batch.blockIcon(models[state], left + 3 + static_cast<float>(i * 20), top + 3,
                        kIconGrassTint);
    }
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
    line("MinecraftClone (1.21 replica)");
    line("%d fps  RD %d", d.fps, d.renderDistance);
    line("C: %d/%d sections  queued: chunks %d, light %d, meshes %d", d.sectionsDrawn,
         d.sectionsTotal, d.pendingChunks, d.pendingLight, d.pendingMeshes);
    line("");
    line("XYZ: %.3f / %.5f / %.3f", d.feet.x, d.feet.y, d.feet.z);
    line("Block: %d %d %d", bx, by, bz);
    line("Chunk: %d %d %d in %d %d %d", blockToLocal(bx), blockToLocal(by), blockToLocal(bz),
         blockToChunk(bx), blockToChunk(by), blockToChunk(bz));
    float yaw = std::fmod(d.yaw, 360.0f);
    if (yaw >= 180.0f) yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;
    line("Facing: %s (%.1f / %.1f)", facingName(d.yaw), yaw, d.pitch);
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

} // namespace mc::ui
