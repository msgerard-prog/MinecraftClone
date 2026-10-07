#pragma once

#include "world/Coords.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace mc::world {

// The three dimensions (M12, wiki: Dimension). One is loaded at a time.
enum class Dimension : uint8_t { Overworld, Nether, End, Count };

struct DimensionInfo {
    std::string_view id;     // "minecraft:the_nether"
    std::string_view folder; // save subfolder ("" overworld, "DIM-1", "DIM1"; vanilla layout)
    bool hasSkyLight;        // wiki: Dimension type › has_skylight
    float ambientLight;      // wiki: Dimension type › ambient_light (Nether 0.1)
    double coordinateScale;  // wiki: Dimension type › coordinate_scale (Nether 8)
    double voidY;            // void damage below (wiki: Void - 64 below the min Y)
    HeightRange height;      // wiki: Dimension type › min_y, height
    int32_t logicalHeight;   // wiki: Dimension type › logical_height (portals, teleports)
};

inline const DimensionInfo& dimensionInfo(Dimension d) {
    static constexpr DimensionInfo kInfo[] = {
        {"minecraft:overworld", "", true, 0.0f, 1.0, -128.0, kOverworldHeight, 384},
        {"minecraft:the_nether", "DIM-1", false, 0.1f, 8.0, -64.0, kNetherHeight, 128},
        {"minecraft:the_end", "DIM1", false, 0.0f, 1.0, -64.0, kEndHeight, 256},
    };
    return kInfo[static_cast<int>(d)];
}

inline std::optional<Dimension> findDimension(std::string_view id) {
    for (int i = 0; i < static_cast<int>(Dimension::Count); ++i) {
        const std::string_view full = dimensionInfo(static_cast<Dimension>(i)).id;
        if (full == id || full.substr(10) == id) return static_cast<Dimension>(i);
    }
    if (id == "nether") return Dimension::Nether;
    if (id == "end") return Dimension::End;
    return std::nullopt;
}

} // namespace mc::world
