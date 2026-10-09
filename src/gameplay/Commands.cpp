#include "gameplay/Commands.h"

#include "world/Enchantments.h"
#include "world/ItemExtras.h"

#include "world/Potions.h"

#include "gameplay/Mobs.h"
#include "world/Blocks.h"
#include "world/DayTime.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace mc {

namespace {

std::vector<std::string_view> split(std::string_view s) {
    std::vector<std::string_view> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ')
            ++i;
        const size_t start = i;
        while (i < s.size() && s[i] != ' ')
            ++i;
        if (i > start) out.push_back(s.substr(start, i - start));
    }
    return out;
}

template <typename T> std::optional<T> number(std::string_view s) {
    T v{};
    if (s.empty()) return std::nullopt;
    if (s.front() == '+') s.remove_prefix(1);
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || p != s.data() + s.size()) return std::nullopt;
    if constexpr (std::is_floating_point_v<T>)
        if (!std::isfinite(v)) return std::nullopt; // from_chars accepts nan/inf
    return v;
}

// A coordinate: absolute, or ~ / ~N relative to `base` (wiki: Coordinates). Absolute
// whole numbers for x/z are block centres (+0.5), as vanilla does for /tp.
std::optional<double> coordinate(std::string_view s, double base, bool centre) {
    if (!s.empty() && s.front() == '~') {
        if (s.size() == 1) return base;
        const auto d = number<double>(s.substr(1));
        return d ? std::optional<double>(base + *d) : std::nullopt;
    }
    const auto v = number<double>(s);
    if (!v) return std::nullopt;
    const bool whole = s.find('.') == std::string_view::npos;
    return centre && whole ? *v + 0.5 : *v;
}

bool isSelf(std::string_view s) { return s == "@s" || s == "@p"; }

// Time values: plain ticks, or with a unit suffix d (days), s (seconds), t (ticks).
std::optional<int64_t> timeValue(std::string_view s) {
    double scale = 1.0;
    if (!s.empty() && (s.back() == 'd' || s.back() == 's' || s.back() == 't')) {
        scale = s.back() == 'd' ? 24000.0 : s.back() == 's' ? 20.0 : 1.0;
        s.remove_suffix(1);
    }
    const auto v = number<double>(s);
    if (!v || *v < 0) return std::nullopt;
    const double ticks = *v * scale;
    if (ticks > 2147483647.0) return std::nullopt; // vanilla's time argument is an int
    return static_cast<int64_t>(std::llround(ticks));
}

CommandResult fail(std::string msg) { return {false, std::move(msg)}; }

std::string format(const char* fmt, auto... args) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), fmt, args...);
    return buf;
}

CommandResult teleport(const std::vector<std::string_view>& a, CommandContext& ctx) {
    // /tp x y z [yaw pitch] or /tp @s x y z [yaw pitch]
    size_t i = 1;
    if (i < a.size() && isSelf(a[i]))
        ++i;
    else if (i < a.size() && a[i].front() == '@')
        return fail("Only @s/@p is supported");
    if (a.size() - i != 3 && a.size() - i != 5)
        return fail("Usage: /tp [@s] <x> <y> <z> [yaw pitch]");
    const glm::dvec3 p = ctx.player.position();
    const auto x = coordinate(a[i], p.x, true);
    const auto y = coordinate(a[i + 1], p.y, false);
    const auto z = coordinate(a[i + 2], p.z, true);
    if (!x || !y || !z) return fail("Invalid position");
    // Vanilla's teleport range (wiki: Commands/teleport).
    if (std::abs(*x) >= 30000000 || std::abs(*z) >= 30000000 || std::abs(*y) >= 20000000)
        return fail("Invalid position for teleport");
    float yaw = ctx.player.yaw(), pitch = ctx.player.pitch();
    if (a.size() - i == 5) {
        const auto yw = coordinate(a[i + 3], yaw, false);
        const auto pt = coordinate(a[i + 4], pitch, false);
        if (!yw || !pt) return fail("Invalid rotation");
        yaw = static_cast<float>(*yw);
        pitch = static_cast<float>(*pt);
    }
    ctx.player.setPosition({*x, *y, *z});
    if (ctx.vitals) ctx.vitals->resetFall(); // a teleport is not a fall
    ctx.player.setRotation(yaw, pitch);
    return {true, format("Teleported Player to %.2f, %.2f, %.2f", *x, *y, *z)};
}

CommandResult time(const std::vector<std::string_view>& a, CommandContext& ctx) {
    if (a.size() < 3) return fail("Usage: /time set|add|query ...");
    if (a[1] == "set") {
        // Named times (wiki: Commands/time).
        std::optional<int64_t> t;
        if (a[2] == "day")
            t = 1000;
        else if (a[2] == "noon")
            t = 6000;
        else if (a[2] == "night")
            t = 13000;
        else if (a[2] == "midnight")
            t = 18000;
        else
            t = timeValue(a[2]);
        if (!t) return fail("Invalid time");
        // `set` sets the absolute day time: the day count (and moon phase) restart
        // (wiki: Commands/time).
        ctx.dayTime = *t;
        return {true, format("Set the time to %lld", static_cast<long long>(*t))};
    }
    if (a[1] == "add") {
        const auto t = timeValue(a[2]);
        if (!t) return fail("Invalid time");
        ctx.dayTime += *t;
        return {true, format("Set the time to %lld",
                             static_cast<long long>(ctx.dayTime % world::kTicksPerDay))};
    }
    if (a[1] == "query") {
        long long v = 0;
        if (a[2] == "daytime")
            v = ctx.dayTime % world::kTicksPerDay;
        else if (a[2] == "gametime")
            v = ctx.gameTime;
        else if (a[2] == "day")
            v = ctx.dayTime / world::kTicksPerDay;
        else
            return fail("Usage: /time query daytime|gametime|day");
        return {true, format("The time is %lld", v)};
    }
    return fail("Usage: /time set|add|query ...");
}

