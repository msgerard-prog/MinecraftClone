// Armor trim tables (M23.6, see the header).
#include "world/ArmorTrims.h"

#include <string>

namespace mc::world {

namespace {
std::string_view bare(std::string_view id) { return id.starts_with("minecraft:") ? id.substr(10) : id; }
} // namespace

int trimPatternOf(ItemId templateItem) {
    const std::string_view id = bare(itemRegistry().item(templateItem).id);
    constexpr std::string_view kSuffix = "_armor_trim_smithing_template";
    if (!id.ends_with(kSuffix)) return 0;
    return findTrimPattern(id.substr(0, id.size() - kSuffix.size())).value_or(0);
}

int trimMaterialOf(ItemId item) {
    const std::string_view id = bare(itemRegistry().item(item).id);
    for (int i = 0; i < int(std::size(kTrimMaterials)); ++i)
        if (kTrimMaterials[i].item == id) return i + 1;
    return 0;
}

std::optional<int> findTrimPattern(std::string_view id) {
    for (int i = 0; i < int(std::size(kTrimPatterns)); ++i)
        if (kTrimPatterns[i] == bare(id)) return i + 1;
    return std::nullopt;
}

std::optional<int> findTrimMaterial(std::string_view id) {
    for (int i = 0; i < int(std::size(kTrimMaterials)); ++i)
        if (kTrimMaterials[i].id == bare(id)) return i + 1;
    return std::nullopt;
}

} // namespace mc::world
