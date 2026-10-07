#include "core/CommandLine.h"
#include "core/FrameStats.h"
#include "core/GameClock.h"
#include "core/Log.h"
#include "core/Window.h"
#include "gameplay/FlyController.h"
#include "rendering/Camera.h"
#include "rendering/GlContext.h"
#include "rendering/Screenshot.h"
#include "rendering/WorldRenderer.h"
#include "world/Blocks.h"
#include "world/FlatGenerator.h"
#include "world/World.h"

#include <span>
#include <string>
#include <vector>

namespace {

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

// M2 test world: 8x8 chunks of Classic Flat around the origin, plus a display of
// every block on the grass (row at z = 4), logs on all three axes, and a pit that
// exposes dirt and bedrock sides. Replaced by real terrain in M3.
void buildTestWorld(mc::world::World& world) {
    using namespace mc::world;
    const auto gen = FlatGenerator::fromPreset(FlatGenerator::kClassicFlat);
    for (int cz = -4; cz < 4; ++cz) {
        for (int cx = -4; cx < 4; ++cx)
            gen->generate(world.createChunk({cx, cz}));
    }
    const auto& r = blockRegistry();
    const int y = gen->surfaceY(); // -60, first air layer
    const BlockId row[] = {blocks::Stone,     blocks::Cobblestone, blocks::Dirt,
                           blocks::OakPlanks, blocks::OakLog,      blocks::Sand,
                           blocks::Bedrock,   blocks::GrassBlock};
    int x = -7;
    for (BlockId b : row) {
        world.setBlock({x, y, 4}, r.defaultState(b));
        x += 2;
    }
    const BlockStateId log = r.defaultState(blocks::OakLog);
    world.setBlock({-4, y, 0}, *r.with(log, "axis", "x"));
    world.setBlock({-4, y + 1, 0}, log);
    world.setBlock({-4, y + 2, 0}, *r.with(log, "axis", "z"));
    // A 3x3 pit down to the bedrock.
    for (int dz = 0; dz < 3; ++dz)
        for (int dx = 0; dx < 3; ++dx)
            for (int dy = 1; dy <= 3; ++dy)
                world.setBlock({2 + dx, y - dy, -1 + dz}, 0);
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
    if (!window.create(opts->width, opts->height, "MinecraftClone", !opts->hidden, opts->vsync)) {
        MC_LOG_ERROR("Could not create an OpenGL 4.6 window");
        return 1;
    }
    if (!mc::gfx::initOpenGl()) return 1;

    mc::gfx::WorldRenderer renderer;
    if (!renderer.init()) return 1;
    mc::world::World world;
    buildTestWorld(world);
    renderer.markAllDirty(world);

    mc::FlyController player;
    player.setPosition(opts->hasPos ? opts->pos : glm::dvec3(0.5, -57.0, -6.0));
    player.setRotation(opts->hasLook ? opts->yaw : 0.0f, opts->hasLook ? opts->pitch : 25.0f);

    mc::GameClock clock;
    double last = mc::timeSeconds();
    int frame = 0;
    int exitCode = 0;
    mc::FrameStats frameStats;
    const double startTime = last;
    bool meshed = false;

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
        // Full frame period (includes swap, i.e. waiting for the GPU / vsync).
        // Skips the first frame after meshing: it includes one-off driver warm-up
        // (first multi-draw), which is not a steady-state cost.
        if (frame > 1) frameStats.add((now - last) * 1000.0);
        clock.advance(now - last);
        last = now;
        for (int i = 0; i < clock.ticksDue; ++i) {
            player.tick(readMoveInput(window));
        }

        int fbWidth = 0;
        int fbHeight = 0;
        window.framebufferSize(fbWidth, fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            // Minimised: swapBuffers may not block, so sleep instead of spinning a core.
            window.waitEvents(0.05);
            continue;
        }

        mc::gfx::Camera camera;
        camera.position = player.renderPosition(clock.alpha);
        camera.yaw = player.yaw();
        camera.pitch = player.pitch();
        renderer.update(world, camera.position);
        renderer.drawFrame(camera, fbWidth, fbHeight);

        if (!meshed && renderer.pendingMeshes() == 0) {
            meshed = true;
            const auto& st = renderer.stats();
            MC_LOG_INFO("World meshed in %.0f ms: %d sections, %llu quads",
                        (mc::timeSeconds() - startTime) * 1000.0, st.sections,
                        static_cast<unsigned long long>(st.quadsTotal));
        }

        // Screenshots wait until every section is meshed, then count frames.
        if (meshed) ++frame;
        if (screenshotMode && frame >= opts->screenshotFrames) {
            if (!mc::gfx::saveScreenshot(opts->screenshotPath.c_str(), fbWidth, fbHeight)) {
                exitCode = 1;
            }
            break;
        }
        window.swapBuffers();
    }
    const auto summary = frameStats.summarize();
    const auto& st = renderer.stats();
    // Frame times count only frames after meshing finished (steady state).
    MC_LOG_INFO("Frame time over %zu frames: avg %.2f ms (%.0f fps), p99 %.2f ms, max %.2f ms%s",
                summary.frames, summary.avgMs, summary.avgMs > 0 ? 1000.0 / summary.avgMs : 0.0,
                summary.p99Ms, summary.maxMs, opts->vsync ? " [vsync on]" : "");
    MC_LOG_INFO("Last frame: sections drawn %d/%d, quads drawn %llu", st.sectionsDrawn, st.sections,
                static_cast<unsigned long long>(st.quadsDrawn));
    return exitCode;
}
