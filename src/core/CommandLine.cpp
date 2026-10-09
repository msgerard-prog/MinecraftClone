#include "core/CommandLine.h"

#include <charconv>
#include <cmath>
#include <string_view>

namespace mc {

namespace {

template <typename T> bool parseNumber(std::string_view text, T& out) {
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
    return ec == std::errc{} && ptr == text.data() + text.size();
}

bool parseSize(std::string_view text, int& w, int& h) {
    const size_t x = text.find('x');
    return x != std::string_view::npos && parseNumber(text.substr(0, x), w) &&
           parseNumber(text.substr(x + 1), h) && w > 0 && h > 0;
}

// Parses "a,b,c" into exactly `count` numbers.
template <typename T> bool parseList(std::string_view text, T* out, int count) {
    for (int i = 0; i < count; ++i) {
        const size_t comma = text.find(',');
        const bool last = i == count - 1;
        if (last != (comma == std::string_view::npos)) return false;
        if (!parseNumber(text.substr(0, comma), out[i])) return false;
        if (!std::isfinite(out[i])) return false; // from_chars accepts "nan" and "inf"
        if (!last) text.remove_prefix(comma + 1);
    }
    return true;
}

} // namespace

std::optional<LaunchOptions> parseCommandLine(std::span<const char* const> args,
                                              std::string& error) {
    LaunchOptions opts;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const bool hasValue = i + 1 < args.size();
        auto needValue = [&]() -> std::optional<std::string_view> {
            if (!hasValue) {
                error = std::string(arg) + " needs a value";
                return std::nullopt;
            }
            return std::string_view(args[++i]);
        };

        if (arg == "--screenshot") {
            auto v = needValue();
            if (!v) return std::nullopt;
            opts.screenshotPath = std::string(*v);
        } else if (arg == "--frames") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseNumber(*v, opts.screenshotFrames) || opts.screenshotFrames < 1) {
                error = "--frames needs a positive integer";
                return std::nullopt;
            }
        } else if (arg == "--time") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseNumber(*v, opts.time) || opts.time < 0) {
                error = "--time needs ticks >= 0, e.g. 6000 (noon) or 18000 (midnight)";
                return std::nullopt;
            }
        } else if (arg == "--world") {
            auto v = needValue();
            if (!v) return std::nullopt;
            const std::string_view name(*v);
            if (name.empty() || name.find_first_of("/\\:*?\"<>|") != std::string_view::npos ||
                name == "." || name == "..") {
                error = "--world needs a folder name (no path separators)";
                return std::nullopt;
            }
            opts.world = std::string(name);
        } else if (arg == "--generator") {
            auto v = needValue();
            if (!v) return std::nullopt;
            const std::string_view g(*v);
            if (g != "overworld8" && g != "overworld7" && g != "overworld6" && g != "overworld5" && g != "overworld4" && g != "overworld3" && g != "overworld2" && g != "overworld" &&
                g != "terrain") {
                error = "--generator needs overworld8, overworld7, overworld6, overworld5, overworld4, overworld3, overworld2, overworld or terrain";
                return std::nullopt;
            }
            opts.generator = *v;
        } else if (arg == "--dimension") {
            auto v = needValue();
            if (!v) return std::nullopt;
            const std::string_view d(*v);
            if (d != "overworld" && d != "nether" && d != "end") {
                error = "--dimension needs overworld, nether or end";
                return std::nullopt;
            }
            opts.dimension = *v;
        } else if (arg == "--version") {
            opts.printVersion = true;
        } else if (arg == "--no-save") {
            opts.noSave = true;
        } else if (arg == "--inventory") {
            opts.inventory = true;
        } else if (arg == "--use") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseNumber(*v, opts.use) || opts.use < 1) {
                error = "--use needs a positive count";
                return std::nullopt;
            }
        } else if (arg == "--book") {
            auto v = needValue();
            if (!v) return std::nullopt;
            opts.book = true;
            opts.bookText = *v;
        } else if (arg == "--trade") {
            opts.trade = true;
        } else if (arg == "--mount") {
            opts.mount = true;
        } else if (arg == "--f3") {
            opts.debugScreen = true;
        } else if (arg == "--recipe-book") {
            opts.recipeBook = true;
        } else if (arg == "--perspective") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseNumber(*v, opts.perspective) || opts.perspective < 0 || opts.perspective > 2) {
                error = "--perspective needs 0, 1 or 2";
                return std::nullopt;
            }
        } else if (arg == "--command") {
            auto v = needValue();
            if (!v) return std::nullopt;
            opts.commands.emplace_back(*v);
        } else if (arg == "--seed") {
            auto v = needValue();
            if (!v) return std::nullopt;
            // Seeds are signed 64-bit in vanilla; unsigned input is accepted too.
            int64_t signedSeed = 0;
            if (parseNumber(*v, signedSeed)) {
                opts.seed = static_cast<uint64_t>(signedSeed);
            } else if (!parseNumber(*v, opts.seed)) {
                error = "--seed needs an unsigned integer";
                return std::nullopt;
            }
        } else if (arg == "--size") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseSize(*v, opts.width, opts.height)) {
                error = "--size needs WIDTHxHEIGHT, e.g. 1280x720";
                return std::nullopt;
            }
        } else if (arg == "--pos") {
            auto v = needValue();
            if (!v) return std::nullopt;
            double xyz[3];
            if (!parseList(*v, xyz, 3)) {
                error = "--pos needs x,y,z, e.g. 0.5,70,0.5";
                return std::nullopt;
            }
            opts.pos = {xyz[0], xyz[1], xyz[2]};
            opts.hasPos = true;
        } else if (arg == "--open-block") {
            auto v = needValue();
            if (!v) return std::nullopt;
            double xyz[3];
            if (!parseList(*v, xyz, 3) || xyz[0] != std::floor(xyz[0]) || xyz[1] != std::floor(xyz[1]) ||
                xyz[2] != std::floor(xyz[2]) || std::abs(xyz[0]) > 3.0e7 || std::abs(xyz[1]) > 4096 ||
                std::abs(xyz[2]) > 3.0e7) {
                error = "--open-block needs whole x,y,z, e.g. 1,-60,1";
                return std::nullopt;
            }
            opts.openBlock[0] = int(xyz[0]), opts.openBlock[1] = int(xyz[1]), opts.openBlock[2] = int(xyz[2]);
            opts.hasOpenBlock = true;
        } else if (arg == "--look") {
            auto v = needValue();
            if (!v) return std::nullopt;
            float yawPitch[2];
            if (!parseList(*v, yawPitch, 2)) {
                error = "--look needs yaw,pitch in degrees, e.g. -45,30";
                return std::nullopt;
            }
            opts.yaw = yawPitch[0];
            opts.pitch = yawPitch[1];
            opts.hasLook = true;
        } else if (arg == "--resourcepacks") {
            auto v = needValue();
            if (!v) return std::nullopt;
            opts.resourcePacks = std::string(*v);
        } else if (arg == "--render-distance") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseNumber(*v, opts.renderDistance) || opts.renderDistance < 2 ||
                opts.renderDistance > 32) {
                error = "--render-distance needs 2..32";
                return std::nullopt;
            }
            opts.renderDistanceSet = true;
        } else if (arg == "--max-fps") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseNumber(*v, opts.maxFps) || opts.maxFps < 1) {
                error = "--max-fps needs a positive integer";
                return std::nullopt;
            }
        } else if (arg == "--demo-edit") {
            opts.demoEdit = true;
        } else if (arg == "--auto-fly") {
            opts.autoFly = true;
        } else if (arg == "--difficulty") {
            auto v = needValue();
            if (!v) return std::nullopt;
            const std::string_view d(*v);
            opts.difficulty = d == "peaceful" ? 0 : d == "easy" ? 1 : d == "normal" ? 2 : d == "hard" ? 3 : -1;
            if (opts.difficulty < 0) {
                error = "--difficulty needs peaceful, easy, normal or hard";
                return std::nullopt;
            }
        } else if (arg == "--flat") {
            opts.flat = true;
        } else if (arg == "--no-vsync") {
            opts.vsync = false;
        } else if (arg == "--hidden") {
            opts.hidden = true;
        } else if (arg == "--mute") {
            opts.mute = true;
        } else if (arg == "--sound") {
            opts.sound = true;
        } else if (arg == "--menu") {
            auto v = needValue();
            if (!v) return std::nullopt;
            opts.menu = std::string(*v);
            if (opts.menu != "title" && opts.menu != "worlds" && opts.menu != "create" && opts.menu != "options" &&
                opts.menu != "pause" && opts.menu != "statistics" && opts.menu != "advancements" &&
                opts.menu != "commandblock") {
                error = "--menu needs title|worlds|create|options|pause|statistics|advancements|commandblock";
                return std::nullopt;
            }
        } else {
            error = "unknown option: " + std::string(arg);
            return std::nullopt;
        }
    }
    return opts;
}

} // namespace mc
