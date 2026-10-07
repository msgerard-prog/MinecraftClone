#include "core/Files.h"

#include <fstream>
#include <sstream>

namespace mc {

std::string assetPath(const char* relative) { return std::string(MC_ASSETS_DIR) + "/" + relative; }

std::optional<std::string> readTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

} // namespace mc
