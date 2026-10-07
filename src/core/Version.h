#pragma once

namespace mc {

// The program's version ("0.12.0") and build ("v0.12.0-4-g1a2b3c4-dirty", from git
// describe), from the generated BuildInfo.h - only Version.cpp includes it, so a new
// revision recompiles one small file.
const char* version();
const char* buildString();

} // namespace mc