// An item with its components as commands write it: "oak_log[axis=x]",
// "potion[potion_contents={potion:\"swiftness\"}]", "red_banner[banner_patterns=[...]]".
std::optional<world::ItemStack> parseStack(std::string_view id, std::string& error) {
    auto failed = [&](std::string msg) -> std::optional<world::ItemStack> {
        error = std::move(msg);
        return std::nullopt;
    };
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    const std::string_view name = id.substr(0, id.find('['));
    const auto item = world::itemRegistry().find(name);
    if (!item || *item == world::kNoItem)
        return failed(format("Unknown item '%.*s'", int(id.size()), id.data()));
    world::ItemStack stack{*item, 1};
    if (const size_t fw = id.find("[fireworks={"); fw != std::string_view::npos) {
        // firework_rocket[fireworks={flight_duration:2,explosions:[{shape:"large_ball",colors:[I;11743532],
        // has_trail:true}]}] (M28.4c; vanilla's component, colours as RGB ints)
        world::Fireworks f;
        std::string_view rest = id.substr(fw);
        if (const size_t d = rest.find("flight_duration:"); d != std::string_view::npos) {
            const auto n =
                number<int64_t>(rest.substr(d + 16, rest.find_first_of(",}", d + 16) - d - 16));
            f.flight = uint8_t(std::clamp<int64_t>(n.value_or(1), 0, 3));
        }
        for (size_t s = rest.find("shape:");
             s != std::string_view::npos && f.count < world::Fireworks::kMax;
             s = rest.find("shape:", s + 6)) {
            world::FireworkExplosion e;
            std::string_view shape = rest.substr(s + 6);
            if (!shape.empty() && shape.front() == '"') shape.remove_prefix(1);
            shape = shape.substr(0, shape.find_first_of("\",}"));
            for (int k = 0; k < 5; ++k)
                if (shape == world::kFireworkShapes[k]) e.shape = uint8_t(k);
            const size_t end = rest.find('}', s);
            const std::string_view body =
                rest.substr(s, end == std::string_view::npos ? std::string_view::npos : end - s);
            auto colours = [&](std::string_view key) {
                uint16_t m = 0;
                const size_t c = body.find(key);
                if (c == std::string_view::npos) return m;
                std::string_view list = body.substr(c + key.size());
                list = list.substr(0, list.find(']'));
                while (!list.empty()) {
                    const size_t comma = list.find(',');
                    const auto rgb = number<int64_t>(list.substr(0, comma));
                    if (rgb) {
                        int best = 0;
                        long bestD = -1;
                        for (int d = 0; d < 16; ++d) {
                            const uint32_t k = world::kFireworkColours[d];
                            const long dr = long(*rgb >> 16 & 255) - long(k >> 16 & 255),
                                       dg = long(*rgb >> 8 & 255) - long(k >> 8 & 255),
                                       db = long(*rgb & 255) - long(k & 255);
                            if (bestD < 0 || dr * dr + dg * dg + db * db < bestD)
                                bestD = dr * dr + dg * dg + db * db, best = d;
                        }
                        m = uint16_t(m | (1u << best));
                    }
                    if (comma == std::string_view::npos) break;
                    list.remove_prefix(comma + 1);
                }
                return m;
            };
            e.colours = colours("colors:[I;");
            e.fades = colours("fade_colors:[I;");
            e.trail = body.find("has_trail:true") != std::string_view::npos ||
                      body.find("has_trail:1") != std::string_view::npos;
            e.twinkle = body.find("has_twinkle:true") != std::string_view::npos ||
                        body.find("has_twinkle:1") != std::string_view::npos;
            f.explosions[f.count++] = e;
        }
        stack.extra = world::addFireworks(f);
    } else if (id.find("[charged_projectiles=") != std::string_view::npos) {
        stack.state = 1; // (M28.4a) a crossbow loaded with an arrow
    } else if (const size_t bp = id.find("[banner_patterns=["); bp != std::string_view::npos) {
        // red_banner[banner_patterns=[{pattern:"cross",color:"white"},...]] (M28.3d; vanilla
        // components)
        world::BannerLayers layers;
        std::string_view rest = id.substr(bp + 18);
        while (layers.count < world::BannerLayers::kMax) {
            const size_t p = rest.find("pattern:"), c = rest.find("color:");
            if (p == std::string_view::npos || c == std::string_view::npos) break;
            auto value = [&](size_t at) {
                std::string_view v = rest.substr(at);
                if (!v.empty() && v.front() == '"') v.remove_prefix(1);
                return v.substr(0, v.find_first_of("\",}]"));
            };
            const auto pat = world::findBannerPattern(value(p + 8));
            const std::string_view col = value(c + 6);
            int dye = -1;
            for (int k = 0; k < 16; ++k)
                if (col == world::kDyeColours[k]) dye = k;
            if (!pat || dye < 0) return failed("Unknown banner pattern or colour");
            layers.pattern[layers.count] = uint8_t(*pat);
            layers.colour[layers.count] = uint8_t(dye);
            ++layers.count;
            rest = rest.substr(std::max(p, c) + 6);
            if (const size_t next = rest.find('{'); next != std::string_view::npos)
                rest = rest.substr(next);
            else
                break;
        }
        stack.extra = world::addBannerLayers(layers);
    } else if (const size_t pc = id.find("[potion_contents={potion:");
               pc != std::string_view::npos) {
        // potion[potion_contents={potion:"minecraft:swiftness"}] (vanilla components)
        std::string_view rest = id.substr(pc + 25);
        if (!rest.empty() && rest.front() == '"') rest.remove_prefix(1);
        rest = rest.substr(0, rest.find_first_of("\"}"));
        const auto potion = world::findPotion(rest);
        if (!potion) return failed(format("Unknown potion '%.*s'", int(rest.size()), rest.data()));
        stack.potion = static_cast<uint8_t>(*potion);
    } else if (const size_t cn = id.find("[custom_name="); cn != std::string_view::npos) {
        // name_tag[custom_name="Dinnerbone"] (M29.3b; vanilla's component, quotes optional)
        std::string_view v = id.substr(cn + 13);
        v = v.substr(0, v.rfind(']'));
        while (!v.empty() && (v.front() == '"' || v.front() == '\'')) v.remove_prefix(1);
        while (!v.empty() && (v.back() == '"' || v.back() == '\'')) v.remove_suffix(1);
        stack.name = world::addName(v);
    } else if (const size_t en = id.find("[enchantments="); en != std::string_view::npos) {
        // diamond_sword[enchantments={sharpness:5,"minecraft:mending":1}] (M29.2b; vanilla's
        // component; the older {levels:{...}} wrapper is accepted too)
        std::string_view rest = id.substr(en + 14);
        if (const size_t lv = rest.find("levels:{"); lv != std::string_view::npos) rest = rest.substr(lv + 8);
        if (!rest.empty() && rest.front() == '{') rest.remove_prefix(1);
        rest = rest.substr(0, rest.find('}'));
        while (!rest.empty()) {
            const size_t comma = rest.find(',');
            std::string_view pair = rest.substr(0, comma);
            const size_t colon = pair.rfind(':');
            if (colon == std::string_view::npos) return failed("Invalid enchantments");
            std::string_view key = pair.substr(0, colon);
            if (!key.empty() && key.front() == '"') key = key.substr(1, key.size() - 2);
            const auto e = world::findEnchantment(key);
            const auto level = number<int64_t>(pair.substr(colon + 1));
            if (!e || !level) return failed(format("Unknown enchantment '%.*s'", int(key.size()), key.data()));
            world::setEnchantment(stack, *e, int(std::clamp<int64_t>(*level, 1, 255)));
            if (comma == std::string_view::npos) break;
            rest.remove_prefix(comma + 1);
        }
    } else if (name.size() != id.size()) { // a block state
        const auto state = world::blockRegistry().parse(id);
        if (!state) return failed(format("Unknown item '%.*s'", int(id.size()), id.data()));
        stack = Inventory::blockStack(*state);
    }
    return stack;
}

