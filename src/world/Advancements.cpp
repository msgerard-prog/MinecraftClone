// Advancements (M28.5c, see the header; wiki: Advancement - ids, titles, descriptions).
#include "world/Advancements.h"

#include "core/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace mc::world {

namespace {

using T = AdvTab;
using F = AdvFrame;
using E = AdvEvent;

// Story, Nether, The End, Adventure, Husbandry (vanilla's ids and texts; our triggers).
constexpr Advancement kAdvancements[] = {
    {"story/root", "Minecraft", "The heart and story of the game", T::Story, F::Task, {"crafting_table"}},
    {"story/mine_stone", "Stone Age", "Mine Stone with your new Pickaxe", T::Story, F::Task,
     {"cobblestone", "cobbled_deepslate", "blackstone"}},
    {"story/upgrade_tools", "Getting an Upgrade", "Construct a better Pickaxe", T::Story, F::Task, {"stone_pickaxe"}},
    {"story/smelt_iron", "Acquire Hardware", "Smelt an Iron Ingot", T::Story, F::Task, {"iron_ingot"}},
    {"story/obtain_armor", "Suit Up", "Protect yourself with a piece of iron armor", T::Story, F::Task,
     {"iron_helmet", "iron_chestplate", "iron_leggings", "iron_boots"}},
    {"story/lava_bucket", "Hot Stuff", "Fill a Bucket with lava", T::Story, F::Task, {"lava_bucket"}},
    {"story/iron_tools", "Isn't It Iron Pick", "Upgrade your Pickaxe", T::Story, F::Task, {"iron_pickaxe"}},
    {"story/form_obsidian", "Ice Bucket Challenge", "Obtain a block of Obsidian", T::Story, F::Task, {"obsidian"}},
    {"story/mine_diamond", "Diamonds!", "Acquire diamonds", T::Story, F::Task, {"diamond"}},
    {"story/enter_the_nether", "We Need to Go Deeper", "Build, light and enter a Nether Portal", T::Story, F::Task,
     {}, MobType::Count, false, E::EnterNether},
    {"story/shiny_gear", "Cover Me with Diamonds", "Diamond armor saves lives", T::Story, F::Task,
     {"diamond_helmet", "diamond_chestplate", "diamond_leggings", "diamond_boots"}},
    {"story/enchant_item", "Enchanter", "Enchant an item at an Enchanting Table", T::Story, F::Task, {},
     MobType::Count, false, E::Enchanted},
    {"story/cure_zombie_villager", "Zombie Doctor", "Weaken and then cure a Zombie Villager", T::Story, F::Goal, {},
     MobType::Count, false, E::CuredZombieVillager},
    {"story/follow_ender_eye", "Eye Spy", "Follow an Eye of Ender", T::Story, F::Task, {"ender_eye"}},
    {"story/enter_the_end", "The End?", "Enter the End Portal", T::Story, F::Task, {}, MobType::Count, false,
     E::EnterEnd},

    {"nether/root", "Nether", "Bring summer clothes", T::Nether, F::Task, {}, MobType::Count, false, E::EnterNether},
    {"nether/return_to_sender", "Return to Sender", "Destroy a Ghast with a fireball", T::Nether, F::Challenge, {},
     MobType::Ghast},
    {"nether/find_fortress", "A Terrible Fortress", "Break your way into a Nether Fortress", T::Nether, F::Task, {},
     MobType::Blaze},
    {"nether/obtain_ancient_debris", "Hidden in the Depths", "Obtain Ancient Debris", T::Nether, F::Task,
     {"ancient_debris"}},
    {"nether/obtain_crying_obsidian", "Who is Cutting Onions?", "Obtain Crying Obsidian", T::Nether, F::Task,
     {"crying_obsidian"}},
    {"nether/use_lodestone", "Country Lode, Take Me Home", "Use a Compass on a Lodestone", T::Nether, F::Task, {},
     MobType::Count, false, E::UsedLodestone},
    {"nether/netherite_armor", "Cover Me in Debris", "Get a full suit of Netherite armor", T::Nether, F::Challenge,
     {"netherite_chestplate"}},
    {"nether/get_wither_skull", "Spooky Scary Skeleton", "Obtain a Wither Skeleton's skull", T::Nether, F::Task,
     {"wither_skeleton_skull"}},
    {"nether/obtain_blaze_rod", "Into Fire", "Relieve a Blaze of its rod", T::Nether, F::Task, {"blaze_rod"}},
    {"nether/summon_wither", "Withering Heights", "Summon the Wither", T::Nether, F::Task, {}, MobType::Count, false,
     E::SummonedWither},
    {"nether/brew_potion", "Local Brewery", "Brew a Potion", T::Nether, F::Task, {}, MobType::Count, false,
     E::BrewedPotion},
    {"nether/create_beacon", "Bring Home the Beacon", "Construct and place a Beacon", T::Nether, F::Task, {"beacon"}},

    {"end/root", "The End", "Or the beginning?", T::End, F::Task, {}, MobType::Count, false, E::EnterEnd},
    {"end/kill_dragon", "Free the End", "Good luck", T::End, F::Task, {}, MobType::Count, false, E::KillDragon},
    {"end/dragon_egg", "The Next Generation", "Hold the Dragon Egg", T::End, F::Goal, {"dragon_egg"}},
    {"end/enter_end_gateway", "Remote Getaway", "Escape the island", T::End, F::Task, {}, MobType::Count, false,
     E::EnterGateway},
    {"end/respawn_dragon", "The End... Again...", "Respawn the Ender Dragon", T::End, F::Goal, {}, MobType::Count,
     false, E::RespawnDragon},
    {"end/dragon_breath", "You Need a Mint", "Collect Dragon's Breath in a Glass Bottle", T::End, F::Goal,
     {"dragon_breath"}},
    {"end/find_end_city", "The City at the End of the Game", "Go on in, what could happen?", T::End, F::Task, {},
     MobType::Shulker},
    {"end/elytra", "Sky's the Limit", "Find Elytra", T::End, F::Goal, {"elytra"}},
    {"end/levitate", "Great View From Up Here", "Levitate up 50 blocks from the attacks of a Shulker", T::End,
     F::Challenge, {}, MobType::Count, false, E::Levitated},

    {"adventure/root", "Adventure", "Adventure, exploration and combat", T::Adventure, F::Task, {}, MobType::Count,
     true},
    {"adventure/kill_a_mob", "Monster Hunter", "Kill any hostile monster", T::Adventure, F::Task, {}, MobType::Count,
     true},
    {"adventure/trade", "What a Deal!", "Successfully trade with a Villager", T::Adventure, F::Task, {},
     MobType::Count, false, E::Traded},
    {"adventure/sleep_in_bed", "Sweet Dreams", "Sleep in a Bed to change your respawn point", T::Adventure, F::Task,
     {}, MobType::Count, false, E::Slept},
    {"adventure/shoot_arrow", "Take Aim", "Shoot something with an Arrow", T::Adventure, F::Task, {},
     MobType::Count, false, E::ShotArrow},
    {"adventure/ol_betsy", "Ol' Betsy", "Shoot a Crossbow", T::Adventure, F::Task, {}, MobType::Count, false,
     E::ShotCrossbow},
    {"adventure/throw_trident", "A Throwaway Joke", "Throw a Trident at something", T::Adventure, F::Task, {},
     MobType::Count, false, E::ThrewTrident},
    {"adventure/summon_iron_golem", "Hired Help", "Summon an Iron Golem to help defend a village", T::Adventure,
     F::Goal, {}, MobType::Count, false, E::SummonedIronGolem},
    {"adventure/hero_of_the_village", "Hero of the Village", "Successfully defend a village from a raid",
     T::Adventure, F::Challenge, {}, MobType::Count, false, E::HeroOfTheVillage},
    {"adventure/voluntary_exile", "Voluntary Exile", "Kill a raid captain", T::Adventure, F::Task, {},
     MobType::Pillager},
    {"adventure/totem_of_undying", "Postmortal", "Use a Totem of Undying to cheat death", T::Adventure, F::Goal,
     {"totem_of_undying"}},
    {"adventure/trim_with_any_armor_pattern", "Crafting a New Look", "Craft a trimmed armor at a Smithing Table",
     T::Adventure, F::Task, {}, MobType::Count, false, E::TrimmedArmor},
    {"adventure/salvage_sherd", "Respecting the Remnants", "Brush a Suspicious block to obtain a Pottery Sherd",
     T::Adventure, F::Task,
     {"archer_pottery_sherd", "arms_up_pottery_sherd", "prize_pottery_sherd", "skull_pottery_sherd",
      "angler_pottery_sherd", "blade_pottery_sherd"}},
    {"adventure/minecraft_trials_edition", "Minecraft: Trial(s) Edition", "Step foot in a Trial Chamber",
     T::Adventure, F::Task, {"trial_key"}},
    {"adventure/under_lock_and_key", "Under Lock and Key", "Unlock a Vault with a Trial Key", T::Adventure, F::Task,
     {}, MobType::Count, false, E::OpenedVault},
    {"adventure/revaulting", "Revaulting", "Unlock an Ominous Vault with an Ominous Trial Key", T::Adventure,
     F::Goal, {}, MobType::Count, false, E::OpenedOminousVault},
    {"adventure/overoverkill", "Over-Overkill", "Deal 50 hearts of damage in a single hit using the Mace",
     T::Adventure, F::Challenge, {}, MobType::Count, false, E::MaceSmash50},

    {"husbandry/root", "Husbandry", "The world is full of friends and food", T::Husbandry, F::Task, {},
     MobType::Count, false, E::Ate},
    {"husbandry/plant_seed", "A Seedy Place", "Plant a seed and watch it grow", T::Husbandry, F::Task, {},
     MobType::Count, false, E::Planted},
    {"husbandry/breed_an_animal", "The Parrots and the Bats", "Breed two animals together", T::Husbandry, F::Task,
     {}, MobType::Count, false, E::Bred},
    {"husbandry/tame_an_animal", "Best Friends Forever", "Tame an animal", T::Husbandry, F::Task, {},
     MobType::Count, false, E::Tamed},
    {"husbandry/fishy_business", "Fishy Business", "Catch a fish", T::Husbandry, F::Task, {}, MobType::Count, false,
     E::Fished},
    {"husbandry/tactical_fishing", "Tactical Fishing", "Catch a Fish... without a Fishing Rod!", T::Husbandry,
     F::Task, {"cod_bucket", "salmon_bucket", "tropical_fish_bucket", "pufferfish_bucket"}},
    {"husbandry/axolotl_in_a_bucket", "The Cutest Predator", "Catch an Axolotl in a Bucket", T::Husbandry, F::Task,
     {"axolotl_bucket"}},
    {"husbandry/tadpole_in_a_bucket", "Bukkit Bukkit", "Catch a Tadpole in a Bucket", T::Husbandry, F::Task,
     {"tadpole_bucket"}},
    {"husbandry/obtain_netherite_hoe", "Serious Dedication",
     "Use a Netherite Ingot to upgrade a Hoe, and then reevaluate your life choices", T::Husbandry, F::Challenge,
     {"netherite_hoe"}},
    {"husbandry/wax_on", "Wax On", "Apply Honeycomb to a Copper block!", T::Husbandry, F::Task, {}, MobType::Count,
     false, E::Waxed},
    {"husbandry/safely_harvest_honey", "Bee Our Guest", "Use a Campfire to collect Honey from a Beehive using a Glass Bottle without aggravating the Bees",
     T::Husbandry, F::Task, {"honey_bottle"}},
    {"husbandry/obtain_sniffer_egg", "Smells Interesting", "Obtain a Sniffer Egg", T::Husbandry, F::Task,
     {"sniffer_egg"}},
    {"husbandry/plant_any_sniffer_seed", "Planting the Past", "Plant any Sniffer seed", T::Husbandry, F::Task, {},
     MobType::Count, false, E::PlantedSnifferSeed},
    {"husbandry/brush_armadillo", "Shear Brilliance", "Obtain an Armadillo Scute using a Brush", T::Husbandry,
     F::Task, {"armadillo_scute"}},
};
static_assert(std::size(kAdvancements) <= size_t(kMaxAdvancements));

std::string now() {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S +0000", &tm);
    return buf;
}

} // namespace

