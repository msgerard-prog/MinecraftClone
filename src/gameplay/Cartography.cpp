#include "gameplay/Cartography.h"

namespace mc {

namespace {

bool is(const world::ItemStack& s, std::string_view id) {
    return !s.empty() && world::itemRegistry().item(s.item).id == id;
}

} // namespace

std::optional<world::ItemStack> craftMap(std::span<const world::ItemStack> grid, int size) {
    const world::ItemStack* filled = nullptr;
    int filledCount = 0, empties = 0, paper = 0, others = 0;
    for (const world::ItemStack& s : grid) {
        if (s.empty()) continue;
        if (is(s, "minecraft:filled_map")) {
            filled = &s;
            ++filledCount;
        } else if (is(s, "minecraft:map")) {
            ++empties;
        } else if (is(s, "minecraft:paper")) {
            ++paper;
        } else {
            ++others;
        }
    }
    if (filledCount != 1 || others > 0) return std::nullopt;
    if (empties > 0 && paper == 0) { // (wiki: Map › Copying - up to 8 copies at once)
        world::ItemStack out = *filled;
        out.count = uint8_t(empties + 1);
        return out;
    }
    // Zooming out (wiki: Map › Zooming): the map in the middle of 8 paper.
    if (paper == 8 && empties == 0 && size == 3 && is(grid[4], "minecraft:filled_map")) {
        world::ItemStack out = *filled;
        out.count = 1;
        out.state = kMapScale;
        return out;
    }
    return std::nullopt;
}

world::ItemStack cartography(const world::ItemStack& map, const world::ItemStack& other) {
    if (!is(map, "minecraft:filled_map") || other.empty()) return {};
    world::ItemStack out = map;
    out.count = 1;
    if (is(other, "minecraft:paper")) {
        out.state = kMapScale;
    } else if (is(other, "minecraft:map")) {
        out.count = 2;
    } else if (is(other, "minecraft:glass_pane")) {
        out.state = kMapLock;
    } else {
        return {};
    }
    return out;
}

} // namespace mc
