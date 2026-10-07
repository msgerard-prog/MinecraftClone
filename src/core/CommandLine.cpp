#include "core/CommandLine.h"

#include <charconv>
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
        } else if (arg == "--seed") {
            auto v = needValue();
            if (!v) return std::nullopt;
            if (!parseNumber(*v, opts.seed)) {
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