std::span<const Advancement> advancements() { return kAdvancements; }

std::optional<int> findAdvancement(std::string_view id) {
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    for (size_t i = 0; i < std::size(kAdvancements); ++i)
        if (kAdvancements[i].id == id) return int(i);
    return std::nullopt;
}

int Advancements::doneCount() const {
    int n = 0;
    for (size_t i = 0; i < std::size(kAdvancements); ++i) n += m_done[i];
    return n;
}

bool Advancements::grant(int i) {
    if (i < 0 || size_t(i) >= std::size(kAdvancements) || m_done[size_t(i)]) return false;
    m_done[size_t(i)] = true;
    m_when[size_t(i)] = now();
    return true;
}

template <typename F> Advancements::Granted Advancements::grantWhere(F&& match) {
    Granted out;
    out.fill(-1);
    size_t n = 0;
    for (size_t i = 0; i < std::size(kAdvancements) && n < out.size(); ++i)
        if (!m_done[i] && match(kAdvancements[i]) && grant(int(i))) out[n++] = int(i);
    return out;
}

Advancements::Granted Advancements::onItem(std::string_view itemId) {
    if (itemId.starts_with("minecraft:")) itemId.remove_prefix(10);
    return grantWhere([&](const Advancement& a) {
        for (const std::string_view it : a.items)
            if (!it.empty() && it == itemId) return true;
        return false;
    });
}

