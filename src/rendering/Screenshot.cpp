#include "rendering/Screenshot.h"

#include "core/Log.h"

#include <glad/gl.h>

#include <filesystem>
#include <vector>

#include <stb_image_write.h> // implementation: core/Compression.cpp

namespace mc::gfx {

bool saveScreenshot(const char* path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::error_code ec;
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);

    // OpenGL's origin is bottom-left; PNG rows go top-down.
    stbi_flip_vertically_on_write(1);
    if (!stbi_write_png(path, width, height, 3, pixels.data(), width * 3)) {
        MC_LOG_ERROR("Failed to write screenshot %s", path);
        return false;
    }
    MC_LOG_INFO("Screenshot written: %s (%dx%d)", path, width, height);
    return true;
}

} // namespace mc::gfx
