// Villager tables (M24; wiki: Villager › Professions, Villager types).
#include "world/Villagers.h"

#include "world/Blocks.h"

#include <vector>

namespace mc::world {

namespace {

// Job sites (wiki: Villager › Professions); apron colours are our own choice.
constexpr ProfessionInfo kProfessions[] = {
    {"minecraft:none", "", 0x8B6A4A},
    {"minecraft:armorer", "blast_furnace", 0x3A3A44},
    {"minecraft:butcher", "smoker", 0xE8E8E0},
    {"minecraft:cartographer", "cartography_table", 0xD8C890},
    {"minecraft:cleric", "brewing_stand", 0x8A3A9A},
    {"minecraft:farmer", "composter", 0xC89A3A},
    {"minecraft:fisherman", "barrel", 0x3A7AC8},
    {"minecraft:fletcher", "fletching_table", 0x9AB05A},
    {"minecraft:leatherworker", "cauldron", 0x8A4A2A},
    {"minecraft:librarian", "lectern", 0xE0E0E8},
    {"minecraft:mason", "stonecutter", 0x7A7A7A},
    {"minecraft:shepherd", "loom", 0xA8784A},
    {"minecraft:toolsmith", "smithing_table", 0x2A2A2A},
    {"minecraft:weaponsmith", "grindstone", 0x1A1A1A},
    {"minecraft:nitwit", "", 0x3A8A3A},
};
static_assert(std::size(kProfessions) == size_t(Profession::Count));

constexpr std::string_view kTypes[] = {"minecraft:plains", "minecraft:desert", "minecraft:savanna", "minecraft:taiga",
                                       "minecraft:snow",   "minecraft:jungle", "minecraft:swamp"};
static_assert(std::size(kTypes) == size_t(VillagerType::Count));

std::string_view bare(std::string_view id) { return id.starts_with("minecraft:") ? id.substr(10) : id; }

} // namespace

const ProfessionInfo& professionInfo(Profession p) { return kProfessions[size_t(p)]; }

std::optional<Profession> findProfession(std::string_view id) {
    for (size_t i = 0; i < std::size(kProfessions); ++i)
        if (bare(kProfessions[i].id) == bare(id)) return static_cast<Profession>(i);
    return std::nullopt;
}

Profession professionForJobSite(BlockId block) {
    // (cached per block: called while villagers look for work)
    static const std::vector<Profession> table = [] {
        const auto& r = blockRegistry();
        std::vector<Profession> t(r.blockCount(), Profession::None);
        for (BlockId b = 0; b < r.blockCount(); ++b) {
            const std::string_view id = bare(r.block(b).id);
            for (size_t i = 1; i < std::size(kProfessions); ++i)
                if (!kProfessions[i].jobSite.empty() && kProfessions[i].jobSite == id) t[b] = static_cast<Profession>(i);
            // (wiki: any cauldron - water, lava, powder snow - is a leatherworker's)
            if (id == "water_cauldron" || id == "lava_cauldron" || id == "powder_snow_cauldron")
                t[b] = Profession::Leatherworker;
        }
        return t;
    }();
    return block < table.size() ? table[block] : Profession::None;
}

bool isJobSite(BlockId block) { return professionForJobSite(block) != Profession::None; }

std::string_view villagerTypeId(VillagerType t) { return kTypes[size_t(t)]; }

std::optional<VillagerType> findVillagerType(std::string_view id) {
    for (size_t i = 0; i < std::size(kTypes); ++i)
        if (bare(kTypes[i]) == bare(id)) return static_cast<VillagerType>(i);
    return std::nullopt;
}

} // namespace mc::world
