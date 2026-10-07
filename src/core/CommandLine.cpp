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
            if (std::string_view(*v) != "overworld" && std::string_view(*v) != "terrain") {
                error = "--generator needs overworld or terrain";
                return std::nullopt;
            }
            opts.generator = *v;
        } else if (arg == "--no-save") {
            opts.noSave = true;
        } else if (arg == "--inventory") {
            opts.inventory = true;
        } else if (arg == "--f3") {
            opts.debugScreen = true;
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
        } else if (arg == "--flat") {
            opts.flat = true;
        } else if (arg == "--no-vsync") {
            opts.vsync = false;
        } else if (arg == "--hidden") {
            opts.hidden = true;
        } else {
            error = "unknown option: " + std::string(arg);
            return std::nullopt;
        }
    }
    return opts;
}

} // namespace mc