CommandResult give(const std::vector<std::string_view>& a, CommandContext& ctx) {
    // /give @s <item> [count] (wiki: Commands/give): into the inventory, stacking
    // like picked-up items. Block items may carry a state (oak_log[axis=x]).
    if (a.size() < 3 || a.size() > 4 || !isSelf(a[1]))
        return fail("Usage: /give @s <item> [count]");
    int count = 1;
    if (a.size() == 4) {
        const auto n = number<int64_t>(a[3]);
        if (!n || *n < 1 || *n > 2147483647) return fail("Invalid count");
        count = static_cast<int>(std::min<int64_t>(*n, 36 * 64)); // at most a full inventory
    }
    std::string_view id = a[2];
    std::string error;
    auto parsed = parseStack(id, error);
    if (!parsed) return fail(error);
    world::ItemStack stack = *parsed;
    const int max = world::itemRegistry().item(stack.item).maxStack;
    int given = 0;
    for (int left = count; left > 0;) {
        stack.count = static_cast<uint8_t>(std::min(left, max));
        const int n = stack.count;
        const int rest = ctx.inventory.add(stack);
        given += n - rest;
        if (rest > 0) break; // inventory full (vanilla would drop the rest)
        left -= n;
    }
    return {true, format("Gave %d [%.*s] to Player", given, int(id.size()), id.data())};
}

