#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mc::world {

// Painting variants (M28.3a; wiki: Painting › Canvases): vanilla's names and sizes in
// blocks. The art is our own (tools/textures/gen_paintings.py; textures/painting/<name>.png,
// 16 pixels a block). `placeable`: chosen when a painting is hung (vanilla's
// minecraft:placeable tag; earth, wind, fire and water only come from commands).
struct PaintingVariant {
    std::string_view name;
    uint8_t width, height;
    bool placeable = true;
};

inline constexpr std::array<PaintingVariant, 50> kPaintings = {{
    {"alban", 1, 1},          {"aztec", 1, 1},           {"aztec2", 1, 1},        {"bomb", 1, 1},
    {"kebab", 1, 1},          {"meditative", 1, 1},      {"plant", 1, 1},         {"wasteland", 1, 1},
    {"graham", 1, 2},         {"prairie_ride", 1, 2},    {"wanderer", 1, 2},      {"courbet", 2, 1},
    {"creebet", 2, 1},        {"pool", 2, 1},            {"sea", 2, 1},           {"sunset", 2, 1},
    {"baroque", 2, 2},        {"bust", 2, 2},            {"earth", 2, 2, false},  {"fire", 2, 2, false},
    {"humble", 2, 2},         {"match", 2, 2},           {"skull_and_roses", 2, 2}, {"stage", 2, 2},
    {"void", 2, 2},           {"water", 2, 2, false},    {"wind", 2, 2, false},   {"wither", 2, 2},
    {"bouquet", 3, 3},        {"cavebird", 3, 3},        {"cotan", 3, 3},         {"endboss", 3, 3},
    {"fern", 3, 3},           {"owlemons", 3, 3},        {"sunflowers", 3, 3},    {"tides", 3, 3},
    {"backyard", 3, 4},       {"pond", 3, 4},            {"changing", 4, 2},      {"fighters", 4, 2},
    {"finding", 4, 2},        {"lowmist", 4, 2},         {"passage", 4, 2},       {"donkey_kong", 4, 3},
    {"skeleton", 4, 3},       {"burning_skull", 4, 4},   {"orb", 4, 4},           {"pigscene", 4, 4},
    {"pointer", 4, 4},        {"unpacked", 4, 4},
}};

inline std::optional<int> findPainting(std::string_view name) {
    if (name.starts_with("minecraft:")) name.remove_prefix(10);
    for (size_t i = 0; i < kPaintings.size(); ++i)
        if (kPaintings[i].name == name) return int(i);
    return std::nullopt;
}

} // namespace mc::world
