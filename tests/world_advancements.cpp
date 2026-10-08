#include "world/Advancements.h"

#include <doctest/doctest.h>

#include <set>
#include <string>

using namespace mc::world;

namespace {
int count(const Advancements::Granted& g) {
    int n = 0;
    for (const int i : g)
        n += i >= 0;
    return n;
}
bool has(const Advancements::Granted& g, std::string_view id) {
    const auto want = findAdvancement(id);
    for (const int i : g)
        if (want && i == *want) return true;
    return false;
}
} // namespace

TEST_CASE("advancements: table ids are unique and fit") {
    std::set<std::string> ids;
    for (const Advancement& a : advancements()) {
        CHECK(ids.insert(std::string(a.id)).second);
        CHECK(!a.title.empty());
        CHECK(a.tab != AdvTab::Count);
    }
    CHECK(int(advancements().size()) <= kMaxAdvancements);
    CHECK(findAdvancement("minecraft:story/mine_stone"));
    CHECK(findAdvancement("story/mine_stone") == findAdvancement("minecraft:story/mine_stone"));
    CHECK(!findAdvancement("story/nonsense"));
}

TEST_CASE("advancements: items, kills and events grant once") {
    Advancements a;
    const auto g = a.onItem("minecraft:cobblestone");
    CHECK(has(g, "story/mine_stone"));
    CHECK(count(a.onItem("cobblestone")) == 0); // (already made)
    CHECK(has(a.onItem("minecraft:crafting_table"), "story/root"));

    // Any kill makes the Adventure root; a monster also "Monster Hunter"; a cow only the root.
    Advancements b;
    const auto cow = b.onKill(MobType::Cow);
    CHECK(has(cow, "adventure/root"));
    CHECK(!has(cow, "adventure/kill_a_mob"));
    CHECK(has(b.onKill(MobType::Zombie), "adventure/kill_a_mob"));
    CHECK(has(b.onKill(MobType::Ghast), "nether/return_to_sender"));

    CHECK(has(b.onEvent(AdvEvent::EnterNether), "story/enter_the_nether"));
    CHECK(has(b.onEvent(AdvEvent::Bred), "husbandry/breed_an_animal"));
    CHECK(count(b.onEvent(AdvEvent::Bred)) == 0);
    CHECK(count(b.onEvent(AdvEvent::None)) == 0);
    CHECK(b.doneCount() == 6);
}

TEST_CASE("advancements: JSON round trip in vanilla's layout") {
    Advancements a;
    a.onItem("diamond");
    a.onEvent(AdvEvent::EnterEnd);
    const std::string json = a.toJson();
    CHECK(json.find("\"minecraft:story/mine_diamond\"") != std::string::npos);
    CHECK(json.find("\"done\": true") != std::string::npos);
    CHECK(json.find("\"DataVersion\": 4671") != std::string::npos);
    Advancements b;
    REQUIRE(b.fromJson(json));
    CHECK(b.doneCount() == a.doneCount());
    CHECK(b.done(*findAdvancement("story/mine_diamond")));
    CHECK(b.done(*findAdvancement("story/enter_the_end")));
    CHECK(!b.done(*findAdvancement("story/mine_stone")));
    CHECK(!b.fromJson("not json"));
}

TEST_CASE("advancements: saved under advancements/<uuid>.json") {
    const auto p = Advancements::file("w", 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
    CHECK(p.parent_path().filename() == "advancements");
    CHECK(p.filename().string() == "01234567-89ab-cdef-fedc-ba9876543210.json");
}
