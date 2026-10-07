#include "core/CommandLine.h"
#include "core/GameClock.h"
#include "core/Log.h"
#include "core/Window.h"
#include "rendering/GlContext.h"
#include "rendering/Screenshot.h"
#include "rendering/Shader.h"

#include <glad/gl.h>

#include <span>
#include <string>

namespace {

// Vanilla's daytime sky colour at plains biome (#78A7FF).
constexpr float kSkyR = 0x78 / 255.0f;
constexpr float kSkyG = 0xA7 / 255.0f;
constexpr float kSkyB = 0xFF / 255.0f;

} // namespace

int main(int argc, char** argv) {
    std::string error;
    const auto opts = mc::parseCommandLine(
        std::span<const char* const>(argv + 1, static_cast<size_t>(argc - 1)), error);
    if (!opts) {
        MC_LOG_ERROR("%s", error.c_str());
        return 2;
    }
    const bool screenshotMode = !opts->screenshotPath.empty();

    mc::Window window;
    if (!window.create(opts->width, opts->height, "MinecraftClone", !opts->hidden)) {
        MC_LOG_ERROR("Could not create an OpenGL 4.6 window");
        return 1;
    }
    if (!mc::gfx::initOpenGl()) return 1;

    // M0 placeholder scene; replaced by the world renderer in M1.
    mc::gfx::Shader helloShader;
    if (!helloShader.load("hello")) return 1;
    GLuint emptyVao = 0;
    glCreateVertexArrays(1, &emptyVao);

    mc::GameClock clock;
    double last = mc::timeSeconds();
    int frame = 0;
    int exitCode = 0;

    while (!window.shouldClose()) {
        window.pollEvents();

        const double now = mc::timeSeconds();
        clock.advance(now - last);
        last = now;
        for (int i = 0; i < clock.ticksDue; ++i) {
            // world/gameplay tick goes here (M2+)
        }

        int fbWidth = 0;
        int fbHeight = 0;
        window.framebufferSize(fbWidth, fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(kSkyR, kSkyG, kSkyB, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        helloShader.bind();
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        ++frame;
        if (screenshotMode && frame >= opts->screenshotFrames) {
            if (!mc::gfx::saveScreenshot(opts->screenshotPath.c_str(), fbWidth, fbHeight)) {
                exitCode = 1;
            }
            break;
        }
        window.swapBuffers();
    }
    glDeleteVertexArrays(1, &emptyVao);
    return exitCode;
}
