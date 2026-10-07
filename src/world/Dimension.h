#pragma once

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
};

inline const DimensionInfo& dimensionInfo(Dimension d) {
    static constexpr DimensionInfo kInfo[] = {
        {"minecraft:overworld", "", true, 0.0f, 1.0},
        {"minecraft:the_nether", "DIM-1", false, 0.1f, 8.0},
        {"minecraft:the_end", "DIM1", false, 0.0f, 1.0},
    };
    return kInfo[static_cast<int>(d)];
}

inline std::optional<Dimension> findDimension(std::string_view id) {
    if (!id.starts_with("minecraft:")) id = id.substr(0, id.size()); // bare names too
    for (int i = 0; i < static_cast<int>(Dimension::Count); ++i) {
        const std::string_view full = dimensionInfo(static_cast<Dimension>(i)).id;
        if (full == id || full.substr(10) == id) return static_cast<Dimension>(i);
    }
    if (id == "nether") return Dimension::Nether;
    if (id == "end") return Dimension::End;
    return std::nullopt;
}

} // namespace mc::world
