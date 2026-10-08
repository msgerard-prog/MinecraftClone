#include "world/WorldList.h"

#include "world/LevelData.h"

#include <algorithm>
#include <charconv>

namespace mc::world {

std::vector<WorldSummary> listWorlds(const std::filesystem::path& savesDir) {
    std::vector<WorldSummary> out;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(savesDir, ec)) {
        if (!entry.is_directory(ec)) continue;
        const auto level = LevelData::load(entry.path());
        if (!level) continue;
        out.push_back({entry.path().filename().string(), level->name, level->lastPlayed, level->survival, level->flat});
    }
    std::sort(out.begin(), out.end(), [](const WorldSummary& a, const WorldSummary& b) {
        return a.lastPlayed != b.lastPlayed ? a.lastPlayed > b.lastPlayed : a.folder < b.folder;
    });
    return out;
}

std::string folderForWorld(std::string_view name, const std::filesystem::path& savesDir) {
    std::string base;
    for (const char c : name)
        base += (c < 32 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos) ? '_' : c;
    while (!base.empty() && (base.back() == ' ' || base.back() == '.')) base.pop_back(); // (Windows trims these)
    if (base.empty()) base = "New World";
    std::string folder = base;
    std::error_code ec;
    for (int n = 1; std::filesystem::exists(savesDir / folder, ec); ++n)
        folder = base + " (" + std::to_string(n) + ")";
    return folder;
}

uint64_t seedFromText(std::string_view text, uint64_t random) {
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    if (text.empty()) return random;
    int64_t v = 0;
    const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
    if (r.ec == std::errc() && r.ptr == text.data() + text.size()) return uint64_t(v);
    int32_t h = 0; // Java String.hashCode over the (ASCII) characters
    for (const char c : text)
        h = int32_t(uint32_t(h) * 31u + uint32_t(uint8_t(c)));
    return uint64_t(int64_t(h));
}

} // namespace mc::world
