#pragma once

#include "world/Chunk.h"

#include <optional>
#include <string_view>
#include <vector>

namespace mc::world {

// Superflat world generator. Layers come from vanilla's preset string format
// (wiki: Superflat#Preset code format): bottom-up comma-separated layers,
// "N*block" repeats a layer, then ";biome". Layers start at the world floor (Y -64).
class FlatGenerator {
public:
    // Vanilla's default "Classic Flat" preset.
    static constexpr std::string_view kClassicFlat =
        "minecraft:bedrock,2*minecraft:dirt,minecraft:grass_block;minecraft:plains";

    // Returns nullopt for an unknown block or malformed preset.
    static std::optional<FlatGenerator> fromPreset(std::string_view preset);

    void generate(Chunk& chunk) const;
    // Top surface Y (first air block), e.g. -60 for Classic Flat.
    int surfaceY() const { return kMinY + static_cast<int>(m_layers.size()); }
    const std::vector<BlockStateId>& layers() const { return m_layers; }

private:
    std::vector<BlockStateId> m_layers; // index 0 = Y -64
};

} // namespace mc::world
