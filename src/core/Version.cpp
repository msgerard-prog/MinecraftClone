#include "core/Version.h"

#include "BuildInfo.h" // generated each build (cmake/GenBuildInfo.cmake)

namespace mc {

const char* version() { return MC_VERSION; }
const char* buildString() { return MC_BUILD; }

} // namespace mc
