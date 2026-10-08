#pragma once

#include "world/Items.h"

#include <optional>
#include <string_view>

namespace mc::world {

// Armor trims (M23.6; wiki: Smithing Template › Armor trims): a pattern from its
// template and a material from the ingredient, kept on the armor piece as
// ItemStack::trim = pattern << 8 | material (both 1-based, 0 = none) and saved as
// vanilla's minecraft:trim {pattern, material}. Purely decorative.
inline constexpr std::string_view kTrimPatterns[] = {
    "sentry", "dune", "coast", "wild", "ward", "eye", "vex", "tide", "snout",
    "rib", "spire", "wayfinder", "shaper", "silence", "raiser", "host", "flow", "bolt"};
// Materials and the item each comes from (amethyst and resin: not in the game yet).
struct TrimMaterial {
    std::string_view id;   // "iron"
    std::string_view item; // "iron_ingot"
};
inline constexpr TrimMaterial kTrimMaterials[] = {
    {"iron", "iron_ingot"},   {"copper", "copper_ingot"}, {"gold", "gold_ingot"},
    {"lapis", "lapis_lazuli"}, {"emerald", "emerald"},     {"diamond", "diamond"},
    {"netherite", "netherite_ingot"}, {"redstone", "redstone"}, {"quartz", "quartz"}};

// The trim pattern of a template item ("coast_armor_trim_smithing_template"), 1-based.
int trimPatternOf(ItemId templateItem);
// The trim material an ingredient gives, 1-based (0 = not a trim material).
int trimMaterialOf(ItemId item);
std::optional<int> findTrimPattern(std::string_view id);  // with or without "minecraft:"
std::optional<int> findTrimMaterial(std::string_view id); // likewise

} // namespace mc::world