CommandResult item(const std::vector<std::string_view>& a, CommandContext& ctx) {
    // /item replace entity @s <slot> with <item> [count] (wiki: Commands/item): slots
    // weapon.mainhand, weapon.offhand, hotbar.0-8, inventory.0-26, armor.head/chest/legs/feet.
    if (a.size() < 7 || a.size() > 8 || a[1] != "replace" || a[2] != "entity" || !isSelf(a[3]) ||
        a[5] != "with")
        return fail("Usage: /item replace entity @s <slot> with <item> [count]");
    std::string_view id = a[6];
    std::string error;
    const auto parsed = parseStack(id, error);
    if (!parsed) return fail(error);
    const auto it = std::optional<world::ItemId>(parsed->item);
    int count = 1;
    if (a.size() == 8) {
        const auto n = number<int64_t>(a[7]);
        if (!n || *n < 1 || *n > 99) return fail("Invalid count");
        count = int(std::min<int64_t>(*n, world::itemRegistry().item(*it).maxStack));
    }
    world::ItemStack stack = *parsed;
    stack.count = uint8_t(count);
    const std::string_view slot = a[4];
    static constexpr std::string_view kArmor[4] = {"armor.head", "armor.chest", "armor.legs",
                                                   "armor.feet"};
    if (slot == "weapon.mainhand" || slot == "weapon") {
        ctx.inventory.setSlot(ctx.inventory.selected(), stack);
    } else if (slot == "weapon.offhand") {
        ctx.inventory.setOffhand(stack);
    } else if (slot.starts_with("hotbar.") || slot.starts_with("inventory.")) {
        const bool hotbar = slot.starts_with("hotbar.");
        const auto n = number<int64_t>(slot.substr(hotbar ? 7 : 10));
        if (!n || *n < 0 || *n > (hotbar ? 8 : 26)) return fail("Unknown slot");
        ctx.inventory.setSlot(int(*n) + (hotbar ? 0 : 9), stack);
    } else {
        int piece = -1;
        for (int k = 0; k < 4; ++k)
            if (slot == kArmor[k]) piece = k;
        if (piece < 0) return fail(format("Unknown slot '%.*s'", int(slot.size()), slot.data()));
        ctx.inventory.setArmor(piece, stack);
    }
    return {true, format("Replaced a slot on Player with [%.*s]", int(id.size()), id.data())};
}

} // namespace

