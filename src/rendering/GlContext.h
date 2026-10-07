#pragma once

namespace mc::gfx {

// Loads OpenGL 4.6 function pointers (glad) for the current context and
// installs the KHR_debug message callback. Call once after Window::create.
bool initOpenGl();

} // namespace mc::gfx
