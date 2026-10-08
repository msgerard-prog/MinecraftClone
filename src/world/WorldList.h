#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mc::world {

// The world selection list (M22.5): every saves/<folder> with a readable level.dat,
// most recently played first.
struct WorldSummary {
    std::string folder; // saves/<folder>
    std::string name;   // LevelName
    int64_t lastPlayed = 0; // ms since 1970 (LastPlayed)
    bool survival = false;
    bool flat = false;
};
std::vector<WorldSummary> listWorlds(const std::filesystem::path& savesDir);

// A folder name for a new world (vanilla: characters Windows forbids become '_', a
// taken name gets " (1)", " (2)"...; empty names become "New World").
std::string folderForWorld(std::string_view name, const std::filesystem::path& savesDir);

// The seed typed in the create-world screen (vanilla): a number is used as is, other
// text by Java's String.hashCode, empty text picks `random`.
uint64_t seedFromText(std::string_view text, uint64_t random);

} // namespace mc::world
