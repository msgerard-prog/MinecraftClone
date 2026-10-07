#include "core/CommandLine.h"
#include "core/Files.h"
#include "core/GameClock.h"
#include "core/Log.h"
#include "core/Window.h"
#include "gameplay/FlyController.h"
#include "rendering/Camera.h"
#include "rendering/CubeMesher.h"
#include "rendering/GlContext.h"
#include "rendering/Screenshot.h"
#include "rendering/Shader.h"
#include "rendering/TextureAtlas.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <span>
#include <string>
#include <vector>

namespace {

// Vanilla's daytime sky colour at plains biome (#78A7FF).
constexpr float kSkyR = 0x78 / 255.0f;
constexpr float kSkyG = 0xA7 / 255.0f;
constexpr float kSkyB = 0xFF / 255.0f;

// Vanilla controls: WASD move, space up, shift down (in flight), ctrl sprint.
mc::MoveInput readMoveInput(const mc::Window& window) {
    mc::MoveInput in;
    if (!window.cursorCaptured()) return in;
    in.forward = float(window.keyDown(mc::Key::W)) - float(window.keyDown(mc::Key::S));
    in.strafe = float(window.keyDown(mc::Key::D)) - float(window.keyDown(mc::Key::A));
    in.up = float(window.keyDown(mc::Key::Space)) - float(window.keyDown(mc::Key::LeftShift));
    in.sprint = window.keyDown(mc::Key::LeftControl);
    return in;
}

// M1 test scene: a grass block in front of a row of every placeholder block.
// Replaced by real chunks in M2.
std::vector<mc::gfx::BlockVertex> buildTestScene(const mc::gfx::TextureAtlas& atlas) {
    using namespace mc::gfx;
    std::vector<BlockVertex> verts;
    appendCube(verts, {0, 0, 0}, grassBlock(atlas));
    const CubeFaces row[] = {
        cubeAll(atlas, "stone"),
        cubeAll(atlas, "cobblestone"),
        cubeAll(atlas, "dirt"),
        cubeAll(atlas, "oak_planks"),
        cubeColumn(atlas, "oak_log", "oak_log_top"),
        cubeAll(atlas, "sand"),
        cubeAll(atlas, "bedrock"),
        grassBlock(atlas),
    };
    int x = -7;
    for (const CubeFaces& faces : row) {
        appendCube(verts, {x, 0, 4}, faces);
        x += 2;
    }
    return verts;
}

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

    mc::gfx::Shader blockShader;
    if (!blockShader.load("block")) return 1;
    mc::gfx::TextureAtlas atlas;
    if (!atlas.build(mc::assetPath("minecraft/textures/block"))) return 1;
    mc::gfx::Mesh scene;
    scene.upload(buildTestScene(atlas));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // back faces (clockwise from the camera) are never visible

    mc::FlyController player;
    player.setPosition(opts->hasPos ? opts->pos : glm::dvec3(0.5, 3.0, -5.0));
    player.setRotation(opts->hasLook ? opts->yaw : 0.0f, opts->hasLook ? opts->pitch : 25.0f);

    mc::GameClock clock;
    double last = mc::timeSeconds();
    int frame = 0;
    int exitCode = 0;

    while (!window.shouldClose()) {
        window.pollEvents();
        if (!screenshotMode) {
            // Click to capture the mouse, Esc to release it (pause menu comes in M6).
            if (!window.cursorCaptured() && window.leftMousePressed()) {
                window.setCursorCaptured(true);
            }
            if (window.cursorCaptured() && window.keyDown(mc::Key::Escape)) {
                window.setCursorCaptured(false);
            }
        }
        player.turn(window.mouseDx(), window.mouseDy());

        const double now = mc::timeSeconds();
        clock.advance(now - last);
        last = now;
        for (int i = 0; i < clock.ticksDue; ++i) {
            player.tick(readMoveInput(window));
        }

        int fbWidth = 0;
        int fbHeight = 0;
        window.framebufferSize(fbWidth, fbHeight);
        if (fbWidth == 0 || fbHeight == 0) { // minimised
            window.swapBuffers();
            continue;
        }

        mc::gfx::Camera camera;
        camera.position = player.renderPosition(clock.alpha);
        camera.yaw = player.yaw();
        camera.pitch = player.pitch();
        const glm::mat4 viewProj = camera.viewProjection(float(fbWidth) / float(fbHeight));

        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(kSkyR, kSkyG, kSkyB, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        blockShader.bind();
        glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(viewProj));
        glBindTextureUnit(0, atlas.texture());
        scene.draw();

        ++frame;
        if (screenshotMode && frame >= opts->screenshotFrames) {
            if (!mc::gfx::saveScreenshot(opts->screenshotPath.c_str(), fbWidth, fbHeight)) {
                exitCode = 1;
            }
            break;
        }
        window.swapBuffers();
    }
    return exitCode;
}
