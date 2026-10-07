#include "gameplay/Commands.h"

#include "world/Blocks.h"
#include "world/DayTime.h"
#include "gameplay/Mobs.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <optional>
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
    if (i < a.size() && isSelf(a[i])) ++i;
    else if (i < a.size() && a[i].front() == '@')
        return fail("Only @s/@p is supported");
    if (a.size() - i != 3 && a.size() - i != 5) return fail("Usage: /tp [@s] <x> <y> <z> [yaw pitch]");
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
        if (a[2] == "day") t = 1000;
        else if (a[2] == "noon") t = 6000;
        else if (a[2] == "night") t = 13000;
        else if (a[2] == "midnight") t = 18000;
        else t = timeValue(a[2]);
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
        if (a[2] == "daytime") v = ctx.dayTime % world::kTicksPerDay;
        else if (a[2] == "gametime") v = ctx.gameTime;
        else if (a[2] == "day") v = ctx.dayTime / world::kTicksPerDay;
        else return fail("Usage: /time query daytime|gametime|day");
        return {true, format("The time is %lld", v)};
    }
    return fail("Usage: /time set|add|query ...");
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
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    const std::string_view name = id.substr(0, id.find('['));
    const auto item = world::itemRegistry().find(name);
    if (!item || *item == world::kNoItem)
        return fail(format("Unknown item '%.*s'", int(id.size()), id.data()));
    world::ItemStack stack{*item, 1};
    if (name.size() != id.size()) { // a block state
        const auto state = world::blockRegistry().parse(id);
        if (!state) return fail(format("Unknown item '%.*s'", int(id.size()), id.data()));
        stack = Inventory::blockStack(*state);
    }
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

} // namespace

CommandResult runCommand(std::string_view line, CommandContext& ctx) {
    if (!line.empty() && line.front() == '/') line.remove_prefix(1);
    const auto a = split(line);
    if (a.empty()) return fail("");
    if (a[0] == "tp" || a[0] == "teleport") return teleport(a, ctx);
    if (a[0] == "time") return time(a, ctx);
    if (a[0] == "give") return give(a, ctx);
    if (a[0] == "gamemode") {
        // /gamemode survival|creative (wiki: Commands/gamemode; ids 0/1 were removed).
        if (a.size() != 2 || !ctx.survival || (a[1] != "survival" && a[1] != "creative"))
            return fail("Usage: /gamemode survival|creative");
        *ctx.survival = a[1] == "survival";
        if (ctx.vitals) ctx.vitals->resetFall();
        return {true, *ctx.survival ? "Set own game mode to Survival Mode"
                                    : "Set own game mode to Creative Mode"};
    }
    if (a[0] == "setblock") {
        // /setblock <x> <y> <z> <block> (wiki: Commands/setblock): neighbours update.
        if (!ctx.world || a.size() != 5) return fail("Usage: /setblock <x> <y> <z> <block>");
        const glm::dvec3 p = ctx.player.position();
        const auto x = coordinate(a[1], p.x, false), y = coordinate(a[2], p.y, false), z = coordinate(a[3], p.z, false);
        if (!x || !y || !z) return fail("Invalid position");
        const auto state = world::blockRegistry().parse(a[4]);
        if (!state) return fail(format("Unknown block '%.*s'", int(a[4].size()), a[4].data()));
        const world::BlockPos at{int(std::floor(*x)), int(std::floor(*y)), int(std::floor(*z))};
        if (!ctx.world->isInHeight(at.y) || !ctx.world->chunk(at.chunk())) return fail("That position is not loaded");
        if (ctx.world->getBlock(at) == *state) return fail("Could not set the block");
        ctx.world->updateBlock(at, *state);
        if (ctx.changed) ctx.changed->push_back(at);
        return {true, format("Changed the block at %d, %d, %d", at.x, at.y, at.z)};
    }
    if (a[0] == "fill") {
        // /fill <from> <to> <block> (wiki: Commands/fill): every block is placed first,
        // then the neighbours are updated; at most 32768 blocks.
        if (!ctx.world || a.size() != 8) return fail("Usage: /fill <x1> <y1> <z1> <x2> <y2> <z2> <block>");
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
        const int64_t volume = int64_t(hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1);
        if (volume > 32768) return fail(format("Too many blocks in the specified area (maximum 32768, specified %lld)", (long long)volume));
        for (int y = lo[1]; y <= hi[1]; ++y)
            for (int z = lo[2]; z <= hi[2]; ++z)
                for (int x = lo[0]; x <= hi[0]; ++x)
                    if (!ctx.world->isInHeight(y) || !ctx.world->chunk(world::BlockPos{x, y, z}.chunk()))
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
    if (a[0] == "summon") {
        // /summon <zombie|cow> [x y z] (wiki: Commands/summon).
        if (!ctx.world || !ctx.rng || (a.size() != 2 && a.size() != 5)) return fail("Usage: /summon <entity> [x y z]");
        std::string_view id = a[1];
        if (id.starts_with("minecraft:")) id.remove_prefix(10);
        std::optional<world::MobType> type;
        for (int k = 0; k < static_cast<int>(world::MobType::Count); ++k)
            if (world::mobInfo(static_cast<world::MobType>(k)).id.substr(10) == id) type = static_cast<world::MobType>(k);
        if (!type) return fail(format("Unknown entity '%.*s'", int(id.size()), id.data()));
        glm::dvec3 p = ctx.player.position();
        if (a.size() == 5) {
            const auto x = coordinate(a[2], p.x, true), y = coordinate(a[3], p.y, false), z = coordinate(a[4], p.z, true);
            if (!x || !y || !z) return fail("Invalid position");
            p = {*x, *y, *z};
        }
        if (!world::isValidMobPosition(p)) return fail("Invalid position for summon");
        if (!Mobs::add(*ctx.world, Mobs::make(*type, p, *ctx.rng))) return fail("That position is not loaded");
        return {true, format("Summoned new %.*s", int(id.size()), id.data())};
    }
    if (a[0] == "kill") {
        // /kill [@s] (wiki: Commands/kill): works in creative too.
        if (!ctx.vitals || (a.size() > 1 && !isSelf(a[1]))) return fail("Usage: /kill [@s]");
        ctx.vitals->kill();
        return {true, "Killed Player"};
    }
    if (a[0] == "seed") return {true, format("Seed: [%lld]", static_cast<long long>(ctx.seed))};
    if (a[0] == "help") return {true, "/fill /gamemode /give /help /kill /seed /setblock /summon /teleport /time /tp"};
    return fail(format("Unknown command: %.*s", int(a[0].size()), a[0].data()));
}

} // namespace mc
