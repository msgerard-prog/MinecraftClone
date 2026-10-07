#include "core/CommandLine.h"
#include "core/FrameStats.h"
#include "core/GameClock.h"
#include "core/Log.h"
#include "core/Window.h"
#include "gameplay/Player.h"
#include "rendering/Camera.h"
#include "rendering/GlContext.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/Screenshot.h"
#include "rendering/WorldRenderer.h"
#include "world/Blocks.h"
#include "world/ChunkLoader.h"
#include "world/FlatGenerator.h"
#include "world/Raycast.h"
#include "world/Rotation.h"
#include "world/TerrainGenerator.h"
#include "world/World.h"

#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

// Vanilla controls: WASD move, space jump (double-tap: fly), shift sneak, ctrl sprint.
mc::PlayerInput readInput(const mc::Window& window) {
    mc::PlayerInput in;
    if (!window.cursorCaptured()) return in;
    in.forward = float(window.keyDown(mc::Key::W)) - float(window.keyDown(mc::Key::S));
    in.strafe = float(window.keyDown(mc::Key::D)) - float(window.keyDown(mc::Key::A));
    in.jump = window.keyDown(mc::Key::Space);
    in.sneak = window.keyDown(mc::Key::LeftShift);
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

// Vanilla-like spawn: the nearest land column to the origin (spiral search), feet on
// its surface.
glm::dvec3 findSpawn(const mc::world::TerrainGenerator& gen) {
    for (int r = 0; r <= 256; r += 4) {
        for (int i = -r; i <= r; i += 4) {
            const int pts[4][2] = {{i, -r}, {i, r}, {-r, i}, {r, i}};
            for (const auto& p : pts) {
                const int h = gen.surfaceHeight(p[0], p[1]);
                if (h >= mc::world::TerrainGenerator::kSeaLevel)
                    return {p[0] + 0.5, h + 1.0, p[1] + 0.5};
            }
        }
    }
    return {0.5, mc::world::TerrainGenerator::kSeaLevel + 1.0, 0.5};
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
    if (!renderer.init(opts->resourcePacks.empty() ? std::string(MC_RESOURCEPACKS_DIR)
                                                   : opts->resourcePacks))
        return 1;
    mc::gfx::OverlayRenderer overlay;
    if (!overlay.init()) return 1;
    mc::world::World world;
    glm::dvec3 spawn(0.5, -60.0, -6.0); // flat world: feet on the grass
    const mc::world::TerrainGenerator generator(opts->seed);
    std::unique_ptr<mc::world::ChunkLoader> loader;
    if (opts->flat) {
        buildTestWorld(world);
        renderer.setRenderDistance(8);
        renderer.markAllDirty(world);
    } else {
        // Chunks stream in around the player on worker threads (a quarter of the cores;
        // meshing has half).
        const int genThreads =
            std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 4);
        loader = std::make_unique<mc::world::ChunkLoader>(world, generator, genThreads);
        loader->setRenderDistance(opts->renderDistance);
        renderer.setRenderDistance(opts->renderDistance);
        spawn = findSpawn(generator);
    }
    std::vector<mc::world::ChunkPos> loadedChunks;
    std::vector<mc::world::ChunkPos> unloadedChunks;
    loadedChunks.reserve(256);
    unloadedChunks.reserve(256);

    mc::Player player;
    // The flight benchmark starts high above spawn so it never hits terrain.
    player.setPosition(opts->hasPos ? opts->pos : spawn + glm::dvec3(0, opts->autoFly ? 60 : 0, 0));
    // Scripted views (--pos) and the flight benchmark start in the air: fly.
    player.setFlying(opts->hasPos || opts->autoFly);
    player.setRotation(opts->hasLook ? opts->yaw : 0.0f, opts->hasLook ? opts->pitch : 25.0f);

    if (opts->autoFly) player.setFlySpeedMultiplier(4.0);
    mc::GameClock clock;
    double last = mc::timeSeconds();
    int frame = 0;
    int exitCode = 0;
    mc::FrameStats frameStats;
    mc::FrameStats workStats; // CPU time per frame before the swap (excludes vsync/cap)
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
            mc::PlayerInput input = readInput(window);
            if (opts->autoFly) { // benchmark: constant sprint-flight forward
                input.forward = 1.0f;
                input.sprint = true;
            }
            player.tick(world, input);
            renderer.tick();
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
        camera.position = player.eyePosition(clock.alpha);
        camera.yaw = player.yaw();
        camera.pitch = player.pitch();
        if (loader) {
            const mc::world::ChunkPos center{
                mc::world::blockToChunk(static_cast<int32_t>(std::floor(camera.position.x))),
                mc::world::blockToChunk(static_cast<int32_t>(std::floor(camera.position.z)))};
            loader->update(center, loadedChunks, unloadedChunks);
            renderer.onChunksUnloaded(unloadedChunks);
            renderer.onChunksLoaded(world, loadedChunks);
        }
        renderer.update(world, camera.position);
        renderer.drawFrame(camera, fbWidth, fbHeight);

        // Targeted block: from the eye along the look direction, creative reach.
        const auto hit = mc::world::raycastBlocks(
            world, camera.position, glm::dvec3(mc::world::lookVector(camera.yaw, camera.pitch)),
            mc::world::kCreativeReach);
        overlay.draw(camera, fbWidth, fbHeight,
                     hit ? std::optional<mc::world::BlockPos>(hit->block) : std::nullopt);

        if (!meshed && renderer.pendingMeshes() == 0 && (!loader || loader->pending() == 0)) {
            meshed = true;
            renderer.resetGpuStats(); // steady-state GPU numbers, like the CPU stats
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
        if (frame > 1) workStats.add((mc::timeSeconds() - now) * 1000.0);
        window.swapBuffers();
        if (opts->maxFps > 0) { // simple frame cap (benchmarks): sleep off the rest
            const double target = 1.0 / opts->maxFps;
            while (mc::timeSeconds() - now < target)
                std::this_thread::yield();
        }
    }
    const auto summary = frameStats.summarize();
    const auto& st = renderer.stats();
    // Frame times count only frames after meshing finished (steady state).
    MC_LOG_INFO("Frame time over %zu frames: avg %.2f ms (%.0f fps), p99 %.2f ms, max %.2f ms%s",
                summary.frames, summary.avgMs, summary.avgMs > 0 ? 1000.0 / summary.avgMs : 0.0,
                summary.p99Ms, summary.maxMs, opts->vsync ? " [vsync on]" : "");
    MC_LOG_INFO("Last frame: sections drawn %d/%d, quads drawn %llu", st.sectionsDrawn, st.sections,
                static_cast<unsigned long long>(st.quadsDrawn));
    const auto work = workStats.summarize();
    MC_LOG_INFO("CPU work per frame: avg %.2f ms, p99 %.2f ms, max %.2f ms", work.avgMs, work.p99Ms,
                work.maxMs);
    MC_LOG_INFO("GPU time per frame: avg %.2f ms, max %.2f ms", renderer.averageGpuMs(),
                renderer.maxGpuMs());
    const auto& tst = renderer.translucentStats();
    MC_LOG_INFO("Last frame (translucent): sections drawn %d/%d, quads drawn %llu",
                tst.sectionsDrawn, tst.sections, static_cast<unsigned long long>(tst.quadsDrawn));
    return exitCode;
}
