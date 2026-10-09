// Completeness (M29): every block, item, entity, effect and enchantment id of Java
// Edition 1.21.11 (tests/data/ids_1_21_11.txt, from the wiki's data values; the 26.x
// additions left out) exists in the game.
#include "world/Blocks.h"
#include "world/Enchantments.h"
#include "world/Items.h"
#include "world/Mob.h"
#include "world/Potions.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace mc::world;

TEST_CASE("every 1.21.11 id is in the game (M29 completeness)") {
    std::ifstream in(std::filesystem::path(MC_TEST_DATA_DIR) / "ids_1_21_11.txt");
    REQUIRE(in);
    // Entities that are not mob types here: projectiles, boats and carts by wood/kind, and
    // the ones other systems keep (gameplay/Projectiles, ItemEntities, FallingBlocks...).
    static const std::set<std::string> kElsewhere = {
        "arrow",          "spectral_arrow",  "trident",          "egg",           "ender_pearl",    "eye_of_ender",
        "fireball",       "small_fireball",  "dragon_fireball",  "wither_skull",  "shulker_bullet", "llama_spit",
        "snowball",       "splash_potion",   "lingering_potion", "experience_bottle", "wind_charge",
        "breeze_wind_charge", "firework_rocket", "fishing_bobber", "evoker_fangs", "area_effect_cloud",
        "experience_orb", "item",            "falling_block",    "tnt",           "lightning_bolt", "player",
        "leash_knot",     "potion"};
    int checked = 0;
    std::string kind, id;
    while (in >> kind) {
        if (kind.starts_with("#")) {
            std::getline(in, id);
            continue;
        }
        in >> id;
        INFO(kind << " " << id);
        ++checked;
        if (kind == "block") CHECK(blockRegistry().findBlock("minecraft:" + id).has_value());
        else if (kind == "item") CHECK((itemRegistry().find("minecraft:" + id).has_value() || id == "air"));
        else if (kind == "effect") CHECK(findEffect("minecraft:" + id).has_value());
        else if (kind == "enchantment") CHECK(findEnchantment(id).has_value());
        else if (kind == "entity") {
            bool known = kElsewhere.count(id) > 0;
            for (int t = 0; t < int(MobType::Count) && !known; ++t)
                known = mobInfo(MobType(t)).id.substr(10) == id;
            for (int w = 0; w < 10 && !known; ++w)
                known = boatId(w) == "minecraft:" + id || chestBoatId(w) == "minecraft:" + id;
            for (const char* k : kCartKinds) known = known || id == k;
            CHECK(known);
        }
    }
    CHECK(checked > 1800);
}