CommandResult runCommand(std::string_view line, CommandContext& ctx) {
    if (!line.empty() && line.front() == '/') line.remove_prefix(1);
    const auto a = split(line);
    if (a.empty()) return fail("");
    if (a[0] == "tp" || a[0] == "teleport") return teleport(a, ctx);
    if (a[0] == "time") return time(a, ctx);
    if (a[0] == "give") return give(a, ctx);
    if (a[0] == "enchant") { // /enchant @s <enchantment> [level] (M29.2b; wiki: Commands/enchant)
        if (a.size() < 3 || a.size() > 4 || (a[1] != "@s" && a[1] != "@p")) return fail("Usage: /enchant @s <enchantment> [level]");
        const auto e = world::findEnchantment(a[2]);
        if (!e) return fail(format("Unknown enchantment '%.*s'", int(a[2].size()), a[2].data()));
        const int level = a.size() == 4 ? int(number<int64_t>(a[3]).value_or(0)) : 1;
        if (level < 1 || level > world::enchantmentInfo(*e).maxLevel) return fail("Invalid level");
        world::ItemStack held = ctx.inventory.selectedStack();
        if (held.empty() || !world::canEnchant(held.item, *e)) return fail("Player can't accept that enchantment");
        world::setEnchantment(held, *e, level);
        ctx.inventory.setSlot(ctx.inventory.selected(), held);
        return {true, format("Applied enchantment %.*s to Player's item", int(world::enchantmentInfo(*e).name.size()),
                         world::enchantmentInfo(*e).name.data())};
    }
    if (a[0] == "item") return item(a, ctx);
    if (a[0] == "gamemode") {
        // /gamemode survival|creative|adventure|spectator (wiki: Commands/gamemode; the
        // number ids were removed). Adventure plays as survival, spectator as creative.
        static constexpr std::string_view kModes[4] = {"survival", "creative", "adventure",
                                                       "spectator"};
        static constexpr std::string_view kShown[4] = {"Survival", "Creative", "Adventure",
                                                       "Spectator"};
        int mode = -1;
        for (int i = 0; i < 4; ++i)
            if (a.size() == 2 && a[1] == kModes[i]) mode = i;
        if (!ctx.survival || mode < 0 || (mode >= 2 && !ctx.gameMode))
            return fail(ctx.gameMode ? "Usage: /gamemode survival|creative|adventure|spectator"
                                     : "Usage: /gamemode survival|creative");
        *ctx.survival = mode == 0 || mode == 2;
        if (ctx.gameMode) *ctx.gameMode = mode;
        if (ctx.vitals) ctx.vitals->resetFall();
        return {true, "Set own game mode to " + std::string(kShown[mode]) + " Mode"};
    }
    if (a[0] == "gamerule") {
        // /gamerule <rule> [value] (wiki: Commands/gamerule; 1.21.11 ids such as
        // minecraft:keep_inventory, the older camelCase names still accepted).
        if (!ctx.rules || a.size() < 2 || a.size() > 3)
            return fail("Usage: /gamerule <rule> [value]");
        const auto now = ctx.rules->get(a[1]);
        if (!now) return fail("Unknown game rule: " + std::string(a[1]));
        if (a.size() == 2)
            return {true, "Gamerule " + std::string(a[1]) + " is currently set to: " + *now};
        if (!ctx.rules->set(a[1], a[2]))
            return fail("Invalid value for " + std::string(a[1]) + ": " + std::string(a[2]));
        return {true, "Gamerule " + std::string(a[1]) + " is now set to: " + *ctx.rules->get(a[1])};
    }
    if (a[0] == "difficulty") {
        // /difficulty [peaceful|easy|normal|hard] (wiki: Commands/difficulty).
        static constexpr std::string_view kNames[4] = {"peaceful", "easy", "normal", "hard"};
        static constexpr std::string_view kShown[4] = {"Peaceful", "Easy", "Normal", "Hard"};
        if (!ctx.difficulty || a.size() > 2)
            return fail("Usage: /difficulty [peaceful|easy|normal|hard]");
        if (a.size() == 1)
            return {true, "The difficulty is " + std::string(kShown[*ctx.difficulty & 3])};
        for (int i = 0; i < 4; ++i)
            if (a[1] == kNames[i]) {
                if (*ctx.difficulty == i)
                    return fail("The difficulty did not change; it is already set to " +
                                std::string(kShown[i]));
                *ctx.difficulty = i;
                return {true, "The difficulty has been set to " + std::string(kShown[i])};
            }
        return fail("Usage: /difficulty [peaceful|easy|normal|hard]");
    }
    if (a[0] == "setblock") {
        // /setblock <x> <y> <z> <block> (wiki: Commands/setblock): neighbours update.
        if (!ctx.world || a.size() != 5) return fail("Usage: /setblock <x> <y> <z> <block>");
        const glm::dvec3 p = ctx.player.position();
        const auto x = coordinate(a[1], p.x, false), y = coordinate(a[2], p.y, false),
                   z = coordinate(a[3], p.z, false);
        if (!x || !y || !z) return fail("Invalid position");
        const auto state = world::blockRegistry().parse(a[4]);
        if (!state) return fail(format("Unknown block '%.*s'", int(a[4].size()), a[4].data()));
        const world::BlockPos at{int(std::floor(*x)), int(std::floor(*y)), int(std::floor(*z))};
        if (!ctx.world->isInHeight(at.y) || !ctx.world->chunk(at.chunk()))
            return fail("That position is not loaded");
        if (ctx.world->getBlock(at) == *state) return fail("Could not set the block");
        ctx.world->updateBlock(at, *state);
        if (ctx.changed) ctx.changed->push_back(at);
        return {true, format("Changed the block at %d, %d, %d", at.x, at.y, at.z)};
    }
    if (a[0] == "fill") {
        // /fill <from> <to> <block> (wiki: Commands/fill): every block is placed first,
        // then the neighbours are updated; at most 32768 blocks.
        if (!ctx.world || a.size() != 8)
            return fail("Usage: /fill <x1> <y1> <z1> <x2> <y2> <z2> <block>");
        const glm::dvec3 p = ctx.player.position();
        std::optional<double> c[6];
        for (int i = 0; i < 6; ++i)
            c[i] = coordinate(a[size_t(1 + i)], i % 3 == 0 ? p.x : i % 3 == 1 ? p.y : p.z, false);
        for (const auto& v : c)
            if (!v) return fail("Invalid position");
        const auto state = world::blockRegistry().parse(a[7]);
        if (!state) return fail(format("Unknown block '%.*s'", int(a[7].size()), a[7].data()));
        int lo[3], hi[3];
        for (int i = 0; i < 3; ++i) {
            const int u = int(std::floor(*c[i])), v = int(std::floor(*c[i + 3]));
            lo[i] = std::min(u, v);
            hi[i] = std::max(u, v);
        }
        const int64_t volume =
            int64_t(hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1);
        if (volume > 32768)
            return fail(
                format("Too many blocks in the specified area (maximum 32768, specified %lld)",
                       (long long)volume));
        for (int y = lo[1]; y <= hi[1]; ++y)
            for (int z = lo[2]; z <= hi[2]; ++z)
                for (int x = lo[0]; x <= hi[0]; ++x)
                    if (!ctx.world->isInHeight(y) ||
                        !ctx.world->chunk(world::BlockPos{x, y, z}.chunk()))
                        return fail("That position is not loaded");
        std::vector<std::pair<world::BlockPos, world::BlockStateId>> old;
        for (int y = lo[1]; y <= hi[1]; ++y)
            for (int z = lo[2]; z <= hi[2]; ++z)
                for (int x = lo[0]; x <= hi[0]; ++x) {
                    const world::BlockPos at{x, y, z};
                    const world::BlockStateId was = ctx.world->getBlock(at);
                    if (was == *state) continue;
                    old.emplace_back(at, was);
                    ctx.world->setBlock(at, *state);
                    if (ctx.changed) ctx.changed->push_back(at);
                }
        if (old.empty()) return fail("No blocks were filled");
        for (const auto& [at, was] : old)
            ctx.world->notifyChanged(at, was, *state);
        return {true, format("Successfully filled %d block(s)", int(old.size()))};
    }
    if (a[0] == "data") {
        // /data merge block x y z {front_text:{messages:["a","b","c","d"]}} - only a
        // sign's front lines (wiki: Commands/data; M23.3c).
        if (!ctx.world || a.size() < 6 || a[1] != "merge" || a[2] != "block")
            return fail("Usage: /data merge block <x> <y> <z> {front_text:{messages:[...]}}");
        const glm::dvec3 here = ctx.player.position();
        const auto x = coordinate(a[3], here.x, true), y = coordinate(a[4], here.y, false),
                   z = coordinate(a[5], here.z, true);
        if (!x || !y || !z) return fail("Invalid position");
        const world::BlockPos p{int(std::floor(*x)), int(std::floor(*y)), int(std::floor(*z))};
        world::Chunk* c = ctx.world->chunk(p.chunk());
        world::SignData* sign =
            c ? c->sign(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z)) : nullptr;
        if (!sign) return fail("The target block is not a block entity");
        // The rest of the line (the tag may contain spaces inside quotes).
        const size_t brace = line.find('{');
        if (brace == std::string_view::npos) return fail("Invalid data tag");
        const std::string_view tag = line.substr(brace);
        const size_t msgs = tag.find("messages:[");
        if (msgs == std::string_view::npos) return fail("Invalid data tag");
        size_t i = msgs + 10;
        for (int n = 0; n < world::SignData::kLines && i < tag.size(); ++n) {
            const size_t open = tag.find('"', i);
            if (open == std::string_view::npos) break;
            const size_t close = tag.find('"', open + 1);
            if (close == std::string_view::npos) break;
            auto& out = sign->front.lines[size_t(n)];
            size_t k = 0;
            for (const char ch : tag.substr(open + 1, close - open - 1))
                if (k < size_t(world::SignData::kChars) && ch >= 32 && ch < 127) out[k++] = ch;
            out[k] = 0;
            i = close + 1;
        }
        c->markDirty();
        return {true, format("Modified block data of %d, %d, %d", p.x, p.y, p.z)};
    }
    if (a[0] == "weather") {
        // /weather (clear|rain|thunder) [duration] (wiki: Commands/weather): the duration
        // in ticks or with a unit (10s, 2d); without one, a random length as the natural
        // cycle would pick (clear 12,000-180,000, rain 12,000-24,000, thunder 3,600-15,600).
        if (!ctx.weather || a.size() < 2 || a.size() > 3)
            return fail("Usage: /weather (clear|rain|thunder) [duration]");
        world::Weather::Kind kind;
        if (a[1] == "clear")
            kind = world::Weather::Kind::Clear;
        else if (a[1] == "rain")
            kind = world::Weather::Kind::Rain;
        else if (a[1] == "thunder")
            kind = world::Weather::Kind::Thunder;
        else
            return fail("Usage: /weather (clear|rain|thunder) [duration]");
        int duration = 0;
        if (a.size() == 3) {
            std::string_view d = a[2];
            int unit = 1;
            if (!d.empty() && (d.back() == 't' || d.back() == 's' || d.back() == 'd')) {
                unit = d.back() == 's' ? 20 : d.back() == 'd' ? 24000 : 1;
                d.remove_suffix(1);
            }
            const auto r = std::from_chars(d.data(), d.data() + d.size(), duration);
            if (r.ec != std::errc() || duration <= 0 || duration > 1000000)
                return fail("Invalid duration");
            duration *= unit;
        } else if (ctx.rng) {
            duration = kind == world::Weather::Kind::Clear  ? 12000 + int(ctx.rng->nextInt(168001))
                       : kind == world::Weather::Kind::Rain ? 12000 + int(ctx.rng->nextInt(12001))
                                                            : 3600 + int(ctx.rng->nextInt(12001));
        } else {
            duration = 6000;
        }
        ctx.weather->set(kind, duration);
        return {true, kind == world::Weather::Kind::Clear  ? "Set the weather to clear"
                      : kind == world::Weather::Kind::Rain ? "Set the weather to rain"
                                                           : "Set the weather to rain & thunder"};
    }
    if (a[0] == "summon" && a.size() >= 2 &&
        (a[1] == "lightning_bolt" || a[1] == "minecraft:lightning_bolt")) {
        if (!ctx.lightning || (a.size() != 2 && a.size() != 5))
            return fail("Usage: /summon lightning_bolt [x y z]");
        glm::dvec3 p = ctx.player.position();
        if (a.size() == 5) {
            const auto x = coordinate(a[2], p.x, true), y = coordinate(a[3], p.y, false),
                       z = coordinate(a[4], p.z, true);
            if (!x || !y || !z) return fail("Invalid position");
            p = {*x, *y, *z};
        }
        ctx.lightning->push_back(
            {int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))});
        return {true, "Summoned new Lightning Bolt"};
    }
    if (a[0] == "summon") {
        // /summon <entity> [x y z] [{Tag:value,...}] (wiki: Commands/summon). Tags (no
        // spaces): Color, Sheared, Age, Size (magma cubes), Health - a small subset of the
        // entity's data.
        if (!ctx.world || !ctx.rng || (a.size() != 2 && a.size() != 5 && a.size() != 6))
            return fail("Usage: /summon <entity> [x y z] [{tags}]");
        std::string_view id = a[1];
        if (id.starts_with("minecraft:")) id.remove_prefix(10);
        std::optional<world::MobType> type;
        for (int k = 0; k < static_cast<int>(world::MobType::Count); ++k)
            if (world::mobInfo(static_cast<world::MobType>(k)).id.substr(10) == id)
                type = static_cast<world::MobType>(k);
        int cartKind = 0; // (M29.3e: chest_minecart... are minecarts here)
        for (int k = 1; k < 6 && !type; ++k)
            if (id == world::kCartKinds[k]) {
                type = world::MobType::Minecart;
                cartKind = k;
            }
        if (!type) return fail(format("Unknown entity '%.*s'", int(id.size()), id.data()));
        glm::dvec3 p = ctx.player.position();
        if (a.size() >= 5) {
            const auto x = coordinate(a[2], p.x, true), y = coordinate(a[3], p.y, false),
                       z = coordinate(a[4], p.z, true);
            if (!x || !y || !z) return fail("Invalid position");
            p = {*x, *y, *z};
        }
        if (!world::isValidMobPosition(p)) return fail("Invalid position for summon");
        world::MobData mob = Mobs::make(*type, p, *ctx.rng);
        if (cartKind != 0) {
            mob.decor = uint8_t(cartKind);
            mob.strength = uint8_t(world::cartSlotsOf(cartKind));
            mob.hasChest = mob.strength > 0;
        }
        if (a.size() == 6) {
            std::string_view tags = a[5];
            if (tags.size() < 2 || tags.front() != '{' || tags.back() != '}')
                return fail("Invalid data tag");
            tags = tags.substr(1, tags.size() - 2);
            while (!tags.empty()) {
                const size_t comma = tags.find(',');
                const std::string_view pair = tags.substr(0, comma);
                tags =
                    comma == std::string_view::npos ? std::string_view{} : tags.substr(comma + 1);
                const size_t colon = pair.find(':');
                if (colon == std::string_view::npos) return fail("Invalid data tag");
                const std::string_view key = pair.substr(0, colon);
                std::string value(pair.substr(colon + 1));
                if (!value.empty() &&
                    (value.back() == 'b' || value.back() == 's' || value.back() == 'f'))
                    value.pop_back(); // NBT type suffixes
                char* end = nullptr;
                const double v = std::strtod(value.c_str(), &end);
                if (value.empty() || *end != '\0' || !std::isfinite(v))
                    return fail("Invalid data tag");
                if (key == "Color")
                    mob.woolColour = static_cast<uint8_t>(std::clamp(int(v), 0, 15));
                else if (key == "Sheared")
                    mob.sheared = v != 0.0;
                else if (key == "Age" && !world::mobInfo(*type).hostile)
                    mob.age = std::clamp(int(v), -24000, 6000);
                else if (key == "Size" &&
                         (*type == world::MobType::MagmaCube ||
                          *type == world::MobType::Slime)) { // vanilla: 0 small, 1 medium, 3 big
                    mob.size = v >= 3.0 ? 4 : v >= 1.0 ? 2 : 1;
                    mob.health = float(mob.size * mob.size);
                } else if (key == "Health")
                    mob.health = std::clamp(float(v), 0.1f, world::mobInfo(*type).maxHealth);
                // Mounts (M26.2): vanilla's Tame, Variant (horses: colour | markings << 8),
                // Strength, ChestedHorse; and our shorthands Saddle:1b, Armor:1-4 (leather..
                // diamond), Decor:1-16 (a carpet's dye + 1) for their equipment.
                else if (key == "Tame" && world::isMount(*type))
                    mob.tamed = v != 0.0;
                else if (key == "SkeletonTrap" && *type == world::MobType::SkeletonHorse) // (M29.1b)
                    mob.skeletonTrap = v != 0.0;
                else if (key == "Glowing" && v != 0.0) // (M29.2c: ours, a very long Glowing effect)
                    Mobs::addEffect(mob, world::Effect::Glowing, 0, 32767);
                else if (key == "FarmVariant" && (*type == world::MobType::Cow || *type == world::MobType::Pig ||
                                                  *type == world::MobType::Chicken)) { // (M29.1d, ours: 0-2)
                    mob.woolColour = uint8_t(std::clamp(int(v), 0, 2));
                    mob.color2 = 1;
                }
                else if (key == "Variant" && *type == world::MobType::Horse) {
                    mob.woolColour = uint8_t(std::clamp(int(v) & 255, 0, 6));
                    mob.color2 = uint8_t(std::clamp(int(v) >> 8, 0, 4));
                } else if (key == "Variant" && world::isLlama(*type))
                    mob.woolColour = uint8_t(std::clamp(int(v), 0, 3));
                else if (key == "Strength" && world::isLlama(*type))
                    mob.strength = uint8_t(std::clamp(int(v), 1, 5));
                else if (key == "ChestedHorse" && world::canCarryChest(*type))
                    mob.hasChest = v != 0.0;
                else if (key == "Saddle" && (world::isMount(*type) || world::isStickRidden(*type)))
                    mob.saddled = v != 0.0;
                else if (key == "Armor" && *type == world::MobType::Horse)
                    mob.horseArmor = uint8_t(std::clamp(int(v), 0, 4));
                else if (key == "Decor" && (world::isLlama(*type) ||
                                            *type == world::MobType::HappyGhast)) // (harness too)
                    mob.decor = uint8_t(std::clamp(int(v), 0, 16));
                // Wildlife (M26.3): RabbitType, IsScreamingGoat; our shorthands FoxType (0 red,
                // 1 snow) and MainGene / HiddenGene (0-6: kPandaGenes order).
                else if (key == "RabbitType" && *type == world::MobType::Rabbit)
                    mob.woolColour = uint8_t(std::clamp(int(v), 0, 5));
                else if (key == "FoxType" && *type == world::MobType::Fox)
                    mob.woolColour = uint8_t(std::clamp(int(v), 0, 1));
                else if (key == "MainGene" && *type == world::MobType::Panda)
                    mob.woolColour = uint8_t(std::clamp(int(v), 0, 6));
                else if (key == "HiddenGene" && *type == world::MobType::Panda)
                    mob.color2 = uint8_t(std::clamp(int(v), 0, 6));
                else if (key == "IsScreamingGoat" && *type == world::MobType::Goat)
                    mob.powered = v != 0.0;
                else
                    return fail(format("Unknown data tag '%.*s'", int(key.size()), key.data()));
            }
        }
        if (!Mobs::add(*ctx.world, mob)) return fail("That position is not loaded");
        return {true, format("Summoned new %.*s", int(id.size()), id.data())};
    }
    if (a[0] == "xp" || a[0] == "experience") {
        // /xp add @s <amount> [levels|points] (wiki: Commands/experience).
        if (!ctx.vitals || a.size() < 4 || a[1] != "add" || !isSelf(a[2]))
            return fail("Usage: /xp add @s <amount> [levels|points]");
        int amount = 0;
        const auto r = std::from_chars(a[3].data(), a[3].data() + a[3].size(), amount);
        if (r.ec != std::errc() || amount < 0 || amount > 100000) return fail("Invalid amount");
        const bool levels = a.size() > 4 && a[4] == "levels";
        if (levels) {
            int64_t points = 0;
            const int to =
                std::min(ctx.vitals->xpLevel() + amount, 21863); // vanilla's cap (saves clamp too)
            for (int l = ctx.vitals->xpLevel(); l < to; ++l)
                points += Vitals::pointsForLevel(l);
            ctx.vitals->addExperience(int(std::min<int64_t>(points, 1 << 30)));
        } else {
            ctx.vitals->addExperience(amount);
        }
        return {true,
                format("Gave %d experience %s to Player", amount, levels ? "levels" : "points")};
    }
    if (a[0] == "effect") {
        // /effect give @s <effect> [seconds] [amplifier] | /effect clear @s [<effect>]
        // (wiki: Commands/effect; 30 s by default).
        if (!ctx.vitals || a.size() < 3 || !isSelf(a[2]) || (a[1] != "give" && a[1] != "clear"))
            return fail("Usage: /effect give @s <effect> [seconds] [amplifier] | /effect clear @s "
                        "[<effect>]");
        std::optional<world::Effect> kind;
        if (a.size() > 3) {
            const std::string id = a[3].find(':') == std::string_view::npos
                                       ? "minecraft:" + std::string(a[3])
                                       : std::string(a[3]);
            kind = world::findEffect(id);
            if (!kind) return fail(format("Unknown effect: %s", id.c_str()));
        }
        if (a[1] == "clear") {
            if (kind)
                ctx.vitals->removeEffect(*kind);
            else
                ctx.vitals->clearEffects();
            return {true, "Removed effects from Player"};
        }
        if (!kind) return fail("Usage: /effect give @s <effect> [seconds] [amplifier]");
        int seconds = 30, amplifier = 0;
        if (a.size() > 4 &&
            (std::from_chars(a[4].data(), a[4].data() + a[4].size(), seconds).ec != std::errc() ||
             seconds < 1 || seconds > 1000000))
            return fail("Invalid duration");
        if (a.size() > 5 &&
            (std::from_chars(a[5].data(), a[5].data() + a[5].size(), amplifier).ec != std::errc() ||
             amplifier < 0 || amplifier > 255))
            return fail("Invalid amplifier");
        ctx.vitals->addEffect(*kind, amplifier, seconds * 20);
        return {true, "Applied effect to Player"};
    }
    if (a[0] == "kill") {
        // /kill [@s] (wiki: Commands/kill): works in creative too.
        if (!ctx.vitals || (a.size() > 1 && !isSelf(a[1]))) return fail("Usage: /kill [@s]");
        ctx.vitals->kill();
        return {true, "Killed Player"};
    }
    if (a[0] == "seed") return {true, format("Seed: [%lld]", static_cast<long long>(ctx.seed))};
    if (a[0] == "help")
        return {true, "/data /difficulty /effect /enchant /fill /gamemode /gamerule /give /help /item /kill "
                      "/seed /setblock /summon /teleport /time /tp /weather /xp"};
    return fail(format("Unknown command: %.*s", int(a[0].size()), a[0].data()));
}

} // namespace mc
