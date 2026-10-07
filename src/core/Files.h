#pragma once

#include <optional>
#include <string>

namespace mc {

// Absolute path of an asset, e.g. assetPath("shaders/sky.vert").
std::string assetPath(const char* relative);

// Reads a whole file into a string. Load-time only (allocates).
std::optional<std::string> readTextFile(const std::string& path);

} // namespace mc
