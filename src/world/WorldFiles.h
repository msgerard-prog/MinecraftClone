#pragma once

#include "core/Nbt.h"
#include "world/Dimension.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace mc::world {

// The world folder's layout (M34; wiki: Java Edition level format, 26.1+):
//   level.dat                                   the world's own settings
//   players/data/<uuid>.dat, players/stats/, players/advancements/
//   data/minecraft/*.dat                        data shared by all dimensions (game rules,
//                                               weather, world clocks, world gen settings,
//                                               the wandering trader), maps/<id>.dat
//   dimensions/minecraft/<dimension>/           region/, entities/, data/minecraft/*.dat
// Before 26.1 the Overworld's region/ and entities/ sat in the world folder itself, the
// Nether's in DIM-1/, the End's in DIM1/, and players' files in playerdata/, stats/ and
// advancements/; `migrateLegacyLayout` moves an older world into place.
std::filesystem::path dimensionFolder(const std::filesystem::path& world, Dimension d);
std::filesystem::path dimensionDataFolder(const std::filesystem::path& world, Dimension d);
std::filesystem::path dataFolder(const std::filesystem::path& world); // data/minecraft
std::filesystem::path playersFolder(const std::filesystem::path& world, std::string_view kind); // data|stats|advancements
std::filesystem::path mapsFolder(const std::filesystem::path& world);

// Vanilla's saved-data files: gzip NBT, root {data: {...}, DataVersion}. Written to a
// temporary file first and renamed into place. Reading returns the `data` compound.
bool writeSavedData(const std::filesystem::path& file, nbt::Compound data);
std::optional<nbt::Compound> readSavedData(const std::filesystem::path& file);
// Any gzip NBT file (players/data/<uuid>.dat holds the player itself at its root).
bool writeNbtFile(const std::filesystem::path& file, const nbt::Compound& root);
std::optional<nbt::Compound> readNbtFile(const std::filesystem::path& file);

// Moves a pre-26.1 world's folders and files to where 26.1+ keeps them (as vanilla does
// when it opens an older world). Nothing to do for a world already laid out this way.
// Returns false (and logs) if something couldn't be moved; the world is then left as it
// was found as far as possible, and must not be opened.
bool migrateLegacyLayout(const std::filesystem::path& world);

} // namespace mc::world