Advancements::Granted Advancements::onKill(MobType type) {
    return grantWhere([&](const Advancement& a) {
        if (a.id == "adventure/kill_a_mob") return mobInfo(type).hostile; // (a monster)
        return a.anyKill || (a.kill != MobType::Count && a.kill == type);
    });
}

Advancements::Granted Advancements::onEvent(AdvEvent e) {
    return grantWhere([&](const Advancement& a) { return a.event == e && e != AdvEvent::None; });
}

std::string Advancements::toJson() const {
    // Vanilla's layout: {"minecraft:<id>": {"criteria": {"<name>": "<time>"}, "done": true}, "DataVersion": 4671}
    std::string out = "{";
    for (size_t i = 0; i < std::size(kAdvancements); ++i) {
        if (!m_done[i]) continue;
        out += "\n  \"minecraft:" + std::string(kAdvancements[i].id) + "\": {\n    \"criteria\": {\n      \"requirement\": \"" +
               (m_when[i].empty() ? now() : m_when[i]) + "\"\n    },\n    \"done\": true\n  },";
    }
    out += "\n  \"DataVersion\": 4671\n}\n";
    return out;
}

bool Advancements::fromJson(std::string_view json) {
    // Tolerant reading: each "minecraft:<id>" whose object says "done": true.
    if (json.find('{') == std::string_view::npos) return false;
    size_t at = 0;
    while ((at = json.find("\"minecraft:", at)) != std::string_view::npos) {
        const size_t end = json.find('"', at + 1);
        if (end == std::string_view::npos) return false;
        const std::string_view id = json.substr(at + 1, end - at - 1);
        const size_t close = json.find("\"done\"", end);
        const size_t next = json.find("\"minecraft:", end);
        if (const auto i = findAdvancement(id); i && close != std::string_view::npos && (next == std::string_view::npos || close < next)) {
            const std::string_view rest = json.substr(close + 6, 12);
            if (rest.find("true") != std::string_view::npos) {
                m_done[size_t(*i)] = true;
                const size_t q = json.find("\"requirement\": \"", end);
                if (q != std::string_view::npos && q < close) {
                    const size_t qe = json.find('"', q + 16);
                    if (qe != std::string_view::npos) m_when[size_t(*i)] = std::string(json.substr(q + 16, qe - q - 16));
                }
            }
        }
        at = end;
    }
    return true;
}

std::filesystem::path Advancements::file(const std::filesystem::path& worldDir, uint64_t hi, uint64_t lo) {
    char name[48];
    std::snprintf(name, sizeof(name), "%08x-%04x-%04x-%04x-%012llx.json", unsigned(hi >> 32), unsigned(hi >> 16 & 0xFFFF),
                  unsigned(hi & 0xFFFF), unsigned(lo >> 48), static_cast<unsigned long long>(lo & 0xFFFFFFFFFFFFull));
    return worldDir / "advancements" / name;
}

bool Advancements::save(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    const std::string json = toJson();
    f.write(json.data(), std::streamsize(json.size()));
    return bool(f);
}

std::optional<Advancements> Advancements::load(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    Advancements a;
    if (!a.fromJson(ss.str())) {
        MC_LOG_WARN("Unreadable advancements file %s", path.string().c_str());
        return std::nullopt;
    }
    return a;
}

} // namespace mc::world
