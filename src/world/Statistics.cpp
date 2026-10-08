#include "world/Statistics.h"

#include "core/Log.h"
#include "world/BlockRegistry.h"
#include "world/Blocks.h"

#include <charconv>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace mc::world {

namespace {

struct StatInfo {
    std::string_view id, label;
    Statistics::Format format;
};

using F = Statistics::Format;
// Vanilla's ids and the Statistics screen's General tab labels (wiki: Statistics).
constexpr StatInfo kStats[] = {
    {"minecraft:play_time", "Time Played", F::Time},
    {"minecraft:total_world_time", "Time with World Open", F::Time},
    {"minecraft:time_since_death", "Time Since Last Death", F::Time},
    {"minecraft:time_since_rest", "Time Since Last Rest", F::Time},
    {"minecraft:leave_game", "Games Quit", F::Count},
    {"minecraft:deaths", "Number of Deaths", F::Count},
    {"minecraft:mob_kills", "Mob Kills", F::Count},
    {"minecraft:damage_dealt", "Damage Dealt", F::Damage},
    {"minecraft:damage_taken", "Damage Taken", F::Damage},
    {"minecraft:jump", "Jumps", F::Count},
    {"minecraft:drop", "Items Dropped", F::Count},
    {"minecraft:sleep_in_bed", "Times Slept in a Bed", F::Count},
    {"minecraft:animals_bred", "Animals Bred", F::Count},
    {"minecraft:fish_caught", "Fish Caught", F::Count},
    {"minecraft:traded_with_villager", "Traded with Villagers", F::Count},
    {"minecraft:enchant_item", "Items Enchanted", F::Count},
    {"minecraft:open_chest", "Chests Opened", F::Count},
    {"minecraft:interact_with_crafting_table", "Interactions with Crafting Table", F::Count},
    {"minecraft:interact_with_furnace", "Interactions with Furnace", F::Count},
    {"minecraft:walk_one_cm", "Distance Walked", F::Distance},
    {"minecraft:sprint_one_cm", "Distance Sprinted", F::Distance},
    {"minecraft:crouch_one_cm", "Distance Crouched", F::Distance},
    {"minecraft:swim_one_cm", "Distance Swum", F::Distance},
    {"minecraft:walk_on_water_one_cm", "Distance Walked on Water", F::Distance},
    {"minecraft:walk_under_water_one_cm", "Distance Walked under Water", F::Distance},
    {"minecraft:fall_one_cm", "Distance Fallen", F::Distance},
    {"minecraft:climb_one_cm", "Distance Climbed", F::Distance},
    {"minecraft:fly_one_cm", "Distance Flown", F::Distance},
    {"minecraft:aviate_one_cm", "Distance by Elytra", F::Distance},
    {"minecraft:boat_one_cm", "Distance by Boat", F::Distance},
    {"minecraft:minecart_one_cm", "Distance by Minecart", F::Distance},
    {"minecraft:horse_one_cm", "Distance by Horse", F::Distance},
};
static_assert(std::size(kStats) == size_t(Stat::Count));

constexpr std::string_view kItemStatIds[] = {"minecraft:mined",  "minecraft:crafted",   "minecraft:used",
                                             "minecraft:broken", "minecraft:picked_up", "minecraft:dropped"};
static_assert(std::size(kItemStatIds) == size_t(ItemStat::Count));

void appendGroup(std::string& out, bool& firstGroup, std::string_view group, auto&& each) {
    std::string body;
    bool first = true;
    each([&](std::string_view key, int64_t v) {
        if (v == 0) return;
        body += first ? "\n      \"" : ",\n      \"";
        body += key;
        body += "\": ";
        body += std::to_string(v);
        first = false;
    });
    if (first) return; // (vanilla leaves empty groups out)
    out += firstGroup ? "\n    \"" : ",\n    \"";
    out += group;
    out += "\": {";
    out += body;
    out += "\n    }";
    firstGroup = false;
}

// A tiny reader for the stats file: objects of strings to objects or integers.
struct Reader {
    std::string_view s;
    size_t i = 0;
    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t'))
            ++i;
    }
    bool eat(char c) {
        ws();
        if (i < s.size() && s[i] == c) {
            ++i;
            return true;
        }
        return false;
    }
    std::optional<std::string_view> str() {
        if (!eat('"')) return std::nullopt;
        const size_t start = i;
        while (i < s.size() && s[i] != '"')
            ++i;
        if (i >= s.size()) return std::nullopt;
        return s.substr(start, i++ - start);
    }
    std::optional<int64_t> num() {
        ws();
        int64_t v = 0;
        const auto r = std::from_chars(s.data() + i, s.data() + s.size(), v);
        if (r.ec != std::errc()) return std::nullopt;
        i = size_t(r.ptr - s.data());
        return v;
    }
    // Calls f(key) for each member; f reads the value. False on malformed input.
    template <typename Fn> bool object(Fn&& f) {
        if (!eat('{')) return false;
        if (eat('}')) return true;
        do {
            const auto key = str();
            if (!key || !eat(':') || !f(*key)) return false;
        } while (eat(','));
        return eat('}');
    }
    bool skipValue() {
        ws();
        if (i < s.size() && s[i] == '{') return object([&](std::string_view) { return skipValue(); });
        if (i < s.size() && s[i] == '"') return str().has_value();
        return num().has_value();
    }
};

} // namespace

Statistics::Statistics()
    : m_mined(blockRegistry().blockCount(), 0), m_items(itemRegistry().count()),
      m_perItem(size_t(ItemStat::Count) * itemRegistry().count(), 0) {}

