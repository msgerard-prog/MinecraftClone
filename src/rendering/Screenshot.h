#pragma once

namespace mc::gfx {

// Reads the current back buffer (call before swapBuffers) and writes it to
// `path` as an RGB PNG, top row first. Creates parent folders if needed.
bool saveScreenshot(const char* path, int width, int height);

} // namespace mc::gfx
