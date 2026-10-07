#include "gameplay/Commands.h"

#include "world/Blocks.h"
#include "world/DayTime.h"

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
    return static_cast<int64_t>(std::lround(*v * scale));
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
    float yaw = ctx.player.yaw(), pitch = ctx.player.pitch();
    if (a.size() - i == 5) {
        const auto yw = coordinate(a[i + 3], yaw, false);
        const auto pt = coordinate(a[i + 4], pitch, false);
        if (!yw || !pt) return fail("Invalid rotation");
        yaw = static_cast<float>(*yw);
        pitch = static_cast<float>(*pt);
    }
    ctx.player.setPosition({*x, *y, *z});
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
        // `set` keeps the day count (vanilla sets the time of the current day).
        ctx.dayTime = ctx.dayTime - ctx.dayTime % world::kTicksPerDay + *t;
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
    // /give @s <block> [count]: blocks only until items exist (M9); the block goes
    // into the selected hotbar slot (no inventory yet).
    if (a.size() < 3 || !isSelf(a[1])) return fail("Usage: /give @s <block> [count]");
    std::string_view id = a[2];
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    const auto state = world::blockRegistry().parse(id);
    if (!state || *state == 0) return fail(format("Unknown item '%.*s'", int(id.size()), id.data()));
    ctx.hotbar.setSlot(ctx.hotbar.selected(), *state);
    return {true, format("Gave 1 [%.*s] to Player", int(id.size()), id.data())};
}

} // namespace

CommandResult runCommand(std::string_view line, CommandContext& ctx) {
    if (!line.empty() && line.front() == '/') line.remove_prefix(1);
    const auto a = split(line);
    if (a.empty()) return fail("");
    if (a[0] == "tp" || a[0] == "teleport") return teleport(a, ctx);
    if (a[0] == "time") return time(a, ctx);
    if (a[0] == "give") return give(a, ctx);
    if (a[0] == "seed") return {true, format("Seed: [%llu]", static_cast<unsigned long long>(ctx.seed))};
    if (a[0] == "help") return {true, "/give /help /seed /teleport /time /tp"};
    return fail(format("Unknown command: %.*s", int(a[0].size()), a[0].data()));
}

} // namespace mc