std::string_view Statistics::id(Stat s) { return kStats[size_t(s)].id; }
std::string_view Statistics::label(Stat s) { return kStats[size_t(s)].label; }
Statistics::Format Statistics::format(Stat s) { return kStats[size_t(s)].format; }

void Statistics::formatValue(Stat s, int64_t v, char* out, size_t size) {
    // Vanilla's formatters: times in s/min/h/d/y, distances in cm/m/km (two decimals),
    // damage in tenths shown as hearts' worth.
    switch (format(s)) {
    case Format::Time: {
        const double sec = double(v) / 20.0, min = sec / 60.0, h = min / 60.0, d = h / 24.0, y = d / 365.0;
        if (y > 0.5) std::snprintf(out, size, "%.2f y", y);
        else if (d > 0.5) std::snprintf(out, size, "%.2f d", d);
        else if (h > 0.5) std::snprintf(out, size, "%.2f h", h);
        else if (min > 0.5) std::snprintf(out, size, "%.2f min", min);
        else std::snprintf(out, size, "%.2f s", sec);
        break;
    }
    case Format::Distance: {
        const double m = double(v) / 100.0, km = m / 1000.0;
        if (km > 0.5) std::snprintf(out, size, "%.2f km", km);
        else if (m > 0.5) std::snprintf(out, size, "%.2f m", m);
        else std::snprintf(out, size, "%lld cm", static_cast<long long>(v));
        break;
    }
    case Format::Damage: std::snprintf(out, size, "%.1f", double(v) / 10.0); break;
    case Format::Count: std::snprintf(out, size, "%lld", static_cast<long long>(v)); break;
    }
}

std::string Statistics::toJson() const {
    std::string out = "{\n  \"stats\": {";
    bool firstGroup = true;
    appendGroup(out, firstGroup, "minecraft:custom", [&](auto&& put) {
        for (size_t s = 0; s < m_custom.size(); ++s)
            put(kStats[s].id, m_custom[s]);
    });
    appendGroup(out, firstGroup, kItemStatIds[0], [&](auto&& put) {
        for (size_t b = 0; b < m_mined.size(); ++b)
            put(blockRegistry().block(BlockId(b)).id, m_mined[b]);
    });
    for (size_t k = 1; k < size_t(ItemStat::Count); ++k)
        appendGroup(out, firstGroup, kItemStatIds[k], [&](auto&& put) {
            for (size_t it = 1; it < m_items; ++it)
                put(itemRegistry().item(ItemId(it)).id, m_perItem[k * m_items + it]);
        });
    appendGroup(out, firstGroup, "minecraft:killed", [&](auto&& put) {
        for (size_t t = 0; t < m_killed.size(); ++t)
            put(mobInfo(MobType(t)).id, m_killed[t]);
    });
    appendGroup(out, firstGroup, "minecraft:killed_by", [&](auto&& put) {
        for (size_t t = 0; t < m_killedBy.size(); ++t)
            put(mobInfo(MobType(t)).id, m_killedBy[t]);
    });
    out += "\n  },\n  \"DataVersion\": 4671\n}\n";
    return out;
}

bool Statistics::fromJson(std::string_view json) {
    Reader r{json};
    auto findMob = [](std::string_view id) -> int {
        for (size_t t = 0; t < size_t(MobType::Count); ++t)
            if (mobInfo(MobType(t)).id == id) return int(t);
        return -1;
    };
    return r.object([&](std::string_view top) {
        if (top != "stats") return r.skipValue();
        return r.object([&](std::string_view group) {
            return r.object([&](std::string_view key) {
                const auto v = r.num();
                if (!v) return false;
                if (group == "minecraft:custom") {
                    for (size_t s = 0; s < size_t(Stat::Count); ++s)
                        if (kStats[s].id == key) m_custom[s] = *v;
                } else if (group == kItemStatIds[0]) {
                    if (const auto b = blockRegistry().findBlock(key)) m_mined[*b] = *v;
                } else if (group == "minecraft:killed" || group == "minecraft:killed_by") {
                    if (const int t = findMob(key); t >= 0) (group == "minecraft:killed" ? m_killed : m_killedBy)[size_t(t)] = *v;
                } else {
                    for (size_t k = 1; k < size_t(ItemStat::Count); ++k)
                        if (group == kItemStatIds[k])
                            if (const auto it = itemRegistry().find(key)) m_perItem[k * m_items + *it] = *v;
                }
                return true;
            });
        });
    });
}

std::filesystem::path Statistics::file(const std::filesystem::path& worldDir, uint64_t hi, uint64_t lo) {
    char name[48];
    std::snprintf(name, sizeof(name), "%08x-%04x-%04x-%04x-%012llx.json", unsigned(hi >> 32), unsigned(hi >> 16 & 0xFFFF),
                  unsigned(hi & 0xFFFF), unsigned(lo >> 48), static_cast<unsigned long long>(lo & 0xFFFFFFFFFFFFull));
    return worldDir / "stats" / name;
}

bool Statistics::save(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    const std::string json = toJson();
    f.write(json.data(), std::streamsize(json.size()));
    return bool(f);
}

std::optional<Statistics> Statistics::load(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    Statistics s;
    if (!s.fromJson(ss.str())) {
        MC_LOG_WARN("Unreadable statistics file %s", path.string().c_str());
        return std::nullopt;
    }
    return s;
}

} // namespace mc::world
