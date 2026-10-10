#include "core/Options.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <string>

namespace mc {

namespace {

template <typename T>
bool number(std::string_view s, T& out) {
    if (!s.empty() && s.front() == '"') s = s.substr(1, s.size() >= 2 ? s.size() - 2 : 0);
    if constexpr (std::is_floating_point_v<T>) {
        try {
            out = static_cast<T>(std::stod(std::string(s)));
            return true;
        } catch (...) {
            return false;
        }
    } else {
        return std::from_chars(s.data(), s.data() + s.size(), out).ec == std::errc();
    }
}

} // namespace

bool GameOptions::load(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string_view key(line.data(), colon), value(line.data() + colon + 1, line.size() - colon - 1);
        float f = 0;
        int i = 0;
        if (key == "fov" && number(value, f)) fov = std::clamp(70.0f + f * 40.0f, 30.0f, 110.0f);
        else if (key == "renderDistance" && number(value, i)) renderDistance = std::clamp(i, 2, 32);
        else if (key == "simulationDistance" && number(value, i)) simulationDistance = std::clamp(i, 5, 32);
        else if (key == "mouseSensitivity" && number(value, f)) sensitivity = std::clamp(f, 0.0f, 1.0f);
        else if (key == "guiScale" && number(value, i)) guiScale = std::clamp(i, 0, 16);
        else if (key == "soundCategory_master" && number(value, f)) masterVolume = std::clamp(f, 0.0f, 1.0f);
        else if (key == "renderClouds") clouds = value.find("false") == std::string_view::npos;
        else if (key == "enableVsync") vsync = value == "true";
        else if (key == "bobView") bobView = value == "true";
        else if (key == "clone_hotbarNumbers") hotbarNumbers = value == "true";
        else if (key == "fullscreen" && value == "true" && displayMode == DisplayMode::Windowed)
            displayMode = DisplayMode::Fullscreen; // (vanilla's key; ours below says which kind)
        else if (key == "clone_displayMode")
            displayMode = value == "borderless"   ? DisplayMode::Borderless
                          : value == "fullscreen" ? DisplayMode::Fullscreen
                                                  : DisplayMode::Windowed;
        else if (key == "fullscreenResolution") { // vanilla's form: "1920x1080@60:24"
            DisplayResolution r;
            const char* p = value.data();
            const char* end = value.data() + value.size();
            auto num = [&](int& out) {
                const auto res = std::from_chars(p, end, out);
                if (res.ec != std::errc()) return false;
                p = res.ptr;
                return true;
            };
            bool ok = num(r.width) && p < end && *p++ == 'x' && num(r.height);
            if (ok && p < end && *p == '@') {
                ++p;
                ok = num(r.refresh);
            }
            if (ok && r.width >= 320 && r.height >= 240 && r.width <= 16384 && r.height <= 16384) resolution = r;
        }
    }
    return true;
}

bool GameOptions::save(const std::filesystem::path& file) const {
    std::ofstream out(file, std::ios::trunc);
    if (!out) return false;
    out << "fov:" << (fov - 70.0f) / 40.0f << '\n'
        << "renderDistance:" << renderDistance << '\n'
        << "simulationDistance:" << simulationDistance << '\n'
        << "mouseSensitivity:" << sensitivity << '\n'
        << "guiScale:" << guiScale << '\n'
        << "soundCategory_master:" << masterVolume << '\n'
        << "renderClouds:\"" << (clouds ? "true" : "false") << "\"\n"
        << "enableVsync:" << (vsync ? "true" : "false") << '\n'
        << "bobView:" << (bobView ? "true" : "false") << '\n'
        << "clone_hotbarNumbers:" << (hotbarNumbers ? "true" : "false") << '\n'
        << "fullscreen:" << (displayMode != DisplayMode::Windowed ? "true" : "false") << '\n'
        << "clone_displayMode:"
        << (displayMode == DisplayMode::Borderless   ? "borderless"
            : displayMode == DisplayMode::Fullscreen ? "fullscreen"
                                                     : "windowed")
        << '\n';
    if (resolution.width > 0)
        out << "fullscreenResolution:" << resolution.width << 'x' << resolution.height << '@' << resolution.refresh
            << ":24\n";
    return bool(out);
}

} // namespace mc
