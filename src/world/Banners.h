#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mc::world {

// Banner patterns (M28.3d; wiki: Banner › Patterns, Loom): vanilla's ids; the masks are
// our own (tools/textures/gen_banners.py -> textures/clone_banner/<name>.png, 16x32,
// cut into two atlas sprites like paintings). `item`: the banner pattern item the loom
// needs for it ("" = always available).
struct BannerPattern {
    std::string_view name;
    std::string_view item = "";
};

inline constexpr std::array<BannerPattern, 42> kBannerPatterns = {{
    {"square_bottom_left"}, {"square_bottom_right"}, {"square_top_left"}, {"square_top_right"},
    {"stripe_bottom"}, {"stripe_top"}, {"stripe_left"}, {"stripe_right"}, {"stripe_center"}, {"stripe_middle"},
    {"stripe_downright"}, {"stripe_downleft"}, {"small_stripes"}, {"cross"}, {"straight_cross"},
    {"triangle_bottom"}, {"triangle_top"}, {"triangles_bottom"}, {"triangles_top"}, {"diagonal_left"},
    {"diagonal_up_right"}, {"diagonal_up_left"}, {"diagonal_right"}, {"circle"}, {"rhombus"}, {"half_vertical"},
    {"half_horizontal"}, {"half_vertical_right"}, {"half_horizontal_bottom"}, {"border"}, {"gradient"},
    {"gradient_up"},
    {"bricks", "field_masoned_banner_pattern"}, {"curly_border", "bordure_indented_banner_pattern"},
    {"creeper", "creeper_banner_pattern"}, {"skull", "skull_banner_pattern"}, {"flower", "flower_banner_pattern"},
    {"mojang", "mojang_banner_pattern"}, {"globe", "globe_banner_pattern"}, {"piglin", "piglin_banner_pattern"},
    {"flow", "flow_banner_pattern"}, {"guster", "guster_banner_pattern"},
}};

inline std::optional<int> findBannerPattern(std::string_view name) {
    if (name.starts_with("minecraft:")) name.remove_prefix(10);
    for (size_t i = 0; i < kBannerPatterns.size(); ++i)
        if (kBannerPatterns[i].name == name) return int(i);
    return std::nullopt;
}

// The dye index of a banner block's base colour ("minecraft:light_blue_wall_banner" -> 3).
inline int bannerColour(std::string_view id) {
    static constexpr std::string_view kDyes[16] = {"white", "orange", "magenta", "light_blue", "yellow", "lime",
                                                   "pink", "gray", "light_gray", "cyan", "purple", "blue",
                                                   "brown", "green", "red", "black"};
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    const size_t end = id.ends_with("_wall_banner") ? id.size() - 12 : id.ends_with("_banner") ? id.size() - 7 : id.size();
    id = id.substr(0, end);
    for (int i = 0; i < 16; ++i)
        if (id == kDyes[i]) return i;
    return 0;
}

// A banner's layers over its base colour (the base comes from the block): up to 6 in
// survival (the loom's limit), pattern index and dye colour each.
struct BannerLayers {
    static constexpr int kMax = 16; // (commands may add more than the loom's 6)
    uint8_t count = 0;
    std::array<uint8_t, kMax> pattern{}, colour{};
    bool operator==(const BannerLayers&) const = default;
};

} // namespace mc::world
