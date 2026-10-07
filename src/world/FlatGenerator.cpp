#include "world/FlatGenerator.h"

#include "world/Blocks.h"

#include <charconv>

namespace mc::world {

std::optional<FlatGenerator> FlatGenerator::fromPreset(std::string_view preset) {
    const std::string_view layersPart = preset.substr(0, preset.find(';'));
    FlatGenerator gen;
    std::string_view rest = layersPart;
    while (!rest.empty()) {
        const size_t comma = rest.find(',');
        std::string_view layer = rest.substr(0, comma);
        int count = 1;
        if (const size_t star = layer.find('*'); star != std::string_view::npos) {
            const auto [ptr, ec] = std::from_chars(layer.data(), layer.data() + star, count);
            if (ec != std::errc{} || ptr != layer.data() + star || count < 1) return std::nullopt;
            layer.remove_prefix(star + 1);
        }
        const auto state = blockRegistry().parse(layer);
        if (!state) return std::nullopt;
        // Check the height limit before inserting (a huge count must not allocate).
        if (static_cast<size_t>(count) > static_cast<size_t>(kHeight) - gen.m_layers.size()) {
            return std::nullopt;
        }
        gen.m_layers.insert(gen.m_layers.end(), static_cast<size_t>(count), *state);
        if (comma == std::string_view::npos) break;
        rest.remove_prefix(comma + 1);
    }
    if (gen.m_layers.empty() || gen.m_layers.size() > static_cast<size_t>(kHeight)) {
        return std::nullopt;
    }
    return gen;
}

void FlatGenerator::generate(Chunk& chunk) const {
    // Layer by layer, block by block (a few thousand sets per chunk; fine for flat).
    for (size_t i = 0; i < m_layers.size(); ++i) {
        const int y = kMinY + static_cast<int>(i);
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x)
                chunk.set(x, y, z, m_layers[i]);
        }
    }
}

} // namespace mc::world
