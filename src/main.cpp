#include "audio/SoundEngine.h"
#include "core/CommandLine.h"
#include "core/FileLock.h"
#include "core/FrameStats.h"
#include "core/GameClock.h"
#include "core/Log.h"
#include "core/Options.h"
#include "core/Version.h"
#include "core/Window.h"
#include "gameplay/Bundles.h"
#include "gameplay/Beacons.h"
#include "gameplay/Beds.h"
#include "gameplay/BlockInteraction.h"
#include "gameplay/Brewing.h"
#include "gameplay/Buckets.h"
#include "gameplay/Cartography.h"
#include "gameplay/Combat.h"
#include "gameplay/CommandBlocks.h"
#include "gameplay/Commands.h"
#include "gameplay/Dispensers.h"
#include "gameplay/DragonFight.h"
#include "gameplay/Enchanting.h"
#include "gameplay/ExperienceOrbs.h"
#include "gameplay/Explosion.h"
#include "gameplay/FallingBlocks.h"
#include "gameplay/Fireworks.h"
#include "gameplay/Fishing.h"
#include "gameplay/FluidContact.h"
#include "gameplay/Furnace.h"
#include "gameplay/Grindstone.h"
#include "gameplay/Hoppers.h"
#include "gameplay/Inventory.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Jukebox.h"
#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "gameplay/Particles.h"
#include "gameplay/Patrols.h"
#include "gameplay/Player.h"
#include "gameplay/PlayerAnimation.h"
#include "gameplay/Portals.h"
#include "gameplay/PrimedTnt.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Raids.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "gameplay/WanderingTraders.h"
#include "rendering/Camera.h"
#include "rendering/EntityRenderer.h"
#include "rendering/Frustum.h"
#include "rendering/GlContext.h"
#include "rendering/GuiRenderer.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/Screenshot.h"
#include "rendering/WorldRenderer.h"
#include "ui/BookScreen.h"
#include "ui/Chat.h"
#include "ui/ContainerScreen.h"
#include "ui/CreativeInventory.h"
#include "ui/Hud.h"
#include "ui/Menus.h"
#include "ui/SignEditor.h"
#include "world/Advancements.h"
#include "world/Beehives.h"
#include "world/BlockShapes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/ChunkLoader.h"
#include "world/ChunkStorage.h"
#include "world/DayTime.h"
#include "world/Enchantments.h"
#include "world/FlatGenerator.h"
#include "world/ItemExtras.h"
#include "world/LevelData.h"
#include "world/LightManager.h"
#include "world/Maps.h"
#include "world/NetherGenerator.h"
#include "world/OverworldGenerator.h"
#include "world/Potions.h"
#include "world/Rails.h"
#include "world/Raycast.h"
#include "world/Rotation.h"
#include "world/Statistics.h"
#include "world/TerrainGenerator.h"
#include "world/World.h"
#include "world/WorldList.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>
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
    // 12x12 chunks: the inner 8x8 get lit neighbourhoods and meshes (render distance 8).
    for (int cz = -6; cz < 6; ++cz) {
        for (int cx = -6; cx < 6; ++cx)
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
    // A dark stone room (light check): interior x 9..13, z -5..-1, a doorway in the
    // south wall, a glass window in the east wall, a torch on the floor and
    // glowstone in the ceiling.
    const BlockStateId stone = r.defaultState(blocks::Stone);
    for (int bx = 8; bx <= 14; ++bx)
        for (int bz = -6; bz <= 0; ++bz) {
            world.setBlock({bx, y + 4, bz}, stone); // roof
            if (bx == 8 || bx == 14 || bz == -6 || bz == 0)
                for (int dy = 0; dy < 4; ++dy)
                    world.setBlock({bx, y + dy, bz}, stone);
        }
    world.setBlock({11, y, 0}, 0); // doorway
    world.setBlock({11, y + 1, 0}, 0);
    world.setBlock({14, y + 1, -3}, r.defaultState(blocks::Glass));
    world.setBlock({9, y, -5}, r.defaultState(blocks::Torch));
    world.setBlock({13, y + 4, -1}, r.defaultState(blocks::Glowstone));
    // Its unlit twin to the east (x 16..22): only sky light through the doorway.
    for (int bx = 16; bx <= 22; ++bx)
        for (int bz = -6; bz <= 0; ++bz) {
            world.setBlock({bx, y + 4, bz}, stone);
            if (bx == 16 || bx == 22 || bz == -6 || bz == 0)
                for (int dy = 0; dy < 4; ++dy)
                    world.setBlock({bx, y + dy, bz}, stone);
        }
    world.setBlock({19, y, 0}, 0);
    world.setBlock({19, y + 1, 0}, 0);
}

// A new player's spawn: the generator's spawn column ignores trees and plants, so
// once that chunk exists, pick the nearest column (within 8 blocks) whose highest
// solid block is ground - not a log or leaves - and stand on it.
std::optional<glm::dvec3> settleSpawn(const mc::world::World& world, glm::dvec3 spawn) {
    using namespace mc::world;
    const auto& r = blockRegistry();
    const int sx = static_cast<int>(std::floor(spawn.x)),
              sz = static_cast<int>(std::floor(spawn.z));
    if (!world.chunk({blockToChunk(sx), blockToChunk(sz)})) return std::nullopt;
    for (int radius = 0; radius <= 8; ++radius)
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                const int x = sx + dx, z = sz + dz;
                if (!world.chunk({blockToChunk(x), blockToChunk(z)})) continue;
                for (int y = world.height().maxY(); y > world.height().minY; --y) {
                    const BlockStateId s = world.getBlock({x, y, z});
                    const BlockId b = r.blockOf(s);
                    if (b == blocks::Water || b == blocks::Lava) break; // not on a sea floor
                    if (s == 0 || !r.collides(s)) continue;             // air, plants, snow layers
                    const std::string_view id = r.block(r.blockOf(s)).id;
                    if (id.ends_with("_log") || id.ends_with("_leaves")) break; // a tree
                    return glm::dvec3(x + 0.5, y + 1.0, z + 0.5);
                }
            }
    return glm::dvec3(spawn); // nothing suitable nearby: keep the generator's spawn
}

// --demo-edit: drives the real click path (raycast -> BlockInteraction -> World ->
// re-mesh) with scripted look directions, so a screenshot can verify editing.
void runDemoEdit(mc::world::World& world, mc::Player& player, mc::Inventory& inventory,
                 std::vector<mc::world::BlockPos>& edits) {
    std::vector<mc::world::BlockPos> changed;
    const float yaw = player.yaw(), pitch = player.pitch();
    auto click = [&](float y, float p, bool attack, int slot) {
        player.setRotation(y, p);
        inventory.select(slot);
        mc::InteractionInput in;
        in.attack = attack;
        in.use = !attack;
        mc::BlockInteraction fresh; // no cooldown between scripted clicks
        fresh.tick(world, player, mc::BlockInteraction::target(world, player),
                   inventory.placeState(), in, changed);
        edits.insert(edits.end(), changed.begin(), changed.end());
    };
    for (int i = 0; i < 3; ++i)
        click(yaw, 55.0f, true, 0); // dig in front
    for (int i = 0; i < 3; ++i)
        click(yaw + 35.0f, 35.0f, false, 4); // planks, right
    for (int i = 0; i < 4; ++i)
        click(yaw - 40.0f, 60.0f - i * 8.0f, false, 5); // logs, left
    player.setRotation(yaw, pitch);
}

} // namespace

namespace {

// What a world session needs from main (M22.5: sessions start and end from the menus).
struct Shared {
    mc::Window& window;
    mc::gfx::WorldRenderer& renderer;
    mc::gfx::OverlayRenderer& overlay;
    mc::gfx::GuiRenderer& gui;
    mc::audio::SoundEngine& audio;
    std::vector<std::vector<int>>& soundHandles;
    mc::GameOptions& options;
    std::filesystem::path optionsFile;
    mc::ui::Menu& menu;
    mc::ui::MenuState& menuState;
    uint16_t dirtSprite;
    // Command-line overrides (never written to options.txt): --render-distance holds
    // until the slider moves away from options.txt's value; --no-vsync for the run.
    int cliRenderDistance = 0;     // 0: none
    int optionsRenderDistance = 0; // options.txt's value when the game started
    bool cliNoVsync = false;
};
enum class SessionEnd { Quit, ToTitle };

// Settings that live outside a world (volume, VSync, GUI scale).
void applyGlobalOptions(Shared& shared) {
    shared.audio.setMasterVolume(shared.options.masterVolume);
    shared.window.setVsync(shared.options.vsync && !shared.cliNoVsync);
    mc::gfx::GuiRenderer::setScaleSetting(shared.options.guiScale);
}

// One frame of the menu screen: input in GUI pixels, the widgets, the GUI draw. With
// `clear` the screen is cleared first (no world behind it).
mc::ui::MenuAction drawMenuFrame(Shared& shared, int fbWidth, int fbHeight, bool clear) {
    mc::Window& window = shared.window;
    const int scale = mc::gfx::GuiRenderer::guiScale(fbWidth, fbHeight);
    mc::ui::MenuInput in;
    window.cursorPos(in.mx, in.my);
    in.mx /= scale;
    in.my /= scale;
    in.click = window.takePresses(mc::Press::LeftMouse) > 0;
    in.mouseDown = window.leftMousePressed();
    in.wheel = window.scrollDelta();
    static char typed[64];
    int n = window.takeText(typed, int(sizeof(typed)));
    for (int b = window.takePresses(mc::Press::Backspace); b > 0 && n < int(sizeof(typed)); --b)
        typed[n++] = '\b';
    in.typed = std::string_view(typed, size_t(n));
    in.enter = window.takePresses(mc::Press::Enter) > 0;
    in.escape = window.takePresses(mc::Press::Escape) > 0;
    in.timeMs = int64_t(mc::timeSeconds() * 1000.0);
    if (clear) shared.renderer.clearScreen(fbWidth, fbHeight);
    shared.menu.begin(shared.gui.batch(), fbWidth / scale, fbHeight / scale, in);
    const auto action = mc::ui::drawMenu(shared.menu, shared.menuState, shared.options,
                                         shared.dirtSprite, mc::version());
    if (action == mc::ui::MenuAction::OptionsChanged || action == mc::ui::MenuAction::OptionsClosed)
        applyGlobalOptions(shared);
    if (action == mc::ui::MenuAction::OptionsClosed) shared.options.save(shared.optionsFile);
    shared.gui.draw(fbWidth, fbHeight);
    return action;
}

// One world from loading to saving: the game loop. Returns the exit code; `end` says
// whether the player quit the game or went back to the title screen.
int runSession(Shared& shared, mc::LaunchOptions* opts, SessionEnd& sessionEnd) {
    mc::Window& window = shared.window;
    mc::gfx::WorldRenderer& renderer = shared.renderer;
    mc::gfx::OverlayRenderer& overlay = shared.overlay;
    mc::gfx::GuiRenderer& gui = shared.gui;
    mc::audio::SoundEngine& audio = shared.audio;
    std::vector<std::vector<int>>& soundHandles = shared.soundHandles;
    const bool screenshotMode = !opts->screenshotPath.empty();
    sessionEnd = SessionEnd::Quit;
    renderer.clearWorld(); // (the previous world's meshes)
    mc::ui::Chat chat;
    mc::ui::SignEditor signEditor; // (M23.3c)
    bool signClick = false, signDone = false;
    mc::ui::BookScreen bookScreen; // (M28.2c)
    bool bookClick = false;
    mc::ui::BookScreen::Action bookAction = mc::ui::BookScreen::Action::None;
    mc::ui::DebugScreen debugScreen;
    mc::gfx::ItemIcons itemIcons;
    itemIcons.build(renderer.atlas());
    mc::ui::CreativeInventory creative;
    mc::ui::ContainerScreen container; // survival inventory, crafting table, furnace
    mc::world::BlockPos containerBlock{};
    mc::world::BlockPos commandEditing{}; // (M29.7) the command block whose screen is open
    uint64_t commandEditingCart = 0;       // (or the command block minecart's)
    std::optional<mc::world::BlockPos> chestSecond; // a double chest's second half
    std::vector<mc::world::ItemStack> screenDrops;
    screenDrops.reserve(16);
    std::vector<mc::world::ItemStack> pendingThrows; // thrown from screens: spawned in the tick
    std::vector<mc::world::BlockPos> litChanges;
    std::vector<mc::world::ItemStack> lootScratch; // decayed leaves' loot (reused)
    lootScratch.reserve(8);
    litChanges.reserve(16);
    pendingThrows.reserve(16);
    creative.build(renderer.models());
    // --inventory: the creative screen, or in survival the inventory (2x2 crafting) screen;
    // opened after the first tick so --command "/gamemode ..." applies first.
    bool openInventoryPending = opts->inventory;
    bool numberWasDown[mc::Inventory::kHotbar] = {};
    bool showDebug = opts->debugScreen;
    // (M30.1) F5's view: 0 first person, 1 third person behind, 2 in front; the body's animation.
    int perspective = opts->perspective;
    mc::PlayerAnimation playerAnim;
    glm::dvec3 animPrev(0.0);
    bool animStarted = false;
    std::array<char, 64> typed{};
    mc::world::World world;
    glm::dvec3 spawn(0.5, -60.0, -6.0); // flat world: feet on the grass

    // Saved world (saves/<name>, vanilla Anvil layout, ADR 0007). Interactive runs
    // save to "New World" by default; scripted runs (screenshots, hidden) only with
    // --world. An existing world's level.dat decides seed, generator, time and player.
    std::string worldName = opts->world;
    if (worldName.empty() && !screenshotMode && !opts->hidden) worldName = "New World";
    if (opts->noSave) worldName.clear();
    const std::filesystem::path worldDir = worldName.empty()
                                               ? std::filesystem::path()
                                               : std::filesystem::path(MC_SAVES_DIR) / worldName;
    std::optional<mc::world::LevelData> level;
    mc::FileLock sessionLock; // one game instance per world (vanilla session.lock)
    if (!worldName.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(worldDir, ec);
        if (!sessionLock.acquire(worldDir / "session.lock")) {
            MC_LOG_ERROR("World \"%s\" is open in another instance", worldName.c_str());
            return 1;
        }
        level = mc::world::LevelData::load(worldDir);
        // Chunks without settings would be mixed with another seed's terrain.
        if (!level && std::filesystem::exists(worldDir / "region", ec)) {
            MC_LOG_ERROR("World \"%s\" has region files but no readable level.dat; not "
                         "opening it (restore level.dat or level.dat_old)",
                         worldName.c_str());
            return 1;
        }
    }
    const bool flatWorld = level ? level->flat : opts->flat;
    const uint64_t seed = level ? level->seed : opts->seed;
    if (level)
        MC_LOG_INFO("Loading world \"%s\" (seed %lld)", worldName.c_str(),
                    static_cast<long long>(seed));
    else if (!worldName.empty())
        MC_LOG_INFO("Creating world \"%s\"", worldName.c_str());
    // The player's dimension (M12): saved in level.dat; --dimension starts elsewhere.
    using mc::world::Dimension;
    Dimension dimension = Dimension::Overworld;
    if (level)
        if (const auto d = mc::world::findDimension(level->dimension)) dimension = *d;
    if (!opts->dimension.empty() && !flatWorld)
        dimension = *mc::world::findDimension(opts->dimension);
    // Each dimension saves in its own folder (vanilla: DIM-1 Nether, DIM1 End).
    auto dimensionDir = [&](Dimension d) {
        return worldDir / std::string(mc::world::dimensionInfo(d).folder);
    };
    // Worlds saved before kCloneFormat keep their old format number, so chunks not
    // saved since are still upgraded on later visits.
    const int32_t cloneFormat = level ? level->cloneFormat : mc::world::kCloneFormat;
    const bool legacyWorld = cloneFormat < 1;
    std::unique_ptr<mc::world::ChunkStorage> storage;
    if (!worldName.empty())
        storage = std::make_unique<mc::world::ChunkStorage>(dimensionDir(dimension), legacyWorld);
    // Storages of dimensions left behind, still writing (see the dimension switch).
    struct RetiringStorage {
        std::filesystem::path dir;
        std::thread thread;
        RetiringStorage(std::filesystem::path d, std::thread t)
            : dir(std::move(d)), thread(std::move(t)) {}
        RetiringStorage(RetiringStorage&&) = default;
        RetiringStorage& operator=(RetiringStorage&&) = default;
        ~RetiringStorage() {
            if (thread.joinable()) thread.join(); // everything is written before exit
        }
    };
    std::vector<RetiringStorage> retiringStorage;
    // The Overworld's generator: saved worlds keep theirs (pinned outputs never change).
    const std::string generatorKind = level ? level->generator : opts->generator;
    // The Nether's generator (M19): new worlds get the newest; old ones keep theirs.
    const std::string netherKind = level ? level->netherGenerator : std::string("nether4");
    const std::string endKind = level ? level->endGenerator : std::string("end2"); // (M20)
    if (endKind != "end" && endKind != "end2") {
        MC_LOG_ERROR("World \"%s\" uses End generator \"%s\", which this build doesn't have",
                     worldName.c_str(), endKind.c_str());
        return 1;
    }
    if (netherKind != "nether" && netherKind != "nether2" && netherKind != "nether3" && netherKind != "nether4") {
        MC_LOG_ERROR("World \"%s\" uses Nether generator \"%s\", which this build doesn't have",
                     worldName.c_str(), netherKind.c_str());
        return 1;
    }
    if (generatorKind != "terrain" &&
        mc::world::OverworldGenerator::versionOf(generatorKind) == 0) {
        // A world from a newer/other build: generating here would leave seams.
        MC_LOG_ERROR("World \"%s\" uses generator \"%s\", which this build doesn't have",
                     worldName.c_str(), generatorKind.c_str());
        return 1;
    }
    auto makeGenerator = [&](Dimension d) -> std::unique_ptr<mc::world::ChunkGenerator> {
        if (d == Dimension::Nether)
            return std::make_unique<mc::world::NetherGenerator>(seed, netherKind == "nether"    ? 1
                                                                      : netherKind == "nether2" ? 2
                                                                      : netherKind == "nether3" ? 3
                                                                                                : 4);
        if (d == Dimension::End)
            return std::make_unique<mc::world::EndGenerator>(seed, endKind == "end" ? 1 : 2);
        if (generatorKind == "terrain") return std::make_unique<mc::world::TerrainGenerator>(seed);
        return std::make_unique<mc::world::OverworldGenerator>(
            seed, mc::world::OverworldGenerator::versionOf(generatorKind));
    };
    std::unique_ptr<mc::world::ChunkGenerator> generatorPtr = makeGenerator(dimension);
    world.setHasSkyLight(mc::world::dimensionInfo(dimension).hasSkyLight);
    world.setHeight(mc::world::dimensionInfo(dimension).height); // vanilla: per dimension type
    world.setUltrawarm(dimension == Dimension::Nether);
    renderer.setDimension(dimension);
    std::unique_ptr<mc::world::ChunkLoader> loader;
    std::vector<mc::world::ChunkPos> loadedChunks;
    std::vector<mc::world::ChunkPos> unloadedChunks;
    loadedChunks.reserve(256);
    unloadedChunks.reserve(256);
    if (flatWorld) {
        buildTestWorld(world);
        // Saved chunks replace the generated ones; generated ones save too (vanilla).
        world.forEachChunk([&](mc::world::Chunk& c) {
            if (!storage || storage->load(c)) c.clearDirty();
            if (!c.furnaces().empty() || !c.mobs().empty() || !c.blockTicks().empty() ||
                !c.spawners().empty() || !c.brewingStands().empty())
                world.markTicking(c.pos());
        });
        renderer.setRenderDistance(8);
        // The fixed world counts as "loaded" once, on the first frame (lighting, meshing).
        world.forEachChunk([&](const mc::world::Chunk& c) { loadedChunks.push_back(c.pos()); });
    }
    // Chunks stream in around the player on worker threads (a third of the cores:
    // the overworld costs ~1 ms per chunk; meshing has half).
    const int genThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 3);
    if (!flatWorld) {
        loader = std::make_unique<mc::world::ChunkLoader>(world, *generatorPtr, genThreads,
                                                          storage.get());
        loader->setRenderDistance(opts->renderDistance);
        renderer.setRenderDistance(opts->renderDistance);
        spawn = generatorPtr->findSpawn();
    }
    // Lighting on worker threads (a quarter of the cores).
    mc::world::LightManager lighting(
        world, std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 4));
    std::vector<mc::world::ChunkPos> litChunks;
    std::vector<mc::world::SectionPos> relitSections;
    std::vector<mc::world::BlockPos> frameEdits;  // all block edits this frame (for lighting)
    std::vector<mc::world::BlockPos> frameRemesh; // edits that don't change light: re-mesh at once
    std::vector<mc::world::BlockPos>
        frameSettling; // flowing fluids: re-mesh now, relight in the background
    frameSettling.reserve(4096);
    frameRemesh.reserve(4096);
    litChunks.reserve(256);
    relitSections.reserve(256);
    frameEdits.reserve(4096);
    std::vector<mc::world::BlockPos> editsReady;
    editsReady.reserve(4096); // (settling fluids come straight back through it)

    int64_t dayTime = level ? level->dayTime : opts->time; // world day time (world/DayTime.h)
    int64_t gameTime = level ? level->gameTime : 0;        // ticks since the world began
    // Weather (M22.1, world/Weather): one state for the world, shown in the Overworld.
    mc::world::Weather weather;
    if (level) {
        weather.raining = level->raining;
        weather.thundering = level->thundering;
        weather.rainTime = level->rainTime;
        weather.thunderTime = level->thunderTime;
        weather.clearTime = level->clearWeatherTime;
        weather.rain = weather.prevRain = weather.raining ? 1.0f : 0.0f;
        weather.thunder = weather.prevThunder = weather.raining && weather.thundering ? 1.0f : 0.0f;
    }
    std::vector<mc::world::BlockPos> commandBolts; // /summon lightning_bolt (struck next tick)
    commandBolts.reserve(16);
    struct Bolt {
        glm::dvec3 pos;
        uint32_t seed;
        int ticks; // left on screen
    };
    std::vector<Bolt> bolts;
    bolts.reserve(32);
    int skyFlash = 0;        // ticks the sky stays lit by a bolt (vanilla skyFlashTime)
    mc::Particles particles; // M22.3 (visual only)
    mc::world::Xoroshiro particleRng(0x9a77'1c1eull); // (keeps gameRng's sequence for gameplay)

    mc::Player player;
    // The flight benchmark starts high above spawn so it never hits terrain.
    // (the flight benchmark flies at Y 220+, above any mountain, so it keeps streaming)
    player.setPosition(opts->hasPos ? opts->pos
                       : opts->autoFly
                           ? glm::dvec3(spawn.x, std::max(spawn.y + 60.0, 220.0), spawn.z)
                           : spawn);
    // Scripted views (--pos) and the flight benchmark start in the air: fly.
    player.setFlying(opts->hasPos || opts->autoFly);
    player.setRotation(opts->hasLook ? opts->yaw : 0.0f, opts->hasLook ? opts->pitch : 25.0f);

    if (opts->autoFly) player.setFlySpeedMultiplier(4.0);
    mc::Inventory inventory;
    // Survival (M9): game mode, health/hunger, dropped items.
    bool survival = level ? level->survival : opts->survival; // (new worlds: the menu's game mode)
    // M28.1: the world's game rules, difficulty and game mode (0 survival .. 3 spectator).
    mc::world::GameRules rules = level ? level->rules : mc::world::GameRules{};
    int difficulty = level ? level->difficulty : opts->difficulty;
    int gameMode = level ? level->gameMode : (survival ? 0 : 1);
    mc::Vitals vitals;
    vitals.setVoidY(mc::world::dimensionInfo(dimension).voidY);
    if (level) {
        vitals.setState(level->health, level->food, level->saturation, level->exhaustion);
        vitals.setFoodTimer(level->foodTimer);
        vitals.setAir(level->air);
        vitals.setFireTicks(level->fire);
        vitals.setExperience(level->xpLevel, level->xpProgress, level->xpTotal);
        vitals.setEnchantSeed(uint32_t(level->xpSeed));
        vitals.setTimeSinceRest(level->timeSinceRest);
        vitals.setWardenTracker(level->wardenLevel, level->wardenTicks, level->wardenCooldown);
        for (const auto& e : level->effects) // (kinds we don't have are dropped)
            if (const auto kind = mc::world::findEffect(e.id))
                vitals.addEffect(*kind, e.amplifier, e.duration);
        vitals.setHealth(level->health);         // (again: Health Boost may allow more - M29.2a)
        vitals.setAbsorption(level->absorption); // (after the effects, which refill it)
    }
    mc::DragonFight dragonFight;              // (M20.2; end2 worlds)
    mc::WanderingTraderSpawner traderSpawner; // (M24.4)
    mc::PatrolSpawner patrolSpawner;          // (M24.4)
    mc::Raid raid;                            // (M24.5)
    // The player's UUID (M26.1): pets' Owner; made once for a world without one.
    uint64_t playerUuidHi = 0, playerUuidLo = 0;
    if (level) {
        raid.restore({level->raidActive,
                      {level->raidCentre[0], level->raidCentre[1], level->raidCentre[2]},
                      level->raidWave,
                      level->raidWaves,
                      level->raidLevel,
                      level->raidTicks,
                      level->raidCooldown,
                      level->raidWaveHealth,
                      level->raidId,
                      level->raidNextId,
                      level->raidPendingTicks,
                      level->raidPendingLevel,
                      {level->raidPendingCentre[0], level->raidPendingCentre[1],
                       level->raidPendingCentre[2]}});
        dragonFight.killed = level->dragonKilled;
        traderSpawner.delay = level->traderSpawnDelay;
        playerUuidHi = level->playerUuidHi;
        playerUuidLo = level->playerUuidLo;
        traderSpawner.chance = level->traderSpawnChance;
        dragonFight.previouslyKilled = level->dragonPreviouslyKilled;
        dragonFight.uuidHi = level->dragonUuidHi;
        dragonFight.uuidLo = level->dragonUuidLo;
        dragonFight.gateways = level->gateways;
        dragonFight.gatewaysReady = level->hasGateways;
    }
    if (playerUuidHi == 0 && playerUuidLo == 0) { // (a version-4 UUID for this world's player)
        mc::world::Xoroshiro uuidRng(seed ^ 0x9E3779B97F4A7C15ull ^ uint64_t(std::time(nullptr)));
        playerUuidHi = (uuidRng.nextLong() & ~0xF000ull) | 0x4000ull;
        playerUuidLo = (uuidRng.nextLong() & ~(3ull << 62)) | (2ull << 62);
    }
    mc::world::setPlayerUuid(playerUuidHi, playerUuidLo); // (written as pets' Owner)
    // Statistics (M28.1d): saves/<world>/stats/<uuid>.json, as vanilla.
    mc::world::Statistics stats;
    if (!worldDir.empty())
        if (auto s = mc::world::Statistics::load(
                mc::world::Statistics::file(worldDir, playerUuidHi, playerUuidLo)))
            stats = std::move(*s);
    shared.menuState.stats = &stats;
    // Advancements (M28.5c; wiki: Advancement): saves/<world>/advancements/<uuid>.json. Each
    // one made is announced in the chat (if the game rule allows) and with a toast.
    mc::world::Advancements adv;
    if (!worldDir.empty())
        if (auto a = mc::world::Advancements::load(
                mc::world::Advancements::file(worldDir, playerUuidHi, playerUuidLo)))
            adv = std::move(*a);
    shared.menuState.adv = &adv;
    Dimension advDimension = dimension; // (entering another one is an advancement)
    // Maps (M28.2b): data/map_<id>.dat; the held one's picture is uploaded when it changes.
    mc::world::Maps maps;
    if (!worldDir.empty()) maps.load(worldDir);
    std::vector<uint8_t> mapRgba(128 * 128 * 4, 0);
    int shownMap = -1;
    uint32_t shownMapVersion = 0;
    mc::ItemEntities droppedItems;
    mc::FallingBlocks fallingBlocks; // sand and gravel in the air (M16)
    mc::Projectiles projectiles;     // arrows and eggs (M16.4)
    mc::Fishing fishing;             // the cast bobber (M25.2)
    mc::ExperienceOrbs orbs;         // experience orbs (M17.5)
    mc::Explosion fireballBlast;     // ghast fireballs (M19.2)
    mc::Explosion tntBlast;          // (M21.1b)
    mc::PrimedTnt primedTnt;
    int bowTicks = 0;              // how long the bow has been drawn
    int crossbowTicks = 0;         // (M28.4a) how long the crossbow has been loading
    int elytraBoost = 0;           // (M28.4c) ticks of a firework's push left while gliding
    int spearCharge = 0;           // (M28.4e) ticks the spear has been held out
    uint64_t playerTargetUuid = 0; // the mob the player last hit (M26.1: tamed wolves join in)
    int playerTargetTicks = 0;     // (forgotten after 100 ticks, like vanilla's last-hurt memory)
    int tridentTicks = 0;          // how long a trident has been held back (M25.3)
    double airPeakY = 0.0;         // highest feet height since leaving the ground (trampling)
    int shieldTicks = 0;           // how long right-click has held a shield up
    // Beds (M17.4): the respawn point, sleeping (ticks asleep, the bed's head).
    std::optional<mc::world::BlockPos> bedSpawn;
    if (level && level->hasRespawn)
        bedSpawn = mc::world::BlockPos{level->respawn[0], level->respawn[1], level->respawn[2]};
    // (M29.5) the respawn point's dimension: the Overworld for beds, the Nether for anchors
    Dimension spawnDim = level ? Dimension(std::clamp(level->respawnDimension, 0, 2)) : Dimension::Overworld;
    // Where the player last died (M28.2a: recovery compasses).
    std::optional<std::pair<mc::world::BlockPos, Dimension>> lastDeath;
    if (level && level->hasLastDeath)
        lastDeath = {{level->lastDeath[0], level->lastDeath[1], level->lastDeath[2]},
                     Dimension(std::clamp(level->lastDeathDimension, 0, 2))};
    int sleepTicks = 0;
    mc::world::BlockPos sleepBed{};
    bool bedRespawnPending = false; // respawned at the bed: check it once its chunks load
    mc::Explosion bedExplosion;     // beds in the Nether and the End
    std::optional<mc::world::BlockPos> pendingBedUse;
    struct PendingCommandEdit { // (M29.7: the command block screen's Done)
        mc::world::BlockPos pos;
        uint64_t cart = 0;
        std::string command;
        int mode = 0;
        bool conditional = false, always = false;
    };
    std::optional<PendingCommandEdit> pendingCommandEdit;
    mc::Mobs mobs;
    mc::world::Xoroshiro gameRng(seed ^ 0x5EEDull);
    std::vector<mc::BlockInteraction::Drop> drops;
    drops.reserve(64); // (a full chest: 27 stacks and the chest)
    bool dead = false;
    mc::gfx::EntityRenderer entities;
    if (!entities.init(renderer.atlas(), renderer.models(), itemIcons, renderer.packs())) return 1;

    mc::world::Xoroshiro soundRng(0x50'0d'5eedull);
    // Plays a sound event: one of its files at random, its volume and a pitch in its range.
    auto playSound = [&](mc::world::Sound s, const glm::dvec3& pos, float volume, float pitch,
                         bool positional) {
        if (!audio.active()) return;
        const auto& handles = soundHandles[size_t(s)];
        if (handles.empty()) return;
        const auto& info = mc::world::soundInfo(s);
        const float p = info.pitchMin + (info.pitchMax - info.pitchMin) * soundRng.nextFloat();
        audio.play(handles[soundRng.nextInt(uint32_t(handles.size()))], pos, info.volume * volume,
                   p * pitch, positional);
    };
    int toastAdv = -1, toastTicks = 0; // the toast shown (vanilla: 5 s each, queued)
    std::array<int, 8> toastQueue{};
    int toastQueued = 0;
    auto announce = [&](const mc::world::Advancements::Granted& g) {
        for (const int i : g) {
            if (i < 0) break;
            const auto& a = mc::world::advancements()[size_t(i)];
            if (rules.announceAdvancements) {
                // Vanilla: "<player> has made the advancement [Title]" in green brackets
                // (goals: "reached the goal", challenges: "completed the challenge", purple).
                char line[160];
                std::snprintf(line, sizeof line, "Player has %s [%.*s]",
                              a.frame == mc::world::AdvFrame::Challenge ? "completed the challenge"
                              : a.frame == mc::world::AdvFrame::Goal    ? "reached the goal"
                                                                        : "made the advancement",
                              int(a.title.size()), a.title.data());
                chat.addMessage(line,
                                a.frame == mc::world::AdvFrame::Challenge
                                    ? mc::gfx::argb(0xFFAA00AA)
                                    : mc::gfx::argb(0xFF55FF55),
                                gameTime, gui.batch());
            }
            if (toastQueued < int(toastQueue.size())) toastQueue[size_t(toastQueued++)] = i;
            if (a.frame == mc::world::AdvFrame::Challenge)
                playSound(mc::world::Sound::LevelUp, player.position(), 1.0f, 1.0f, false);
        }
    };
    double stepDistance = 0.0, nextStep = 1.0; // footsteps (vanilla moveDist / nextStep)
    int brushTicks = 0;                        // (M27.5) how long the brush has worked the block
    float lastHealth = vitals.health();        // (the loaded values: no sound on the first tick)
    int lastXpLevel = vitals.xpLevel(), lastEatTicks = 0, rainSoundTime = 0;
    bool wasInWater = false;
    glm::dvec3 lastFeet(0.0);
    if (level && !opts->hasPos) { // resume where the player left
        player.setPosition({level->pos[0], level->pos[1], level->pos[2]});
        player.setRotation(level->yaw, level->pitch);
        player.setFlying(level->flying);
    }
    if (level) {
        for (int i = 0; i < mc::Inventory::kSlots; ++i)
            inventory.setSlot(i, {});
        for (const auto& it : level->inventory) { // unknown items are dropped (logged)
            const auto item = mc::world::itemRegistry().find(it.id);
            if (!item) {
                MC_LOG_WARN("Unknown item %s in slot %d", it.id.c_str(), it.slot);
                continue;
            }
            mc::world::ItemStack s{*item, static_cast<uint8_t>(std::clamp(it.count, 1, 64)),
                                   static_cast<uint16_t>(std::clamp(it.damage, 0, 65535))};
            if (!it.state.empty()) {
                std::string_view st(it.state);
                if (st.starts_with("minecraft:")) st.remove_prefix(10);
                if (const auto bs = mc::world::blockRegistry().parse(st)) s.state = *bs;
            }
            for (const auto& [eid, lvl] : it.enchantments)
                if (const auto e = mc::world::findEnchantment(eid))
                    mc::world::setEnchantment(s, *e, std::clamp(lvl, 1, 255));
            s.repairCost = static_cast<uint8_t>(std::clamp(it.repairCost, 0, 255));
            if (const auto p = mc::world::findPotion(it.potion))
                s.potion = static_cast<uint8_t>(*p);
            s.contents = it.contents;
            s.trim = it.trim;
            s.extra = it.extra;
            s.name = mc::world::addName(it.name); // (M29.3b)
            if (it.itemState) s.state = it.itemState;
            // Our armor slots: 100 feet .. 103 head (vanilla's old numbers); 150 offhand.
            if (it.slot >= 100 && it.slot <= 103)
                inventory.setArmor(103 - it.slot, s);
            else if (it.slot == 150)
                inventory.setOffhand(s);
            else if (it.slot >= 200 && it.slot < 227)
                inventory.enderChest().items[size_t(it.slot - 200)] = s;
            else if (it.slot >= 0 && it.slot < mc::Inventory::kSlots)
                inventory.setSlot(it.slot, s);
        }
        inventory.select(level->selectedSlot);
    }
    // World spawn: fixed when the world is created (vanilla SpawnX/Y/Z).
    int32_t worldSpawn[3] = {static_cast<int32_t>(std::floor(spawn.x)),
                             static_cast<int32_t>(std::floor(spawn.y)),
                             static_cast<int32_t>(std::floor(spawn.z))};
    if (level)
        for (int i = 0; i < 3; ++i)
            worldSpawn[i] = level->spawn[i];
    else if (dimension !=
             Dimension::Overworld) { // a new world started elsewhere: spawn is the Overworld's
        const glm::dvec3 s = makeGenerator(Dimension::Overworld)->findSpawn();
        worldSpawn[0] = int32_t(std::floor(s.x)), worldSpawn[1] = int32_t(std::floor(s.y)),
        worldSpawn[2] = int32_t(std::floor(s.z));
    }
    // Portals players lit or came through (level.dat, our tag), travel state (M12).
    std::vector<mc::portals::Known> knownPortals;
    if (level)
        for (const auto& p : level->portals)
            if (const auto d = mc::world::findDimension(p.dimension))
                knownPortals.push_back({*d, {p.x, p.y, p.z}});
    struct Travel {
        Dimension to;
        enum class Via { NetherPortal, EndPortal, Respawn } via;
        mc::world::BlockPos from; // entry block; after the switch: the destination target
        Dimension fromDimension = Dimension::Overworld;
        glm::dvec3 fromPos{0.0};
    };
    std::optional<Travel> pendingTravel;
    std::optional<Travel> arrival; // waiting for the destination's chunks
    int portalTicks = 0;
    int arrivalWait = 0; // ticks spent waiting for a remembered portal's chunk
    if (!level && dimension == Dimension::End && !opts->hasPos) // (a new run started there)
        arrival = Travel{Dimension::End,
                         Travel::Via::EndPortal,
                         {100, 49, 0},
                         Dimension::End,
                         {100.5, 49.0, 0.5}};
    // Just arrived (or loaded, maybe standing in one): step out of the portal first.
    bool portalCooldown = level.has_value();
    int pearlCooldown = 0;
    uint64_t ridingCart = 0; // (M21.4: the minecart the player sits in, by UUID; boats, mounts)
    int mountJumpTicks = 0;  // (M26.2) jump held while riding: the jump bar, 0..10
    int hornCooldown = 0;    // (M26.3) ticks before a goat horn sounds again
    int windCooldown = 0;    // (M26.4c) ticks before another wind charge
    // The cart the player rides (nullptr: none / gone), found around the player.
    auto findCart = [&]() -> mc::world::MobData* {
        if (ridingCart == 0) return nullptr;
        const mc::world::ChunkPos c0 = mc::world::BlockPos{int(std::floor(player.position().x)), 0,
                                                           int(std::floor(player.position().z))}
                                           .chunk();
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (mc::world::Chunk* c = world.chunk({c0.x + dx, c0.z + dz}))
                    for (auto& m : c->mobs())
                        if ((m.type == mc::world::MobType::Minecart || m.type == mc::world::MobType::Boat ||
                             mc::world::isMount(m.type) || mc::world::isStickRidden(m.type)) &&
                            m.uuidHi == ridingCart)
                            return &m;
        return nullptr;
    };
    int glideTicks = 0;
    int64_t sessionTicks = 0;
    // (Overworld only: elsewhere the highest ground is a roof; findSpawn is exact.)
    bool spawnPending = !level && !opts->hasPos && !opts->autoFly && !flatWorld &&
                        dimension == Dimension::Overworld;
    // Saving: dirty chunks to the IO thread, level.dat written here (small).
    auto saveWorld = [&](bool wait) {
        if (!storage) return;
        int chunks = 0;
        world.forEachChunk([&](mc::world::Chunk& c) {
            if (!c.dirty()) return;
            storage->save(mc::world::ChunkSnapshot::of(c, gameTime));
            c.clearDirty();
            ++chunks;
        });
        mc::world::LevelData l;
        l.name = level ? level->name : (opts->worldTitle.empty() ? worldName : opts->worldTitle);
        l.seed = seed;
        l.flat = flatWorld;
        l.generator = generatorKind;
        l.netherGenerator = netherKind;
        l.endGenerator = endKind;
        l.dragonKilled = dragonFight.killed;
        l.traderSpawnDelay = traderSpawner.delay;
        l.playerUuidHi = playerUuidHi;
        l.playerUuidLo = playerUuidLo;
        l.traderSpawnChance = traderSpawner.chance;
        {
            const mc::Raid::State r = raid.state();
            l.raidActive = r.active;
            l.raidCentre[0] = r.centre.x, l.raidCentre[1] = r.centre.y,
            l.raidCentre[2] = r.centre.z;
            l.raidWave = r.wave, l.raidWaves = r.waves, l.raidLevel = r.level;
            l.raidTicks = r.ticks, l.raidCooldown = r.cooldown, l.raidWaveHealth = r.waveHealth;
            l.raidId = r.id, l.raidNextId = r.nextId;
            l.raidPendingTicks = r.pendingTicks, l.raidPendingLevel = r.pendingLevel;
            l.raidPendingCentre[0] = r.pendingCentre.x, l.raidPendingCentre[1] = r.pendingCentre.y,
            l.raidPendingCentre[2] = r.pendingCentre.z;
        }
        l.dragonPreviouslyKilled = dragonFight.previouslyKilled;
        l.dragonUuidHi = dragonFight.uuidHi;
        l.dragonUuidLo = dragonFight.uuidLo;
        l.gateways = dragonFight.gateways;
        l.hasGateways = dragonFight.gatewaysReady;
        l.cloneFormat = cloneFormat;
        // Mid-travel the player is still where they left from (a reload re-enters).
        l.dimension =
            std::string(mc::world::dimensionInfo(arrival ? arrival->fromDimension : dimension).id);
        for (const auto& k : knownPortals)
            l.portals.push_back(
                {std::string(mc::world::dimensionInfo(k.dimension).id), k.pos.x, k.pos.y, k.pos.z});
        for (int i = 0; i < 3; ++i)
            l.spawn[i] = worldSpawn[i];
        l.dayTime = dayTime;
        l.gameTime = gameTime;
        l.raining = weather.raining;
        l.thundering = weather.thundering;
        l.rainTime = weather.rainTime;
        l.thunderTime = weather.thunderTime;
        l.clearWeatherTime = weather.clearTime;
        const glm::dvec3 p = arrival ? arrival->fromPos : player.position();
        l.pos[0] = p.x;
        l.pos[1] = p.y;
        l.pos[2] = p.z;
        l.yaw = player.yaw();
        l.pitch = player.pitch();
        l.flying = player.flying();
        l.survival = survival;
        l.rules = rules;
        l.difficulty = difficulty;
        l.gameMode = gameMode;
        l.health = vitals.health();
        l.absorption = vitals.absorption();
        l.food = vitals.food();
        l.saturation = vitals.saturation();
        l.exhaustion = vitals.exhaustion();
        l.foodTimer = vitals.foodTimer();
        l.air = vitals.air();
        l.xpLevel = vitals.xpLevel();
        l.xpProgress = vitals.xpProgress();
        l.xpTotal = vitals.xpTotal();
        l.xpSeed = int32_t(uint32_t(vitals.enchantSeed()));
        l.timeSinceRest = vitals.timeSinceRest();
        l.wardenLevel = vitals.wardenLevel();
        l.wardenTicks = vitals.wardenTicks();
        l.wardenCooldown = vitals.wardenCooldown();
        for (const auto& e : vitals.effects())
            if (e.duration > 0)
                l.effects.push_back(
                    {std::string(mc::world::effectInfo(e.type).id), e.amplifier, e.duration});
        l.hasRespawn = bedSpawn.has_value();
        l.hasLastDeath = lastDeath.has_value();
        if (lastDeath) {
            l.lastDeath[0] = lastDeath->first.x, l.lastDeath[1] = lastDeath->first.y,
            l.lastDeath[2] = lastDeath->first.z;
            l.lastDeathDimension = int(lastDeath->second);
        }
        if (bedSpawn)
            l.respawn[0] = bedSpawn->x, l.respawn[1] = bedSpawn->y, l.respawn[2] = bedSpawn->z;
        l.respawnDimension = int(spawnDim);
        l.fire = vitals.fireTicks();
        auto saveSlot = [&](int slot, const mc::world::ItemStack& s) {
            if (s.empty()) return;
            mc::world::LevelData::SavedItem it{slot, mc::world::itemRegistry().item(s.item).id,
                                               s.state &&
                                                       mc::world::itemRegistry().item(s.item).block
                                                   ? mc::world::blockRegistry().toString(s.state)
                                                   : std::string(),
                                               s.count, s.damage};
            for (const uint16_t v : s.enchantments)
                if (v)
                    it.enchantments.emplace_back(
                        std::string(mc::world::enchantmentInfo(mc::world::Enchantment(v >> 8)).id),
                        int(v & 0xFF));
            it.repairCost = s.repairCost;
            it.name = std::string(mc::world::nameText(s.name)); // (M29.3b)
            if (s.potion)
                it.potion =
                    std::string(mc::world::potionInfo(static_cast<mc::world::Potion>(s.potion)).id);
            it.storedEnchantments = it.id == "minecraft:enchanted_book";
            it.contents = s.contents;
            it.trim = s.trim;
            it.extra = s.extra;
            if (!mc::world::itemRegistry().item(s.item).block) it.itemState = s.state;
            l.inventory.push_back(std::move(it));
        };
        for (int i = 0; i < mc::Inventory::kSlots; ++i)
            saveSlot(i, inventory.slot(i));
        for (int piece = 0; piece < 4; ++piece) // head = 103 .. feet = 100
            saveSlot(103 - piece, inventory.armor(piece));
        saveSlot(150, inventory.offhand());
        for (int i = 0; i < 27; ++i) // the ender chest (M23.6)
            saveSlot(200 + i, inventory.enderChest().items[size_t(i)]);
        l.selectedSlot = inventory.selected();
        if (!l.save(worldDir)) MC_LOG_ERROR("Failed to write level.dat");
        if (!stats.save(mc::world::Statistics::file(worldDir, playerUuidHi, playerUuidLo)))
            MC_LOG_ERROR("Failed to write the statistics");
        if (!adv.save(mc::world::Advancements::file(worldDir, playerUuidHi, playerUuidLo)))
            MC_LOG_ERROR("Failed to write the advancements");
        if (!maps.save(worldDir)) MC_LOG_ERROR("Failed to write the maps");
        if (wait) storage->flush();
        MC_LOG_INFO("Saved world \"%s\" (%d changed chunks)", worldName.c_str(), chunks);
    };
    if (storage && !level) saveWorld(false); // a new world gets its level.dat at once
    mc::BlockInteraction interaction;
    mc::world::BlockUpdates blockUpdates(world); // block updates, scheduled ticks, redstone (M11)
    mc::setHopperBlockUpdates(&blockUpdates);    // (composters, M23.5)
    struct HopperUpdatesReset { // (cleared when the session ends: no dangling pointer)
        ~HopperUpdatesReset() { mc::setHopperBlockUpdates(nullptr); }
    } hopperUpdatesReset;
    interaction.setBlockUpdates(&blockUpdates);
    std::vector<mc::world::BlockPos> changedBlocks;
    changedBlocks.reserve(8);
    bool attackArmed = false; // the click that captures the mouse must not break a block
    std::optional<mc::world::RayHit> lastHit; // outline target of the last frame
    mc::GameClock clock;
    double last = mc::timeSeconds();
    int frame = 0;
    int exitCode = 0;
    mc::FrameStats frameStats;
    mc::FrameStats workStats; // CPU time per frame before the swap (excludes vsync/cap)
    const double startTime = last;
    bool meshed = false;
    int fps = 0, fpsFrames = 0;
    std::vector<std::string> stateNames(mc::world::blockRegistry().stateCount()); // F3 names
    double fpsStart = startTime;

    // Chat lines: commands run through gameplay/Commands, plain text is echoed.
    auto runChatLine = [&](std::string_view text) {
        if (text.empty()) return;
        if (text.front() == '/') {
            mc::CommandContext ctx{player,   inventory,   dayTime,  gameTime,
                                   seed,     &survival,   &vitals,  &world,
                                   &gameRng, &frameEdits, &weather, &commandBolts};
            ctx.rules = &rules;
            ctx.difficulty = &difficulty;
            ctx.gameMode = &gameMode;
            const auto result = mc::runCommand(text, ctx);
            if (!result.message.empty())
                chat.addMessage(result.message, result.ok ? 0xFFFFFFFFu : mc::gfx::argb(0xFFFF5555),
                                gameTime, gui.batch());
        } else {
            char line[mc::ui::Chat::kMaxInput + 16];
            std::snprintf(line, sizeof(line), "<Player> %.*s", int(text.size()), text.data());
            chat.addMessage(line, 0xFFFFFFFFu, gameTime, gui.batch());
        }
    };
    std::vector<std::string> pendingChat(opts->commands.begin(), opts->commands.end());

    // The open chest screen's storage, looked up again each frame (chunks may reload);
    // a broken chest closes the screen.
    // Hoppers and dispensers/droppers (M21.3): their slots at a block (empty: none).
    auto storeAt = [&](const mc::world::BlockPos& b) -> std::span<mc::world::ItemStack> {
        mc::world::Chunk* c = world.chunk(b.chunk());
        if (!c) return {};
        const int x = mc::world::blockToLocal(b.x), z = mc::world::blockToLocal(b.z);
        if (mc::world::HopperData* h = c->hopper(x, b.y, z)) return h->items;
        if (mc::world::DispenserData* d = c->dispenser(x, b.y, z)) return d->items;
        return {};
    };
    auto pointStore = [&] { // the open screen follows its block (closed if broken)
        const auto slots = storeAt(containerBlock);
        container.setStore(slots);
        if (slots.empty()) {
            screenDrops.clear();
            container.close(inventory, screenDrops);
            pendingThrows.insert(pendingThrows.end(), screenDrops.begin(),
                                 screenDrops.end()); // (the cursor's stack)
            if (!screenshotMode) window.setCursorCaptured(true);
        }
    };
    auto openStoreAt = [&](const mc::world::BlockPos& b) {
        const auto block = mc::world::blockRegistry().blockOf(world.getBlock(b));
        const auto slots = storeAt(b);
        if (slots.empty()) return false;
        containerBlock = b;
        container.openStore(block == mc::world::blocks::Hopper
                                ? mc::ui::ContainerScreen::Type::Hopper
                                : mc::ui::ContainerScreen::Type::Dispenser,
                            slots, block == mc::world::blocks::Dropper);
        return true;
    };
    auto pointChests = [&] {
        auto chestAt = [&](const mc::world::BlockPos& p) -> mc::world::ChestData* {
            mc::world::Chunk* c = world.chunk(p.chunk());
            return c ? c->chest(mc::world::blockToLocal(p.x), p.y, mc::world::blockToLocal(p.z))
                     : nullptr;
        };
        const bool ender = mc::world::blockRegistry().blockOf(world.getBlock(containerBlock)) ==
                           mc::world::blocks::EnderChest;
        mc::world::ChestData* first = ender ? &inventory.enderChest() : chestAt(containerBlock);
        mc::world::ChestData* second = chestSecond ? chestAt(*chestSecond) : nullptr;
        if (!first || (chestSecond && !second)) {
            screenDrops.clear();
            container.close(inventory, screenDrops);
            pendingThrows.insert(pendingThrows.end(), screenDrops.begin(),
                                 screenDrops.end()); // (spawned in the tick)
            if (!screenshotMode) window.setCursorCaptured(true);
            return;
        }
        container.setChests(first, second);
    };
    // An open trading screen follows its villager (mobs move in memory); it closes if
    // the villager is gone or more than 8 blocks away (M24.2).
    uint64_t traderUuid = 0; // the villager whose trading screen is open (M24.2)
    auto pointTrader = [&] {
        mc::world::MobData* trader = nullptr;
        const mc::world::ChunkPos pc{mc::world::blockToChunk(int(std::floor(player.position().x))),
                                     mc::world::blockToChunk(int(std::floor(player.position().z)))};
        for (int dz = -1; dz <= 1 && !trader; ++dz)
            for (int dx = -1; dx <= 1 && !trader; ++dx)
                if (mc::world::Chunk* tc = world.chunk({pc.x + dx, pc.z + dz}))
                    for (auto& mob : tc->mobs())
                        if (mob.uuidHi == traderUuid && mob.health > 0.0f &&
                            (mob.type == mc::world::MobType::Villager ||
                             mob.type == mc::world::MobType::WanderingTrader)) // (not once infected
                                                                               // or a witch)
                            trader = &mob;
        if (trader && glm::length(trader->pos - player.position()) > 8.0) trader = nullptr;
        container.setTrader(trader);
        container.setHeroLevel(vitals.effectLevel(mc::world::Effect::HeroOfTheVillage));
        if (trader) {
            trader->tradingTicks = 5;
        } else {
            screenDrops.clear();
            container.close(inventory, screenDrops);
            pendingThrows.insert(pendingThrows.end(), screenDrops.begin(), screenDrops.end());
            if (!screenshotMode) window.setCursorCaptured(true);
        }
    };
    // A mount's screen (M26.2) follows its mob the same way; its chest's slots live in
    // the mob's chunk (created on first use).
    uint64_t mountUuid = 0;
    auto pointMount = [&] {
        mc::world::MobData* mount = nullptr;
        mc::world::Chunk* in = nullptr;
        const mc::world::ChunkPos pc{mc::world::blockToChunk(int(std::floor(player.position().x))),
                                     mc::world::blockToChunk(int(std::floor(player.position().z)))};
        for (int dz = -1; dz <= 1 && !mount; ++dz)
            for (int dx = -1; dx <= 1 && !mount; ++dx)
                if (mc::world::Chunk* tc = world.chunk({pc.x + dx, pc.z + dz}))
                    for (auto& mob : tc->mobs())
                        if (mob.uuidHi == mountUuid && mob.health > 0.0f) {
                            mount = &mob;
                            in = tc;
                        }
        if (mount && glm::length(mount->pos - player.position()) > 8.0) mount = nullptr;
        if (mount) {
            std::span<mc::world::ItemStack> chest;
            if (mount->hasChest) {
                mc::world::ItemContents& slots = in->addMobStore(mount->uuidHi);
                chest = std::span(slots.data(),
                                  size_t(mc::world::chestSlots(mount->type, mount->strength)));
                in->markDirty();
            }
            container.setMount(mount, chest);
        } else {
            screenDrops.clear();
            container.close(inventory, screenDrops);
            pendingThrows.insert(pendingThrows.end(), screenDrops.begin(), screenDrops.end());
            if (!screenshotMode) window.setCursorCaptured(true);
        }
    };
    auto openMountScreen = [&](uint64_t uuid) {
        mountUuid = uuid;
        container.openMount(nullptr, {});
        pointMount();
        window.setCursorCaptured(false);
    };
    // Opens a chest's screen unless a solid block sits on it (wiki: Chest); a double
    // chest shows its "left" half first.
    auto openChestAt = [&](const mc::world::BlockPos& p) {
        const auto& creg = mc::world::blockRegistry();
        const mc::world::BlockId ob = creg.blockOf(world.getBlock(p));
        // An ender chest won't open under a solid block (like a chest); a shulker box
        // needs room for its lid: the near half of the block it faces must be free of
        // collision boxes (a top slab above is fine) (wiki: Ender Chest, Shulker Box).
        if (ob == mc::world::blocks::EnderChest &&
            creg.opaqueCube(world.getBlock({p.x, p.y + 1, p.z})))
            return false;
        if (creg.likeOf(ob) == mc::world::blocks::ShulkerBox) {
            const auto face = static_cast<mc::world::Direction>(
                creg.get(world.getBlock(p), mc::world::properties::facing6));
            const glm::ivec3 n = mc::world::normal(face);
            const mc::world::BlockStateId front = world.getBlock({p.x + n.x, p.y + n.y, p.z + n.z});
            if (creg.collides(front)) {
                const auto& shape = mc::world::collisionShape(front);
                bool blocked = shape.count == 0; // (a full cube)
                for (int i = 0; i < shape.count && !blocked; ++i) {
                    const auto& bx = shape.boxes[size_t(i)];
                    const int axis = n.x   ? 0
                                     : n.y ? 1
                                           : 2; // the lid needs [0, 8) on that axis, from our side
                    const int lo = (n.x + n.y + n.z) > 0 ? 0 : 8, hi = lo + 8;
                    blocked = bx.from[axis] < hi && bx.to[axis] > lo;
                }
                if (blocked) return false;
            }
        }
        if (ob == mc::world::blocks::Barrel || ob == mc::world::blocks::EnderChest ||
            creg.likeOf(ob) == mc::world::blocks::ShulkerBox) { // (M23.5-6: single chests)
            containerBlock = p;
            chestSecond.reset();
            container.openChest(nullptr, nullptr);
            pointChests();
            playSound(mc::world::Sound::ChestOpen, {p.x + 0.5, p.y + 0.5, p.z + 0.5}, 1.0f, 1.0f,
                      true);
            return true;
        }
        if (creg.likeOf(creg.blockOf(world.getBlock(p))) != mc::world::blocks::Chest)
            return false; // (copper chests too)
        const auto partner = mc::world::BlockUpdates::chestPartner(world, p);
        const bool blocked =
            creg.opaqueCube(world.getBlock({p.x, p.y + 1, p.z})) ||
            (partner && creg.opaqueCube(world.getBlock({partner->x, partner->y + 1, partner->z})));
        if (blocked) return false;
        const bool leftFirst = creg.value(world.getBlock(p), "type") != "right";
        containerBlock = partner && !leftFirst ? *partner : p;
        chestSecond = partner ? std::optional(leftFirst ? *partner : p) : std::nullopt;
        container.openChest(nullptr, nullptr);
        pointChests();
        playSound(mc::world::Sound::ChestOpen, {p.x + 0.5, p.y + 0.5, p.z + 0.5}, 1.0f, 1.0f, true);
        stats.add(
            mc::world::Stat::OpenChest); // (chests and copper chests; vanilla counts barrels apart)
        return true;
    };
    bool openBlockPending = opts->hasOpenBlock;
    bool spyglassUp = false; // (M29.3c: looking through a spyglass)
    bool tradePending = opts->trade;
    bool bookPending = opts->book; // (M28.2c)
    int scriptedUses = opts->use;
    int64_t scriptedUseAt = (level ? level->gameTime : 0) + 5;
    bool mountPending = opts->mount;
    std::optional<mc::world::BlockPos> openBarrel; // the barrel drawn open (its screen is up)
    std::optional<mc::world::BlockPos> openTrapped; // (M29.5) the trapped chest giving power
    glm::vec3 netherFog(0x33 / 255.0f, 0x08 / 255.0f, 0x08 / 255.0f); // (eased toward the biome's)
    // The rain/snow columns around the camera (ground, kind, light), refilled once a
    // tick or when the camera's block changes - not every frame.
    struct RainColumn {
        int ground;
        mc::world::Precipitation kind;
        uint8_t light; // sky x 16 + block
    };
    constexpr int kRainRadius = 10;
    std::array<RainColumn, (2 * kRainRadius + 1) * (2 * kRainRadius + 1)> rainColumns{};
    int64_t rainColumnsTick = -1;
    int rainCx = INT32_MIN, rainCy = 0, rainCz = 0;
    // Settings that touch this world (M22.5): view and simulation distance, clouds.
    auto applySessionOptions = [&] {
        const mc::GameOptions& o = shared.options;
        const int distance =
            shared.cliRenderDistance > 0 && o.renderDistance == shared.optionsRenderDistance
                ? shared.cliRenderDistance
                : o.renderDistance;
        if (loader && distance != opts->renderDistance) {
            opts->renderDistance = distance;
            loader->setRenderDistance(distance);
            renderer.setRenderDistance(distance);
        }
        mobs.setSimulationDistance(o.simulationDistance);
        renderer.setClouds(o.clouds);
    };
    applySessionOptions();
    applyGlobalOptions(shared);
    if (!screenshotMode && !opts->hidden) window.setCursorCaptured(true); // (into the game)
    while (!window.shouldClose()) {
        window.pollEvents();
        // The Game Menu (Esc) pauses the game: no ticks, the menu takes the input.
        const bool paused = shared.menuState.screen != mc::ui::MenuScreen::None;
        // An open chest screen follows its block(s) (closed if broken).
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Chest)
            pointChests();
        if (container.isOpen() && (container.type() == mc::ui::ContainerScreen::Type::Hopper ||
                                   container.type() == mc::ui::ContainerScreen::Type::Dispenser))
            pointStore();
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Trading)
            pointTrader();
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Mount)
            pointMount();
        // An open beacon screen follows its block (closed if it was broken) (M23.6).
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Beacon) {
            mc::world::Chunk* c = world.chunk(containerBlock.chunk());
            mc::world::BeaconData* bd =
                c ? c->beacon(mc::world::blockToLocal(containerBlock.x), containerBlock.y,
                              mc::world::blockToLocal(containerBlock.z))
                  : nullptr;
            container.setBeacon(bd);
            if (!bd) {
                screenDrops.clear();
                container.close(inventory, screenDrops);
                pendingThrows.insert(pendingThrows.end(), screenDrops.begin(), screenDrops.end());
                if (!screenshotMode) window.setCursorCaptured(true);
            }
        }
        // An open brewing stand screen follows its block (closed if it was broken).
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Brewing) {
            mc::world::Chunk* c = world.chunk(containerBlock.chunk());
            mc::world::BrewingData* b =
                c ? c->brewing(mc::world::blockToLocal(containerBlock.x), containerBlock.y,
                               mc::world::blockToLocal(containerBlock.z))
                  : nullptr;
            container.setBrewing(b);
            if (!b) {
                screenDrops.clear();
                container.close(inventory, screenDrops);
                pendingThrows.insert(pendingThrows.end(), screenDrops.begin(), screenDrops.end());
                if (!screenshotMode) window.setCursorCaptured(true);
            }
        }
        // The open furnace screen follows its block (closed if it was broken).
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Furnace) {
            mc::world::Chunk* c = world.chunk(containerBlock.chunk());
            mc::world::FurnaceData* f =
                c ? c->furnace(mc::world::blockToLocal(containerBlock.x), containerBlock.y,
                               mc::world::blockToLocal(containerBlock.z))
                  : nullptr;
            if (f)
                container.setFurnace(f);
            else {
                screenDrops.clear();
                container.close(inventory, screenDrops);
                if (!screenshotMode) window.setCursorCaptured(true);
            }
        }
        // Text typed this frame (only the chat consumes it).
        const int typedCount =
            paused ? 0 : window.takeText(typed.data(), static_cast<int>(typed.size()));
        if (paused) {
            // (the menu reads clicks, Enter, Esc and text when it is drawn; game keys
            // pressed meanwhile must not act after resuming)
            for (const mc::Press p : {mc::Press::Chat, mc::Press::Command, mc::Press::Inventory,
                                      mc::Press::F3, mc::Press::Up, mc::Press::Down,
                                      mc::Press::Drop, mc::Press::Jump, mc::Press::RightMouse})
                window.takePresses(p);
        } else if (chat.isOpen()) {
            chat.type({typed.data(), size_t(typedCount)}); // backspaces included, in order
            window.takePresses(mc::Press::Backspace);
            for (int n = window.takePresses(mc::Press::Up); n > 0; --n)
                chat.browseSent(-1);
            for (int n = window.takePresses(mc::Press::Down); n > 0; --n)
                chat.browseSent(1);
            if (window.takePresses(mc::Press::Enter) > 0) {
                // Commands run on the next tick (game state changes only in ticks).
                const auto line = chat.submit();
                if (!line.empty()) pendingChat.emplace_back(line);
                window.setCursorCaptured(true);
                attackArmed = false;
            } else if (window.takePresses(mc::Press::Escape) > 0) {
                chat.close();
                window.setCursorCaptured(true);
                attackArmed = false;
            }
            for (auto p : {mc::Press::Chat, mc::Press::Command, mc::Press::F3, mc::Press::Inventory,
                           mc::Press::LeftMouse, mc::Press::RightMouse})
                window.takePresses(p); // typing, not game keys
        } else if (bookScreen.isOpen()) {
            // A book (M28.2c): typing goes to the page (or the title); Esc or Done keeps the
            // pages in the book and quill, "Sign and Close" turns it into a written book.
            bookScreen.type({typed.data(), size_t(typedCount)}, gui.batch().font());
            window.takePresses(mc::Press::Backspace);
            for (int n = window.takePresses(mc::Press::Enter); n > 0; --n)
                bookScreen.newline(gui.batch().font());
            bookClick = window.takePresses(mc::Press::LeftMouse) > 0;
            const bool esc = window.takePresses(mc::Press::Escape) > 0;
            if (esc || bookAction != mc::ui::BookScreen::Action::None) {
                const bool signedNow = bookAction == mc::ui::BookScreen::Action::Signed;
                if (bookScreen.editing() && bookScreen.slot() >= 0) {
                    static const mc::world::ItemId writable =
                        *mc::world::itemRegistry().find("writable_book");
                    static const mc::world::ItemId written =
                        *mc::world::itemRegistry().find("written_book");
                    mc::world::ItemStack s = inventory.slot(bookScreen.slot());
                    if (s.item == writable) {
                        mc::world::BookContent content = bookScreen.content();
                        if (signedNow) {
                            s.item = written;
                            content.author = "Player";
                            content.generation = 0;
                        } else {
                            content.title.clear();
                        }
                        s.extra = mc::world::addBook(std::move(content));
                        inventory.setSlot(bookScreen.slot(), s);
                    }
                }
                bookScreen.close();
                bookAction = mc::ui::BookScreen::Action::None;
                window.setCursorCaptured(true);
                attackArmed = false;
            }
            for (auto p : {mc::Press::Chat, mc::Press::Command, mc::Press::F3, mc::Press::Inventory,
                           mc::Press::RightMouse, mc::Press::Up, mc::Press::Down})
                window.takePresses(p);
        } else if (signEditor.isOpen()) {
            // Sign editing (M23.3c): typing goes to the active line; Done or Esc saves.
            signEditor.type({typed.data(), size_t(typedCount)}, gui.batch().font());
            window.takePresses(mc::Press::Backspace);
            for (int n = window.takePresses(mc::Press::Up); n > 0; --n)
                signEditor.previousLine();
            for (int n = window.takePresses(mc::Press::Down) + window.takePresses(mc::Press::Enter);
                 n > 0; --n)
                signEditor.nextLine();
            signClick = window.takePresses(mc::Press::LeftMouse) > 0;
            if (window.takePresses(mc::Press::Escape) > 0 || signDone) {
                const mc::world::BlockPos sp = signEditor.pos();
                if (mc::world::Chunk* sc = world.chunk(sp.chunk()))
                    if (mc::world::SignData* sd = sc->sign(mc::world::blockToLocal(sp.x), sp.y,
                                                           mc::world::blockToLocal(sp.z))) {
                        sd->front.lines = signEditor.text().lines;
                        sc->markDirty();
                    }
                signEditor.close();
                signDone = false;
                window.setCursorCaptured(true);
                attackArmed = false;
            }
            for (auto p : {mc::Press::Chat, mc::Press::Command, mc::Press::F3, mc::Press::Inventory,
                           mc::Press::RightMouse})
                window.takePresses(p);
        } else if (container.isOpen()) {
            container.setPlayer(vitals.xpLevel(), !survival, vitals.enchantSeed());
            int fw = 0, fh = 0;
            window.framebufferSize(fw, fh);
            const int scale = mc::gfx::GuiRenderer::guiScale(fw, fh);
            double mx = 0, my = 0;
            window.cursorPos(mx, my);
            mx /= scale;
            my /= scale;
            const bool shift = window.keyDown(mc::Key::LeftShift);
            if (container.type() == mc::ui::ContainerScreen::Type::Anvil) { // (M29.3b) the name field takes typing
                container.typeAnvilName({typed.data(), size_t(typedCount)});
                window.takePresses(mc::Press::Backspace);
                window.takePresses(mc::Press::Inventory); // (E types an "e" here, as vanilla)
            }
            screenDrops.clear();
            for (int n = window.takePresses(mc::Press::LeftMouse); n > 0; --n)
                container.click(mx, my, mc::ui::ContainerScreen::Button::Left, shift, fw / scale,
                                fh / scale, inventory, screenDrops);
            for (int n = window.takePresses(mc::Press::RightMouse); n > 0; --n)
                container.click(mx, my, mc::ui::ContainerScreen::Button::Right, shift, fw / scale,
                                fh / scale, inventory, screenDrops);
            // Contents edited through the screen: the block entity's chunk needs saving.
            if (container.type() == mc::ui::ContainerScreen::Type::Chest ||
                container.type() == mc::ui::ContainerScreen::Type::Furnace ||
                container.type() == mc::ui::ContainerScreen::Type::Brewing ||
                container.type() == mc::ui::ContainerScreen::Type::Hopper ||
                container.type() == mc::ui::ContainerScreen::Type::Dispenser) {
                if (mc::world::Chunk* c = world.chunk(containerBlock.chunk())) c->markDirty();
                if (chestSecond)
                    if (mc::world::Chunk* c = world.chunk(chestSecond->chunk())) c->markDirty();
            }
            if (window.takePresses(mc::Press::Escape) > 0 ||
                window.takePresses(mc::Press::Inventory) > 0) {
                container.close(inventory, screenDrops);
                if (!screenshotMode) window.setCursorCaptured(true);
                attackArmed = false;
            }
            pendingThrows.insert(pendingThrows.end(), screenDrops.begin(),
                                 screenDrops.end()); // next tick
            for (auto p : {mc::Press::Chat, mc::Press::Command, mc::Press::F3, mc::Press::Backspace,
                           mc::Press::Up, mc::Press::Down, mc::Press::Enter, mc::Press::Drop})
                window.takePresses(p);
        } else if (creative.isOpen()) {
            int fw = 0, fh = 0;
            window.framebufferSize(fw, fh);
            const int scale = mc::gfx::GuiRenderer::guiScale(fw, fh);
            double mx = 0, my = 0;
            window.cursorPos(mx, my);
            mx /= scale;
            my /= scale;
            for (int n = window.takePresses(mc::Press::LeftMouse); n > 0; --n)
                creative.click(mx, my, fw / scale, fh / scale, inventory);
            creative.scroll(window.scrollDelta());
            for (int i = 0; i < mc::Inventory::kHotbar; ++i) {
                const bool down =
                    window.keyDown(static_cast<mc::Key>(static_cast<int>(mc::Key::Num1) + i));
                if (down && !numberWasDown[i])
                    creative.numberKey(i, mx, my, fw / scale, fh / scale, inventory);
            }
            if (window.takePresses(mc::Press::Escape) > 0 ||
                window.takePresses(mc::Press::Inventory) > 0) {
                creative.close();
                if (!screenshotMode) window.setCursorCaptured(true);
                attackArmed = false;
            }
            for (auto p : {mc::Press::Chat, mc::Press::Command, mc::Press::F3,
                           mc::Press::RightMouse, mc::Press::Backspace, mc::Press::Up,
                           mc::Press::Down, mc::Press::Enter, mc::Press::Drop})
                window.takePresses(p);
        } else {
            // Respawn at the bed or world spawn (game rule immediate_respawn: no death screen).
            if (dead && (rules.immediateRespawn || window.takePresses(mc::Press::Enter) > 0)) {
                dead = false;
                vitals.reset(rules.keepInventory);
                if (bedSpawn && dimension == spawnDim) {
                    player.setPosition({bedSpawn->x + 0.5, bedSpawn->y + 1.0, bedSpawn->z + 0.5});
                    player.setVelocity(glm::dvec3(0.0));
                    bedRespawnPending = true; // checked once its chunks are loaded
                } else if (bedSpawn) { // (M29.5) an anchor in the Nether, a bed in the Overworld
                    pendingTravel = Travel{spawnDim, Travel::Via::Respawn, {}};
                } else if (dimension != Dimension::Overworld) {
                    pendingTravel = Travel{Dimension::Overworld,
                                           Travel::Via::Respawn,
                                           {}}; // spawn is in the Overworld
                } else {
                    player.setPosition(
                        glm::dvec3(worldSpawn[0] + 0.5, worldSpawn[1], worldSpawn[2] + 0.5));
                    spawnPending = !flatWorld; // settle on the ground there again
                    if (spawnPending)
                        spawn = glm::dvec3(worldSpawn[0] + 0.5, worldSpawn[1], worldSpawn[2] + 0.5);
                }
            }
            for (auto p : {mc::Press::Backspace, mc::Press::Up, mc::Press::Down, mc::Press::Enter})
                window.takePresses(p);
            if (window.takePresses(mc::Press::F3) > 0) showDebug = !showDebug;
            if (window.takePresses(mc::Press::Perspective) > 0) perspective = (perspective + 1) % 3;
            if (window.cursorCaptured()) {
                // T opens the chat, / opens it with the slash typed (vanilla).
                if (window.takePresses(mc::Press::Command) > 0) {
                    chat.open("/");
                    window.setCursorCaptured(false);
                } else if (window.takePresses(mc::Press::Chat) > 0) {
                    chat.open();
                    window.setCursorCaptured(false);
                } else if (window.takePresses(mc::Press::Inventory) > 0) {
                    // (M26.2) on a tamed mount or in a chest boat: its screen
                    const mc::world::MobData* ride = ridingCart ? findCart() : nullptr;
                    if (ride && ((mc::world::isMount(ride->type) &&
                                  (ride->tamed || mc::world::isCamel(ride->type))) ||
                                 (ride->type == mc::world::MobType::Boat && ride->hasChest)))
                        openMountScreen(ride->uuidHi);
                    else if (gameMode == 3)
                        ; // (spectators have no inventory)
                    else if (survival)
                        container.open(mc::ui::ContainerScreen::Type::Inventory);
                    else
                        creative.open();
                    window.setCursorCaptured(false);
                } else if (lastHit && !window.keyDown(mc::Key::LeftShift) &&
                           window.takePresses(mc::Press::RightMouse) > 0) {
                    // Using a workstation opens its screen (sneaking places against it).
                    const auto& reg = mc::world::blockRegistry();
                    const auto block = reg.blockOf(world.getBlock(lastHit->block));
                    if (block == mc::world::blocks::CraftingTable) {
                        stats.add(mc::world::Stat::InteractWithCraftingTable);
                        container.open(mc::ui::ContainerScreen::Type::Crafting);
                        window.setCursorCaptured(false);
                    } else if (reg.likeOf(block) ==
                               mc::world::blocks::Furnace) { // (smokers, blast furnaces)
                        containerBlock = lastHit->block;
                        stats.add(mc::world::Stat::InteractWithFurnace);
                        container.open(mc::ui::ContainerScreen::Type::Furnace);
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::Beacon) { // (M23.6)
                        containerBlock = lastHit->block;
                        mc::world::Chunk* bc = world.chunk(containerBlock.chunk());
                        container.openBeacon(
                            bc ? bc->beacon(mc::world::blockToLocal(containerBlock.x),
                                            containerBlock.y,
                                            mc::world::blockToLocal(containerBlock.z))
                               : nullptr);
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::BrewingStand) {
                        containerBlock = lastHit->block;
                        mc::world::Chunk* bc = world.chunk(containerBlock.chunk());
                        container.openBrewing(
                            bc ? bc->brewing(mc::world::blockToLocal(containerBlock.x),
                                             containerBlock.y,
                                             mc::world::blockToLocal(containerBlock.z))
                               : nullptr);
                        window.setCursorCaptured(false);
                    } else if ((block == mc::world::blocks::Hopper ||
                                block == mc::world::blocks::Dispenser ||
                                block == mc::world::blocks::Dropper || block == mc::world::blocks::Crafter) &&
                               openStoreAt(lastHit->block)) {
                        window.setCursorCaptured(false);
                    } else if (reg.likeOf(block) == mc::world::blocks::RedBed) { // (any colour)
                        pendingBedUse =
                            lastHit->block; // used in the next tick (simulation stays in ticks)
                    } else if (block == mc::world::blocks::Stonecutter ||
                               block == mc::world::blocks::Grindstone ||
                               block == mc::world::blocks::SmithingTable ||
                               block == mc::world::blocks::Loom ||
                               block == mc::world::blocks::CartographyTable) {
                        using T = mc::ui::ContainerScreen::Type; // (M23.5-6 workstations)
                        containerBlock = lastHit->block;
                        container.open(block == mc::world::blocks::Stonecutter     ? T::Stonecutter
                                       : block == mc::world::blocks::Grindstone    ? T::Grindstone
                                       : block == mc::world::blocks::SmithingTable ? T::Smithing
                                       : block == mc::world::blocks::Loom ? T::Loom
                                                                          : T::Cartography);
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::EnchantingTable) {
                        containerBlock = lastHit->block;
                        container.openEnchanting(mc::countBookshelves(world, lastHit->block),
                                                 vitals.enchantSeed());
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::Anvil ||
                               block == mc::world::blocks::ChippedAnvil ||
                               block == mc::world::blocks::DamagedAnvil) {
                        containerBlock = lastHit->block;
                        container.openAnvil();
                        window.setCursorCaptured(false);
                    } else if ((block == mc::world::blocks::CommandBlock || block == mc::world::blocks::ChainCommandBlock ||
                                block == mc::world::blocks::RepeatingCommandBlock) &&
                               gameMode == 1) { // (M29.7) its settings (creative only, as vanilla's operators)
                        mc::world::Chunk* cc = world.chunk(lastHit->block.chunk());
                        if (const mc::world::CommandBlockData* d =
                                cc ? cc->commandBlock(mc::world::blockToLocal(lastHit->block.x), lastHit->block.y,
                                                      mc::world::blockToLocal(lastHit->block.z))
                                   : nullptr) {
                            auto& ms = shared.menuState;
                            ms.command = d->command;
                            ms.commandOutput = d->lastOutput;
                            ms.commandAlways = d->autoActive;
                            ms.commandMode = block == mc::world::blocks::ChainCommandBlock       ? 1
                                             : block == mc::world::blocks::RepeatingCommandBlock ? 2
                                                                                                 : 0;
                            ms.commandConditional = reg.get(world.getBlock(lastHit->block),
                                                            mc::world::properties::conditional) == 0;
                            ms.screen = mc::ui::MenuScreen::CommandBlock;
                            commandEditing = lastHit->block;
                            commandEditingCart = 0;
                            window.setCursorCaptured(false);
                        }
                    } else if (reg.likeOf(block) == mc::world::blocks::Chest ||
                               block == mc::world::blocks::Barrel ||
                               block == mc::world::blocks::EnderChest ||
                               reg.likeOf(block) == mc::world::blocks::ShulkerBox) {
                        if (openChestAt(lastHit->block)) window.setCursorCaptured(false);
                    } else {
                        window.addPress(mc::Press::RightMouse); // not a workstation: a normal use
                    }
                }
            } else {
                window.takePresses(mc::Press::Chat);
                window.takePresses(mc::Press::Command);
                window.takePresses(mc::Press::Inventory);
            }
            if (!screenshotMode) {
                // Click to capture the mouse, Esc to release it (pause menu: later).
                if (!window.cursorCaptured() && window.leftMousePressed()) {
                    window.setCursorCaptured(true);
                    attackArmed = false;
                    window.takePresses(mc::Press::LeftMouse); // the capturing click doesn't act
                }
                if (window.takePresses(mc::Press::Escape) >
                    0) { // the Game Menu (vanilla pauses and saves)
                    window.setCursorCaptured(false);
                    blockUpdates.landAll();
                    saveWorld(false);
                    shared.menuState.screen = mc::ui::MenuScreen::Pause;
                }
            }
        }
        // Key edges for the creative's number keys: tracked every frame, so a key
        // held while the screen opens doesn't count as a fresh press.
        for (int i = 0; i < mc::Inventory::kHotbar; ++i)
            numberWasDown[i] =
                window.keyDown(static_cast<mc::Key>(static_cast<int>(mc::Key::Num1) + i));
        // Game presses made while the mouse isn't captured (typing, menus) must not
        // act later (spaces typed in chat would toggle flight).
        if (!window.cursorCaptured()) {
            window.takePresses(mc::Press::Jump);
            window.takePresses(mc::Press::RightMouse);
            window.takePresses(mc::Press::Drop);
        }
        player.turn(window.mouseDx(), window.mouseDy(), shared.options.sensitivity);
        if (!window.leftMousePressed()) attackArmed = true;

        // Hotbar: number keys 1-9 and the mouse wheel.
        if (window.cursorCaptured()) {
            for (int i = 0; i < mc::Inventory::kHotbar; ++i) {
                if (window.keyDown(static_cast<mc::Key>(static_cast<int>(mc::Key::Num1) + i)))
                    inventory.select(i);
            }
            inventory.scroll(window.scrollDelta());
        }

        const double now = mc::timeSeconds();
        const double frameSeconds = now - last;
        // Full frame period (includes swap, i.e. waiting for the GPU / vsync).
        // Skips the first frame after meshing: it includes one-off driver warm-up
        // (first multi-draw), which is not a steady-state cost.
        if (frame > 1) frameStats.add((now - last) * 1000.0);
        clock.advance(paused ? 0.0 : now - last);
        last = now;
        for (int i = 0; i < clock.ticksDue; ++i) {
            // Changing dimension (M12): save, unload everything, switch the generator and
            // storage, then wait for the destination to load (vanilla keeps the others
            // loaded; one dimension at a time here).
            if (pendingTravel) {
                Travel t = *pendingTravel;
                pendingTravel.reset();
                t.fromDimension = dimension; // saved as the player's place until arrival
                t.fromPos = player.position();
                MC_LOG_INFO("Travelling to %s",
                            std::string(mc::world::dimensionInfo(t.to).id).c_str());
                blockUpdates.landAll(); // (blocks in flight land in this dimension first)
                saveWorld(false);
                loader.reset(); // joins its workers
                std::vector<mc::world::ChunkPos> all;
                world.forEachChunk([&](const mc::world::Chunk& c) { all.push_back(c.pos()); });
                for (const auto& p : all)
                    world.removeChunk(p);
                unloadedChunks.insert(unloadedChunks.end(), all.begin(), all.end());
                droppedItems.clear(); // (items stay behind in vanilla; ours are lost)
                fallingBlocks.clear();
                projectiles.clear();
                fishing.cancel();
                particles.clear();
                primedTnt.clear();
                blockUpdates.landAll();
                orbs.clear();
                const Dimension from = dimension;
                dimension = t.to;
                world.setHasSkyLight(mc::world::dimensionInfo(dimension).hasSkyLight);
                world.setHeight(
                    mc::world::dimensionInfo(dimension).height); // (no chunks are loaded now)
                world.setUltrawarm(dimension == Dimension::Nether);
                renderer.setDimension(dimension);
                vitals.setVoidY(mc::world::dimensionInfo(dimension).voidY);
                // The old storage finishes its writes on a thread of its own (flushing
                // hundreds of chunks would freeze the frame); a folder is reopened only
                // after its previous storage has finished.
                if (storage) {
                    retiringStorage.push_back(
                        {dimensionDir(from),
                         std::thread([old = std::move(storage)]() mutable { old.reset(); })});
                }
                if (!worldName.empty()) {
                    const auto dir = dimensionDir(dimension);
                    std::erase_if(retiringStorage, [&](RetiringStorage& r) {
                        if (r.dir != dir) return false;
                        r.thread.join();
                        return true;
                    });
                    storage = std::make_unique<mc::world::ChunkStorage>(dir, legacyWorld);
                }
                generatorPtr = makeGenerator(dimension);
                loader = std::make_unique<mc::world::ChunkLoader>(world, *generatorPtr, genThreads,
                                                                  storage.get());
                loader->setRenderDistance(opts->renderDistance);
                arrival = t;
                if (t.via == Travel::Via::NetherPortal)
                    arrival->from = mc::portals::destination(from, dimension, t.from);
                else if (t.via == Travel::Via::EndPortal)
                    arrival->from = {100, 49, 0};
                else
                    arrival->from = {worldSpawn[0], worldSpawn[1], worldSpawn[2]};
                // Wait at the destination; unloaded chunks hold the player up meanwhile.
                player.setPosition(
                    {arrival->from.x + 0.5, double(arrival->from.y), arrival->from.z + 0.5});
                player.setVelocity(glm::dvec3(0.0));
                vitals.resetFall(); // the wait doesn't count as a fall
                portalTicks = 0;
                portalCooldown = true;
            }
            if (arrival) {
                // Destination chunks within 2 of the target loaded: find or build the way in.
                const mc::world::ChunkPos c = arrival->from.chunk();
                bool ready = true;
                for (int dz = -2; dz <= 2 && ready; ++dz)
                    for (int dx = -2; dx <= 2 && ready; ++dx)
                        ready = world.chunk({c.x + dx, c.z + dz}) != nullptr;
                std::optional<mc::world::BlockPos> found;
                if (ready && arrival->via == Travel::Via::NetherPortal) {
                    found = mc::portals::find(world, knownPortals, dimension, arrival->from,
                                              dimension == Dimension::Nether ? 16 : 128);
                    // A remembered portal in a chunk that isn't loaded yet is checked once it
                    // is (it may be gone since), unless that takes too long.
                    if (found && !world.chunk(found->chunk())) {
                        if (++arrivalWait < 400)
                            ready = false;
                        else
                            found.reset();
                    }
                }
                if (ready) {
                    const Travel a = *arrival;
                    arrival.reset();
                    arrivalWait = 0;
                    if (a.via == Travel::Via::NetherPortal) {
                        mc::world::BlockPos in;
                        if (found) {
                            in = *found;
                            while (mc::world::blockRegistry().blockOf(world.getBlock(
                                       {in.x, in.y - 1, in.z})) == mc::world::blocks::NetherPortal)
                                --in.y;
                        } else {
                            const bool nether = dimension == Dimension::Nether;
                            in = mc::portals::build(
                                world, a.from, nether ? 32 : world.height().minY + 8,
                                // vanilla: up to 10 below the logical top
                                mc::world::dimensionInfo(dimension).logicalHeight +
                                    world.height().minY - 10,
                                frameEdits);
                            knownPortals.push_back({dimension, in});
                        }
                        player.setPosition({in.x + 0.5, double(in.y), in.z + 0.5});
                    } else if (a.via == Travel::Via::EndPortal) {
                        player.setPosition(mc::portals::endPlatform(world, frameEdits));
                        player.setRotation(
                            90.0f, 0.0f);  // facing west, toward the island (wiki: End Platform)
                    } else if (bedSpawn && dimension == spawnDim) { // back to the bed (or anchor - M29.5)
                        player.setPosition(
                            {bedSpawn->x + 0.5, bedSpawn->y + 1.0, bedSpawn->z + 0.5});
                        bedRespawnPending = true;
                    } else {
                        spawn = glm::dvec3(worldSpawn[0] + 0.5, worldSpawn[1], worldSpawn[2] + 0.5);
                        player.setPosition(spawn);
                        spawnPending = true; // settle on the ground there
                    }
                    player.setVelocity(glm::dvec3(0.0));
                    vitals.resetFall();
                }
            }
            if (bedRespawnPending && bedSpawn) { // the bed's chunks are in: stand next to it
                const mc::world::ChunkPos bc = bedSpawn->chunk();
                bool loaded = true;
                for (int dz = -1; dz <= 1 && loaded; ++dz)
                    for (int dx = -1; dx <= 1 && loaded; ++dx)
                        loaded = world.chunk({bc.x + dx, bc.z + dz}) != nullptr;
                if (loaded) {
                    bedRespawnPending = false;
                    const bool anchor = spawnDim == Dimension::Nether;
                    if (const auto spot = anchor ? mc::anchorStandSpot(world, *bedSpawn) : mc::bedStandSpot(world, *bedSpawn)) {
                        player.setPosition(*spot);
                        if (anchor) { // (M29.5; wiki: Respawn Anchor) each respawn uses a charge
                            const auto as = world.getBlock(*bedSpawn);
                            const auto& areg = mc::world::blockRegistry();
                            world.updateBlock(*bedSpawn, areg.set(as, mc::world::properties::charges,
                                                                  areg.get(as, mc::world::properties::charges) - 1));
                            frameEdits.push_back(*bedSpawn);
                        }
                    } else { // gone or blocked: the world spawn (wiki: Bed)
                        chat.addMessage(
                            "You have no home bed or charged respawn anchor, or it was obstructed",
                            0xFFFFFFFFu, gameTime, gui.batch());
                        bedSpawn.reset();
                        if (dimension != Dimension::Overworld) { // (M29.5: a spent anchor) the world spawn is home
                            pendingTravel = Travel{Dimension::Overworld, Travel::Via::Respawn, {}};
                        } else {
                            spawn = glm::dvec3(worldSpawn[0] + 0.5, worldSpawn[1], worldSpawn[2] + 0.5);
                            player.setPosition(spawn);
                            spawnPending = !flatWorld;
                        }
                    }
                    vitals.resetFall();
                } else {
                    player.setVelocity(glm::dvec3(0.0));
                }
            }
            blockUpdates.setTime(gameTime);
            blockUpdates.setCreative(!survival);
            // The command block screen's edit (M29.7; in the tick, M29 review): a cart's command,
            // or the block's mode, condition, command and Always Active.
            if (pendingCommandEdit) {
                const PendingCommandEdit ce = std::move(*pendingCommandEdit);
                pendingCommandEdit.reset();
                const auto& reg = mc::world::blockRegistry();
                if (ce.cart != 0) {
                    if (mc::world::MobData* cart = mc::Mobs::mobByUuid(world, player.position(), ce.cart))
                        cart->commandId = mc::world::addName(ce.command);
                } else if (const mc::world::BlockStateId old = world.getBlock(ce.pos);
                           reg.blockOf(old) == mc::world::blocks::CommandBlock ||
                           reg.blockOf(old) == mc::world::blocks::ChainCommandBlock ||
                           reg.blockOf(old) == mc::world::blocks::RepeatingCommandBlock) {
                    const mc::world::BlockId kind = ce.mode == 1   ? mc::world::blocks::ChainCommandBlock
                                                    : ce.mode == 2 ? mc::world::blocks::RepeatingCommandBlock
                                                                   : mc::world::blocks::CommandBlock;
                    mc::world::BlockStateId cmdState =
                        reg.set(reg.defaultState(kind), mc::world::properties::facing6, reg.get(old, mc::world::properties::facing6));
                    cmdState = reg.set(cmdState, mc::world::properties::conditional, ce.conditional ? 0 : 1);
                    if (cmdState != old) world.updateBlock(ce.pos, cmdState);
                    mc::world::Chunk* cc = world.chunk(ce.pos.chunk());
                    if (mc::world::CommandBlockData* d =
                            cc ? cc->commandBlock(mc::world::blockToLocal(ce.pos.x), ce.pos.y, mc::world::blockToLocal(ce.pos.z))
                               : nullptr) {
                        const bool wasAlways = d->autoActive;
                        d->command = ce.command;
                        d->autoActive = ce.always;
                        cc->markDirty();
                        // Always Active: an impulse block fires once; a repeating one starts
                        // whenever it is active (also one that was powered as an impulse block).
                        if ((d->autoActive && !wasAlways) ||
                            (kind == mc::world::blocks::RepeatingCommandBlock && (d->autoActive || d->powered)))
                            blockUpdates.armCommandBlock(ce.pos);
                    }
                    frameEdits.push_back(ce.pos);
                }
            }
            // A bed used this frame (M17.4): sleep, set the respawn point, or explode.
            if (pendingBedUse && !dead) {
                const mc::world::BlockPos bedPos = *pendingBedUse;
                pendingBedUse.reset();
                const glm::dvec3 feetNow = player.position();
                switch (mc::useBed(world, bedPos, dayTime, dimension, !survival, &feetNow,
                                   weather.raining, weather.raining && weather.thundering)) {
                case mc::BedUse::Sleep:
                    bedSpawn = *mc::bedHead(world, bedPos);
                    spawnDim = Dimension::Overworld;
                    sleepBed = *bedSpawn;
                    sleepTicks = 1;
                    stats.add(mc::world::Stat::SleepInBed);
                    announce(adv.onEvent(mc::world::AdvEvent::Slept));
                    chat.addMessage("Respawn point set", 0xFFFFFFFFu, gameTime, gui.batch());
                    break;
                case mc::BedUse::NotNight:
                    bedSpawn = *mc::bedHead(world, bedPos);
                    spawnDim = Dimension::Overworld;
                    chat.addMessage("Respawn point set", 0xFFFFFFFFu, gameTime, gui.batch());
                    chat.addMessage("You can sleep only at night", 0xFFFFFFFFu, gameTime,
                                    gui.batch());
                    break;
                case mc::BedUse::Monsters:
                    chat.addMessage("You may not rest now; there are monsters nearby", 0xFFFFFFFFu,
                                    gameTime, gui.batch());
                    break;
                case mc::BedUse::Occupied:
                    chat.addMessage("This bed is occupied", 0xFFFFFFFFu, gameTime, gui.batch());
                    break;
                case mc::BedUse::Explodes: { // wiki: Bed - power 5 outside the Overworld
                    const mc::world::BlockPos head = *mc::bedHead(world, bedPos);
                    world.updateBlock(head, 0); // (the foot follows)
                    frameEdits.push_back(head);
                    mc::ExplosionTargets t;
                    t.tnt = &primedTnt;
                    t.fire = true; // (wiki: Bed - the blast starts fires)
                    if (survival) {
                        t.player = &player;
                        t.vitals = &vitals;
                    }
                    bedExplosion.explode(world, {head.x + 0.5, head.y + 0.5, head.z + 0.5}, 5.0f,
                                         gameRng, droppedItems, frameEdits, t);
                    break;
                }
                case mc::BedUse::TooFar:
                    chat.addMessage("You can't rest now; the bed is too far away", 0xFFFFFFFFu,
                                    gameTime, gui.batch());
                    break;
                case mc::BedUse::Obstructed:
                    chat.addMessage("This bed is obstructed", 0xFFFFFFFFu, gameTime, gui.batch());
                    break;
                case mc::BedUse::NotABed:
                    break;
                }
            }
            pendingBedUse.reset();

            // Commands wait until the player's chunk is there (--command scripts run
            // before the world has streamed in otherwise).
            if (!pendingChat.empty() &&
                world.chunk(mc::world::ChunkPos{
                    mc::world::blockToChunk(int(std::floor(player.position().x))),
                    mc::world::blockToChunk(int(std::floor(player.position().z)))}) &&
                !spawnPending && !arrival) {
                for (const auto& line : pendingChat)
                    runChatLine(line);
                pendingChat.clear();
            }
            for (const auto& t : pendingThrows)
                droppedItems.throwFrom(
                    player.eyePosition(1.0),
                    glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())), t, gameRng);
            pendingThrows.clear();
            mc::PlayerInput input = readInput(window);
            // Presses since the last tick (only the first tick of a frame sees them).
            input.jumpPresses = window.cursorCaptured() ? window.takePresses(mc::Press::Jump) : 0;
            if (opts->autoFly) { // benchmark: constant sprint-flight forward
                input.forward = 1.0f;
                input.sprint = true;
            }
            player.setSpectator(gameMode == 3);
            player.setCreative(!survival);
            if (survival && player.flying()) player.setFlying(false);
            if (dead) input = {};
            // Asleep (M17.4): lying on the bed; after 100 ticks the night is skipped;
            // sneaking gets up early (vanilla: Leave Bed).
            if (sleepTicks > 0) {
                const bool getUp = input.sneak || dead;
                input = {};
                const bool stillBed = mc::bedHead(world, sleepBed).has_value();
                if (!getUp && stillBed && ++sleepTicks < 100) {
                    player.setPosition({sleepBed.x + 0.5, sleepBed.y + 0.5625, sleepBed.z + 0.5});
                    player.setVelocity(glm::dvec3(0.0));
                } else {
                    if (sleepTicks >= 100) {
                        dayTime = mc::morningAfter(dayTime); // the night passes
                        // A tamed cat near the bed may bring a morning gift (wiki: Cat › Gifts:
                        // 70%; ours from the items we have).
                        static constexpr const char* kGifts[4] = {"string", "feather", "chicken",
                                                                  "rotten_flesh"};
                        world.forEachChunk([&](mc::world::Chunk& gc) {
                            for (const auto& cat : gc.mobs())
                                if (cat.type == mc::world::MobType::Cat && cat.tamed &&
                                    !cat.sitting && cat.health > 0.0f &&
                                    glm::length(cat.pos - player.position()) < 16.0 &&
                                    gameRng.nextInt(10) < 7)
                                    if (const auto gift = mc::world::itemRegistry().find(
                                            kGifts[gameRng.nextInt(4)]))
                                        droppedItems.spawn(player.position() +
                                                               glm::dvec3(0, 0.5, 0),
                                                           {*gift, 1}, gameRng);
                        });
                        // ...and the rain stops: the weather cycle starts over (wiki: Bed).
                        if (weather.raining) weather.set(mc::world::Weather::Kind::Clear, 0);
                    }
                    if (stillBed)
                        if (const auto spot = mc::bedStandSpot(world, sleepBed))
                            player.setPosition(*spot);
                    sleepTicks = 0;
                    vitals.setTimeSinceRest(0); // (M26.4a: rested - no phantoms)
                }
            }
            input.canSprint = (!survival || vitals.canSprint()) && // hunger ends a sprint too
                              vitals.effectLevel(mc::world::Effect::Blindness) == 0; // (and Blindness - M29.2a)
            // (M29.2a; wiki: Infested) each hit 1 in 10: 1-2 silverfish come out of the player.
            for (int hits = vitals.takeInfestedHits(); hits > 0; --hits)
                if (gameRng.nextInt(10) == 0)
                    for (int k = 0, n = 1 + int(gameRng.nextInt(2)); k < n; ++k)
                        mc::Mobs::add(world, mc::Mobs::make(mc::world::MobType::Silverfish, player.position(), gameRng));
            const glm::dvec3 before = player.position();
            const bool wasOnGround = player.onGround();
            {
                using E = mc::world::Effect;
                vitals.setRules(rules.fallDamage, rules.fireDamage, rules.drowningDamage,
                                rules.naturalRegeneration, rules.freezeDamage);
                interaction.setBlockDrops(rules.blockDrops);
                interaction.setMayBuild(gameMode != 2);
                vitals.setDifficulty(difficulty);
                vitals.tickEffects(); // (M19.4: in any game mode)
                player.setEffects(vitals.effectLevel(E::Speed), vitals.effectLevel(E::Slowness),
                                  vitals.effectLevel(E::JumpBoost),
                                  vitals.effectLevel(E::SlowFalling) > 0,
                                  vitals.effectLevel(E::Levitation));
                player.setDolphinsGrace(vitals.effectLevel(E::DolphinsGrace) > 0); // (M25.3b)
                player.setSwiftSneak(mc::world::enchantLevel(
                    inventory.armor(2), mc::world::Enchantment::SwiftSneak)); // (M27.3)
                player.setBootEnchants(mc::world::enchantLevel(inventory.armor(3), mc::world::Enchantment::DepthStrider),
                                       mc::world::enchantLevel(inventory.armor(3), mc::world::Enchantment::SoulSpeed));
                // A turtle shell worn above water gives 10 s of Water Breathing, which then
                // runs down under water (wiki: Turtle Shell).
                static const mc::world::ItemId turtleHelmet =
                    *mc::world::itemRegistry().find("turtle_helmet");
                if (inventory.armor(0).item == turtleHelmet && !dead) {
                    if (!mc::pointInFluid(world, player.eyePosition(1.0), mc::world::blocks::Water))
                        vitals.addEffect(E::WaterBreathing, 0, 200);
                }
            }
            {
                // An elytra worn and not about to break can glide (wiki: Elytra - it stops
                // working at 1 durability left).
                static const mc::world::ItemId elytraItem =
                    *mc::world::itemRegistry().find("elytra");
                const mc::world::ItemStack& chestPiece = inventory.armor(1);
                player.setCanGlide(!dead && chestPiece.item == elytraItem &&
                                   chestPiece.damage < 431);
                // (M29.4c; wiki: Powder Snow) leather boots walk on powder snow
                static const mc::world::ItemId leatherBoots = *mc::world::itemRegistry().find("leather_boots");
                player.setPowderSnowWalker(inventory.armor(3).item == leatherBoots);
            }
            if (elytraBoost > 0) { // (M28.4c) a firework pushing the glide along the look
                if (player.gliding()) {
                    player.setVelocity(mc::boostedVelocity(
                        player.velocity(),
                        glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch()))));
                    --elytraBoost;
                } else {
                    elytraBoost = 0;
                }
            }
            if (!arrival && ridingCart == 0)
                player.tick(world, input);               // waiting for a destination: held in place
            // (M30.1) the body's animation follows where the player went this tick.
            if (!animStarted) animPrev = player.position(), animStarted = true;
            playerAnim.tick(player.position(), animPrev, player.yaw(), player.onGround() || ridingCart != 0,
                            inventory.selectedStack().item);
            animPrev = player.position();
            if (player.takeBounce()) vitals.resetFall(); // (slime blocks: no fall damage, M21.5)
            // Magma blocks burn what stands on them (M29 review; wiki: Magma Block).
            if (survival && !dead && player.onGround() && ridingCart == 0) {
                const glm::dvec3 f = player.position();
                const mc::world::BlockPos below{int(std::floor(f.x)), int(std::floor(f.y - 0.2)), int(std::floor(f.z))};
                if (mc::world::blockRegistry().blockOf(world.getBlock(below)) == mc::world::blocks::MagmaBlock)
                    vitals.hotFloor(player.sneaking(),
                                    mc::world::enchantLevel(inventory.armor(3), mc::world::Enchantment::FrostWalker) > 0);
            }
            // Frost Walker (M29.2b; wiki): on the ground, still water sources within 2 + level
            // blocks under the player's level freeze into frosted ice (with air above).
            if (const int frost = mc::world::enchantLevel(inventory.armor(3), mc::world::Enchantment::FrostWalker);
                frost > 0 && player.onGround() && !dead && ridingCart == 0) {
                const auto& reg = mc::world::blockRegistry();
                const int r = 2 + frost;
                const glm::dvec3 f = player.position();
                const int fy = int(std::floor(f.y)) - 1;
                const mc::world::BlockStateId water = reg.defaultState(mc::world::blocks::Water);
                for (int dz = -r; dz <= r; ++dz)
                    for (int dx = -r; dx <= r; ++dx) {
                        if (dx * dx + dz * dz > r * r) continue;
                        const mc::world::BlockPos w{int(std::floor(f.x)) + dx, fy, int(std::floor(f.z)) + dz};
                        if (world.getBlock(w) != water || world.getBlock({w.x, w.y + 1, w.z}) != 0) continue;
                        world.updateBlock(w, reg.defaultState(mc::world::blocks::FrostedIce));
                        blockUpdates.schedule(w, mc::world::blocks::FrostedIce, 20 + int(gameRng.nextInt(20)), 0);
                        frameRemesh.push_back(w); // (water and frosted ice light alike: no relight - M29 review)
                    }
            }
            // Soul Speed wears the boots now and then on soul blocks (wiki: 4% a tick, ours 1 in 25).
            if (survival && player.onSoulBlock() && gameRng.nextInt(25) == 0 &&
                mc::world::enchantLevel(inventory.armor(3), mc::world::Enchantment::SoulSpeed) > 0)
                inventory.setArmor(3, mc::wearItem(inventory.armor(3), 1, gameRng));
            { // Powder snow (M29.4c; wiki: Powder Snow): inside it the player sinks slowly, moves
                // slowly, takes no fall damage and freezes - not with any leather armor on.
                const mc::Aabb box = player.box();
                bool inPowder = false;
                for (int y = int(std::floor(box.min.y)); y <= int(std::floor(box.max.y - 1e-7)) && !inPowder; ++y)
                    for (int z = int(std::floor(box.min.z)); z <= int(std::floor(box.max.z - 1e-7)) && !inPowder; ++z)
                        for (int x = int(std::floor(box.min.x)); x <= int(std::floor(box.max.x - 1e-7)); ++x)
                            if (mc::world::blockRegistry().blockOf(world.getBlock({x, y, z})) ==
                                mc::world::blocks::PowderSnow) {
                                inPowder = true;
                                break;
                            }
                inPowder = inPowder && !player.spectator();
                static const std::array<mc::world::ItemId, 4> leather = {
                    *mc::world::itemRegistry().find("leather_helmet"), *mc::world::itemRegistry().find("leather_chestplate"),
                    *mc::world::itemRegistry().find("leather_leggings"), *mc::world::itemRegistry().find("leather_boots")};
                bool warm = !survival || dead;
                for (int piece = 0; piece < 4; ++piece)
                    warm = warm || inventory.armor(piece).item == leather[size_t(piece)];
                vitals.tickFreezing(inPowder, warm);
                if (inPowder && !player.flying()) {
                    const glm::dvec3 v = player.velocity();
                    player.setVelocity({v.x * 0.5, std::max(v.y, -0.12), v.z * 0.5});
                    vitals.resetFall();
                }
            }
            if (ridingCart == 0 && !player.flying()) {
                // A sweet berry bush (M26.3; wiki) slows whoever walks through it and pricks
                // them (1 damage) while they move, once it has grown past its first stage.
                const glm::dvec3 f = player.position();
                for (int dy = 0; dy <= 1; ++dy) {
                    const mc::world::BlockStateId bs = world.getBlock(
                        {int(std::floor(f.x)), int(std::floor(f.y)) + dy, int(std::floor(f.z))});
                    // (M29.4a; wiki: Wither Rose) walking into one withers (not in creative)
                    if (mc::world::blockRegistry().blockOf(bs) == mc::world::blocks::WitherRose && survival && !dead &&
                        difficulty != 0) // (wiki: not on Peaceful)
                        vitals.addEffect(mc::world::Effect::Wither, 0, 40);
                    if (mc::world::blockRegistry().blockOf(bs) == mc::world::blocks::Cobweb) {
                        // (M26.4a; wiki: Cobweb) stuck: a quarter of the speed, almost no fall;
                        // (M29.2a) Weaving halves the slowdown.
                        const glm::dvec3 v = player.velocity();
                        const double k = vitals.effectLevel(mc::world::Effect::Weaving) > 0 ? 0.5 : 0.25;
                        player.setVelocity({v.x * k, v.y * 0.05, v.z * k});
                        vitals.resetFall();
                        break;
                    }
                    if (mc::world::blockRegistry().blockOf(bs) != mc::world::blocks::SweetBerryBush)
                        continue;
                    const glm::dvec3 v = player.velocity();
                    player.setVelocity({v.x * 0.8, v.y * 0.75, v.z * 0.8});
                    if (survival && !dead &&
                        mc::world::blockRegistry().get(bs, mc::world::properties::age3) > 0 &&
                        (std::abs(v.x) > 0.003 || std::abs(v.z) > 0.003))
                        vitals.attacked(1.0f, nullptr);
                    break;
                }
            }
            if (ridingCart != 0) { // in a minecart: shift gets out; forward pushes it on
                mc::world::MobData* cart = findCart();
                // (a wild mount that threw its rider: ridden is cleared - M26.2)
                if (!cart || dead || input.sneak || !cart->ridden || cart->health <= 0.0f) {
                    mountJumpTicks = 0;
                    if (cart) { // out to a safe spot beside the cart, else on top (wiki: Minecart)
                        const auto& reg = mc::world::blockRegistry();
                        cart->ridden = false;
                        glm::dvec3 out = cart->pos + glm::dvec3(0.0, 1.0, 0.0);
                        const mc::world::BlockPos c{int(std::floor(cart->pos.x)),
                                                    int(std::floor(cart->pos.y)),
                                                    int(std::floor(cart->pos.z))};
                        for (const glm::ivec2 d : {glm::ivec2{0, -1}, glm::ivec2{1, 0},
                                                   glm::ivec2{0, 1}, glm::ivec2{-1, 0}}) {
                            const mc::world::BlockPos f{c.x + d.x, c.y, c.z + d.y};
                            if (!reg.collides(world.getBlock(f)) &&
                                !reg.collides(world.getBlock({f.x, f.y + 1, f.z})) &&
                                reg.collides(world.getBlock({f.x, f.y - 1, f.z}))) {
                                out = {f.x + 0.5, double(f.y), f.z + 0.5};
                                break;
                            }
                        }
                        player.setPosition(out);
                    }
                    ridingCart = 0;
                } else if (mc::world::isMount(cart->type)) {
                    // Riding (M26.2, Mounts.cpp applies it): W/S/A/D, the mount faces the
                    // rider's look; holding jump fills the jump bar (vanilla: about a second
                    // to full), letting go jumps; a camel's jump is its dash; sprinting
                    // speeds a camel up.
                    cart->paddleForward = int8_t(input.forward > 0.0f   ? (input.sprint ? 2 : 1)
                                                 : input.forward < 0.0f ? -1
                                                                        : 0);
                    cart->paddleTurn = int8_t(input.strafe > 0.0f   ? 1
                                              : input.strafe < 0.0f ? -1
                                                                    : 0);
                    cart->headYaw = player.yaw();
                    cart->pitch =
                        player.pitch(); // (a nautilus swims where the rider looks - M26.5a)
                    if (cart->type == mc::world::MobType::HappyGhast && input.jump)
                        cart->paddleTurn = 2; // (M26.5b: up)
                    if (cart->type ==
                        mc::world::MobType::Nautilus || cart->type == mc::world::MobType::ZombieNautilus) // (its rider keeps their breath)
                        vitals.addEffect(mc::world::Effect::BreathOfTheNautilus, 0, 40);
                    if (input.jump) {
                        mountJumpTicks = std::min(mountJumpTicks + 1, 10);
                    } else if (mountJumpTicks > 0) {
                        cart->riderJump = int8_t(mountJumpTicks * 10);
                        mountJumpTicks = 0;
                    }
                } else if (mc::world::isStickRidden(cart->type)) {
                    // A pig or strider (M29.3d) goes where the rider looks only while they
                    // hold its stick - a carrot / warped fungus on a stick; using the stick
                    // boosts it (the stick wears: carrot 7, fungus 1).
                    static const mc::world::ItemId carrotStick = *mc::world::itemRegistry().find("carrot_on_a_stick"),
                                                   fungusStick = *mc::world::itemRegistry().find("warped_fungus_on_a_stick");
                    const mc::world::ItemId stick = cart->type == mc::world::MobType::Pig ? carrotStick : fungusStick;
                    const bool holding = inventory.selectedStack().item == stick;
                    cart->headYaw = player.yaw();
                    cart->paddleForward = int8_t(holding ? 1 : 0);
                } else if (cart->type == mc::world::MobType::Boat) {
                    // Paddling (M25.2b): W/S forward/back, A/D turn (Boats.cpp applies it).
                    cart->paddleForward = int8_t(input.forward > 0.0f   ? 1
                                                 : input.forward < 0.0f ? -1
                                                                        : 0);
                    cart->paddleTurn = int8_t(input.strafe > 0.0f   ? 1
                                              : input.strafe < 0.0f ? -1
                                                                    : 0);
                } else if (input.forward > 0.0f) {
                    const glm::dvec3 f(mc::world::forwardFlat(player.yaw()));
                    if (cart->vel.x * cart->vel.x + cart->vel.z * cart->vel.z < 0.01)
                        cart->vel += f * 0.04;
                }
            }
            const auto& reg = mc::world::blockRegistry();
            const glm::dvec3 feet = player.position();
            const mc::world::BlockPos feetBlock{int(std::floor(feet.x)), int(std::floor(feet.y)),
                                                int(std::floor(feet.z))};
            const bool inWater = reg.blockOf(world.getBlock(feetBlock)) == mc::world::blocks::Water;
            // Statistics (M28.1d; vanilla's movement stats in cm): riding by vehicle, else
            // swimming, under/on water, climbing, walking/sprinting/crouching, elytra,
            // flight (horizontal where vanilla counts it so); jumps; falls of 2+ on landing.
            if (!dead && !arrival) {
                using S = mc::world::Stat;
                const glm::dvec3 d = feet - before;
                const int64_t full = std::llround(glm::length(d) * 100.0);
                const int64_t horiz = std::llround(glm::length(glm::dvec2(d.x, d.z)) * 100.0);
                const bool eyesWet =
                    mc::pointInFluid(world, player.eyePosition(1.0), mc::world::blocks::Water);
                if (ridingCart != 0) {
                    if (const mc::world::MobData* v = findCart())
                        stats.add(v->type == mc::world::MobType::Minecart ? S::MinecartOneCm
                                  : v->type == mc::world::MobType::Boat   ? S::BoatOneCm
                                                                          : S::HorseOneCm,
                                  full);
                } else if (eyesWet && player.sprinting()) {
                    stats.add(S::SwimOneCm, full);
                } else if (eyesWet) {
                    stats.add(S::WalkUnderWaterOneCm, full);
                } else if (player.inWater()) {
                    stats.add(S::WalkOnWaterOneCm, horiz);
                } else if (player.climbing() && d.y > 0.0) {
                    stats.add(S::ClimbOneCm, std::llround(d.y * 100.0));
                } else if (player.onGround()) {
                    stats.add(player.sprinting()  ? S::SprintOneCm
                              : player.sneaking() ? S::CrouchOneCm
                                                  : S::WalkOneCm,
                              horiz);
                } else if (player.gliding()) {
                    stats.add(S::AviateOneCm, full);
                } else if (player.flying()) {
                    stats.add(S::FlyOneCm, horiz);
                }
                if (wasOnGround && !player.onGround() && player.velocity().y > 0.0 &&
                    !player.flying() && ridingCart == 0)
                    stats.add(S::Jump);
                if (player.onGround() && !wasOnGround && !player.flying() &&
                    airPeakY - feet.y >= 2.0)
                    stats.add(S::FallOneCm, std::llround((airPeakY - feet.y) * 100.0));
            }
            // Landing on farmland may trample it: chance fall distance - 0.5 (wiki: Farmland).
            if (player.onGround() || player.flying() || inWater) {
                if (player.onGround() && !wasOnGround && !player.flying()) {
                    const mc::world::BlockPos below{feetBlock.x, int(std::floor(feet.y - 0.01)),
                                                    feetBlock.z};
                    if (reg.blockOf(world.getBlock(below)) == mc::world::blocks::Farmland &&
                        gameRng.nextFloat() < float(airPeakY - feet.y - 0.5))
                        blockUpdates.trample(below);
                }
                airPeakY = feet.y;
            } else {
                airPeakY = std::max(airPeakY, feet.y);
            }
            if (survival && !dead) {
                // Exhaustion (wiki: Hunger): sprinting 0.1 per metre, jumps 0.05 (0.2
                // sprinting).
                if (player.sprinting())
                    vitals.exhaust(0.1f * float(glm::length(
                                              glm::dvec2(feet.x - before.x, feet.z - before.z))));
                if (wasOnGround && !player.onGround() && player.velocity().y > 0.0)
                    vitals.exhaust(player.sprinting() ? 0.2f : 0.05f);
                if (!arrival) {
                    // (gliding counts as flying for falls: our simplification)
                    {
                        const mc::world::BlockPos under{int(std::floor(feet.x)),
                                                        int(std::floor(feet.y - 0.2)),
                                                        int(std::floor(feet.z))};
                        vitals.setLandingFactor(reg.blockOf(world.getBlock(under)) ==
                                                        mc::world::blocks::HayBlock
                                                    ? 0.2f
                                                    : 1.0f);
                        const mc::world::BlockStateId us =
                            world.getBlock(under); // (M27.2b: onto a point)
                        vitals.setStalagmite(
                            reg.blockOf(us) == mc::world::blocks::PointedDripstone &&
                            reg.get(us, mc::world::properties::verticalDirection) == 0 &&
                            reg.get(us, mc::world::properties::thickness) <= 1);
                    }
                    vitals.tick(feet.y, player.onGround(), inWater || player.inWater(),
                                player.flying() || player.gliding() ||
                                    player.climbing()); // (ladders: no fall)
                    if (const float impact = player.takeImpact(); impact > 0.0f)
                        vitals.damage(impact); // (armour doesn't help)
                    if (player.gliding() &&
                        ++glideTicks % 20 == 0) // an elytra wears 1 per second of flight
                        inventory.setArmor(1, mc::wearItem(inventory.armor(1), 1, gameRng));
                    // Drowning, lava and burning (M14; wiki: Drowning, Lava, Fire).
                    const int respiration = mc::world::enchantLevel(
                        inventory.armor(0), mc::world::Enchantment::Respiration);
                    vitals.breathe(
                        mc::eyesUnderWater(world, player.eyePosition(1.0)), // (bubble columns: air)
                        respiration > 0 && gameRng.nextInt(uint32_t(respiration + 1)) > 0);
                    if (player.inLava()) {
                        vitals.attacked(
                            4.0f, nullptr,
                            mc::Vitals::Hit::Fire); // lava: armor reduces it (wiki: Armor)
                        vitals.setOnFire(300);      // 15 s
                    }
                    { // (M29.4c: soul fire hurts 2)
                        const bool soul = mc::portals::touching(world, player.box(), mc::world::blocks::SoulFire);
                        vitals.touchFire(soul || mc::portals::touching(world, player.box(), mc::world::blocks::Fire),
                                         soul ? 2.0f : 1.0f);
                    }
                    // A lit campfire burns what stands in it: 1, soul campfires 2 (wiki: Campfire).
                    {
                        const mc::world::BlockPos in{int(std::floor(feet.x)),
                                                     int(std::floor(feet.y)),
                                                     int(std::floor(feet.z))};
                        const auto inState = world.getBlock(in);
                        const auto inBlock = reg.blockOf(inState);
                        if ((inBlock == mc::world::blocks::Campfire ||
                             inBlock == mc::world::blocks::SoulCampfire) &&
                            reg.get(inState, mc::world::properties::lit) == 0)
                            vitals.attacked(inBlock == mc::world::blocks::SoulCampfire ? 2.0f
                                                                                       : 1.0f,
                                            nullptr, mc::Vitals::Hit::Fire);
                        // Cauldrons (M23.5; wiki: Cauldron): inside a lava one burns like lava;
                        // a burning player in a water one is put out and the water drops a level.
                        const double depth = feet.y - in.y;
                        if (inBlock == mc::world::blocks::LavaCauldron && depth < 15.0 / 16.0) {
                            vitals.attacked(4.0f, nullptr, mc::Vitals::Hit::Fire);
                            vitals.setOnFire(300);
                        } else if (inBlock == mc::world::blocks::WaterCauldron &&
                                   vitals.burning()) {
                            const int lvl = reg.get(
                                inState, mc::world::properties::cauldronLevel); // 0..2 = 1..3
                            if (depth < (6.0 + 3.0 * (lvl + 1)) / 16.0) {
                                vitals.setFireTicks(0);
                                world.updateBlock(
                                    in, lvl == 0
                                            ? reg.defaultState(mc::world::blocks::Cauldron)
                                            : reg.set(inState, mc::world::properties::cauldronLevel,
                                                      lvl - 1));
                            }
                        }
                    }
                    // Touching a cactus (beside or on top) hurts 1 (wiki: Cactus); our
                    // cactus collides as a full cube, so the box reaches out a hair.
                    if (mc::portals::touching(world, player.box().inflated(0.001),
                                              mc::world::blocks::Cactus))
                        vitals.attacked(1.0f);
                    // Rain puts the player out like water does (wiki: Fire).
                    const glm::dvec3 eye = player.eyePosition(1.0);
                    vitals.tickFire(
                        player.inWater() ||
                        (dimension == Dimension::Overworld &&
                         mc::world::rainingAt(world, weather,
                                              {int(std::floor(eye.x)), int(std::floor(eye.y)),
                                               int(std::floor(eye.z))})));
                }
            }
            // A totem of undying in either hand saves the player once (wiki: Totem of
            // Undying): 1 health, effects cleared, Regeneration II 45 s, Fire Resistance
            // 40 s, Absorption II 5 s.
            if (!dead && vitals.dead()) {
                static const mc::world::ItemId totem =
                    *mc::world::itemRegistry().find("totem_of_undying");
                const bool inHand = inventory.selectedStack().item == totem;
                if (inHand || inventory.offhand().item == totem) {
                    if (inHand)
                        inventory.consumeSelected(1);
                    else
                        inventory.setOffhand({});
                    vitals.setHealth(1.0f);
                    vitals.clearEffects();
                    vitals.addEffect(mc::world::Effect::Regeneration, 1, 900);
                    vitals.addEffect(mc::world::Effect::FireResistance, 0, 800);
                    vitals.addEffect(mc::world::Effect::Absorption, 1, 100);
                    const glm::dvec3 f = player.position();
                    world.levelEvent(mc::world::LevelEvent::Type::Crit, f.x, f.y + 1.0, f.z);
                }
            }
            if (!dead && vitals.dead()) { // drop everything where we died (unless keep_inventory)
                stats.add(mc::world::Stat::Deaths);
                // The 1.21 potions' effects on death (M29.2a; wiki: Wind Charged - a wind
                // burst; Weaving - 2-3 cobwebs about; Oozing - two medium slimes).
                if (vitals.effectLevel(mc::world::Effect::WindCharged) > 0)
                    projectiles.shoot(mc::ProjectileKind::WindCharge, feet + glm::dvec3(0, 0.5, 0),
                                      glm::dvec3(0, -1, 0), 0.5, 0.0, false, false, gameRng);
                if (vitals.effectLevel(mc::world::Effect::Weaving) > 0)
                    for (int k = 0, placed = 0; k < 12 && placed < 3; ++k) {
                        const mc::world::BlockPos w{int(std::floor(feet.x)) + int(gameRng.nextInt(5)) - 2,
                                                    int(std::floor(feet.y)) + int(gameRng.nextInt(2)),
                                                    int(std::floor(feet.z)) + int(gameRng.nextInt(5)) - 2};
                        if (world.getBlock(w) == 0) {
                            world.updateBlock(w, mc::world::blockRegistry().defaultState(mc::world::blocks::Cobweb));
                            frameEdits.push_back(w);
                            ++placed;
                        }
                    }
                if (vitals.effectLevel(mc::world::Effect::Oozing) > 0)
                    for (int k = 0; k < 2; ++k) {
                        mc::world::MobData slime = mc::Mobs::make(mc::world::MobType::Slime, feet, gameRng);
                        slime.size = 2;
                        slime.health = 4.0f;
                        mc::Mobs::add(world, slime);
                    }
                lastDeath = {
                    {int(std::floor(feet.x)), int(std::floor(feet.y)), int(std::floor(feet.z))},
                    dimension};
                stats.set(mc::world::Stat::TimeSinceDeath, 0);
                if (const mc::world::MobData* killer =
                        mobs.playerAttacker()
                            ? mc::Mobs::mobByUuid(world, feet, mobs.playerAttacker())
                            : nullptr)
                    stats.addKilledBy(killer->type);
                if (rules.keepInventory) {
                    screenDrops.clear();
                    if (container.isOpen()) container.close(inventory, screenDrops);
                    if (creative.isOpen()) creative.close();
                    for (const auto& d : screenDrops) // (the crafting grid still spills)
                        droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), d, gameRng);
                    dead = true;
                    if (rules.showDeathMessages)
                        chat.addMessage("Player died", 0xFFFFFFFFu, gameTime, gui.batch());
                } else {
                    // Open screens close first: their grid/carried items drop too.
                    screenDrops.clear();
                    if (container.isOpen()) container.close(inventory, screenDrops);
                    if (creative.isOpen()) creative.close();
                    for (const auto& d : screenDrops)
                        droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), d, gameRng);
                    dead = true;
                    // (M29.2b; wiki: Curse of Vanishing) cursed items are gone, not dropped.
                    const auto keepOnDeath = [](const mc::world::ItemStack& it) {
                        return mc::world::enchantLevel(it, mc::world::Enchantment::VanishingCurse) > 0
                                   ? mc::world::ItemStack{}
                                   : it;
                    };
                    for (int s = 0; s < mc::Inventory::kSlots; ++s) {
                        droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), keepOnDeath(inventory.slot(s)), gameRng,
                                           40);
                        inventory.setSlot(s, {});
                    }
                    for (int piece = 0; piece < 4; ++piece) { // worn armor and the offhand too
                        droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), keepOnDeath(inventory.armor(piece)),
                                           gameRng, 40);
                        inventory.setArmor(piece, {});
                    }
                    droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), keepOnDeath(inventory.offhand()), gameRng,
                                       40);
                    inventory.setOffhand({});
                    orbs.drop(feet + glm::dvec3(0, 0.5, 0), vitals.deathExperience(),
                              gameRng); // (the rest is lost)
                    if (rules.showDeathMessages)
                        chat.addMessage("Player died", 0xFFFFFFFFu, gameTime, gui.batch());
                }
            }
            // Q drops one of the held item (wiki: Controls).
            if (window.cursorCaptured() && window.takePresses(mc::Press::Drop) > 0 &&
                gameMode != 3 && // (spectators: none)
                !inventory.selectedStack().empty()) {
                mc::world::ItemStack one = inventory.selectedStack();
                one.count = 1;
                droppedItems.throwFrom(
                    player.eyePosition(1.0),
                    glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())), one, gameRng);
                inventory.consumeSelected(1);
                stats.add(mc::world::Stat::Drop);
                stats.addItem(mc::world::ItemStat::Dropped, one.item);
            }
            mc::InteractionInput clicks;
            clicks.attack = window.cursorCaptured() && attackArmed && window.leftMousePressed();
            clicks.use = window.cursorCaptured() && window.rightMousePressed();
            clicks.attackClick =
                window.cursorCaptured() && window.takePresses(mc::Press::LeftMouse) > 0;
            clicks.useClick =
                window.cursorCaptured() && window.takePresses(mc::Press::RightMouse) > 0;
            // --use N (screenshots): scripted right-clicks, half a second apart from tick 5.
            if (scriptedUses > 0 && gameTime >= scriptedUseAt) {
                clicks.useClick = true;
                --scriptedUses;
                scriptedUseAt = gameTime + 10;
            }
            if (gameMode == 3) clicks = {}; // spectators touch nothing (M28.1c)
            // (M30.1) the arm swings on a click and keeps swinging while a block is mined.
            if (clicks.attackClick || clicks.useClick || (clicks.attack && interaction.breakingBlock()))
                playerAnim.startSwing();
            const bool mayBuild =
                gameMode != 2; // adventure: no breaking, placing or block-changing items
            // (M28.3) a click on an item frame or an armor stand is for it, not for the held item
            bool decorInFront = false;
            if (clicks.useClick) {
                const auto mh = mc::Mobs::raycast(
                    world, player.eyePosition(1.0),
                    glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())),
                    survival ? 3.0 : 5.0, ridingCart);
                if (mh && (!lastHit || mh->distance < lastHit->distance)) {
                    const auto t = world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type;
                    decorInFront = mc::world::isHanging(t) || t == mc::world::MobType::ArmorStand ||
                                   t == mc::world::MobType::LeashKnot ||
                                   (inventory.selectedStack().item ==
                                        *mc::world::itemRegistry().find("lead") &&
                                    mc::world::isLeashable(t)) ||
                                   world.chunk(mh->chunk)->mobs()[size_t(mh->index)].leash == 1;
                }
            }
            // The click is held back from every held-item action until the decoration handlers
            // below (an empty map put into a frame must not turn into a filled one).
            const bool decorClick = decorInFront && clicks.useClick;
            if (decorClick) clicks.useClick = false;
            // Flint and steel lights portals; eyes of ender go into end portal frames (M12).
            // Candles and cakes (M28.5a; wiki: Candle, Cake): flint and steel or a fire charge
            // lights candles (not under water), an empty hand puts them out; a cake is eaten a
            // slice at a time (2 hunger, 0.4 saturation) when hungry - a candle cake gives its
            // candle back first.
            if (!dead && clicks.useClick && lastHit && !player.sneaking()) {
                const auto& creg = mc::world::blockRegistry();
                const mc::world::BlockPos at = lastHit->block;
                const mc::world::BlockStateId cs = world.getBlock(at);
                const mc::world::BlockId cb = creg.blockOf(cs), like = creg.likeOf(cb);
                const std::string_view heldName =
                    mc::world::itemRegistry().item(inventory.selectedStack().item).id;
                const bool igniter =
                    heldName == "minecraft:flint_and_steel" || heldName == "minecraft:fire_charge";
                bool used = false;
                if (like == mc::world::blocks::Candle || like == mc::world::blocks::CandleCake) {
                    const bool isLit = creg.get(cs, mc::world::properties::lit) == 0;
                    const bool wet = like == mc::world::blocks::Candle &&
                                     creg.get(cs, mc::world::properties::waterlogged) == 0;
                    if (igniter && !isLit && !wet) {
                        world.updateBlock(at, creg.set(cs, mc::world::properties::lit, 0));
                        if (survival) {
                            if (heldName == "minecraft:fire_charge")
                                inventory.consumeSelected(1);
                            else
                                inventory.setSlot(
                                    inventory.selected(),
                                    mc::wearItem(inventory.selectedStack(), 1, gameRng));
                        }
                        used = true;
                    } else if (inventory.selectedStack().empty() && isLit) {
                        world.updateBlock(at, creg.set(cs, mc::world::properties::lit, 1));
                        world.levelEvent(mc::world::LevelEvent::Type::Extinguish, at.x + 0.5,
                                         at.y + 0.5, at.z + 0.5);
                        used = true;
                    }
                }
                const bool candleOnCake = like == mc::world::blocks::Candle; // (handled by placing)
                if (!used && !candleOnCake &&
                    (cb == mc::world::blocks::Cake || like == mc::world::blocks::CandleCake) &&
                    !(cb == mc::world::blocks::Cake &&
                      creg.likeOf(
                          mc::world::itemRegistry().item(inventory.selectedStack().item).block) ==
                          mc::world::blocks::Candle &&
                      creg.get(cs, mc::world::properties::bites) == 0) &&
                    (!survival || vitals.food() < mc::Vitals::kMaxFood)) {
                    if (like ==
                        mc::world::blocks::CandleCake) { // its candle comes off, then a slice
                        std::string candle(creg.block(cb).id);
                        candle.erase(candle.rfind("_cake"));
                        if (const auto ci = mc::world::itemRegistry().find(candle))
                            droppedItems.spawn({at.x + 0.5, at.y + 0.9, at.z + 0.5}, {*ci, 1},
                                               gameRng);
                        world.updateBlock(at, creg.set(creg.defaultState(mc::world::blocks::Cake),
                                                       mc::world::properties::bites, 1));
                    } else if (creg.get(cs, mc::world::properties::bites) >= 6) {
                        world.updateBlock(at, 0); // (the last slice)
                    } else {
                        world.updateBlock(at,
                                          creg.set(cs, mc::world::properties::bites,
                                                   creg.get(cs, mc::world::properties::bites) + 1));
                    }
                    vitals.eat(2, 0.4f);
                    used = true;
                }
                // Chiseled bookshelves (M29.5; wiki: Chiseled Bookshelf): a click on the front picks
                // one of its 6 slots - a book goes in, or the book there comes out.
                if (!used && cb == mc::world::blocks::ChiseledBookshelf &&
                    lastHit->face == static_cast<mc::world::Direction>(creg.get(cs, mc::world::properties::facing) + 2)) {
                    const glm::dvec3 hp = player.eyePosition(1.0) +
                                          glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())) * lastHit->distance -
                                          glm::dvec3(at.x, at.y, at.z);
                    const mc::world::Direction f = lastHit->face;
                    const double u = f == mc::world::Direction::North   ? 1.0 - hp.x
                                     : f == mc::world::Direction::South ? hp.x
                                     : f == mc::world::Direction::West  ? hp.z
                                                                        : 1.0 - hp.z;
                    const int slot = mc::world::BlockUpdates::bookshelfSlot(u, hp.y);
                    if (creg.get(cs, mc::world::properties::bookSlots[slot]) == 1) { // empty
                        if (mc::world::BlockUpdates::isBook(inventory.selectedStack().item) &&
                            blockUpdates.putBook(at, slot, inventory.selectedStack())) {
                            if (survival) inventory.consumeSelected(1);
                            used = true;
                        }
                    } else if (const mc::world::ItemStack book = blockUpdates.takeBook(at, slot); !book.empty()) {
                        if (inventory.add(book) > 0)
                            droppedItems.spawn({at.x + 0.5, at.y + 0.6, at.z + 0.5}, book, gameRng);
                        used = true;
                    }
                }
                // Shelves (M29.6; wiki: Shelf): a click on the front swaps the held stack with the
                // one in that third of the shelf; on a powered row, the row's stacks swap with the
                // rightmost hotbar slots (3, 6 or all 9).
                if (!used && like == mc::world::blocks::Shelf &&
                    lastHit->face == static_cast<mc::world::Direction>(creg.get(cs, mc::world::properties::facing) + 2)) {
                    const glm::dvec3 hp = player.eyePosition(1.0) +
                                          glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())) * lastHit->distance -
                                          glm::dvec3(at.x, at.y, at.z);
                    const mc::world::Direction f = lastHit->face;
                    const double u = f == mc::world::Direction::North   ? 1.0 - hp.x
                                     : f == mc::world::Direction::South ? hp.x
                                     : f == mc::world::Direction::West  ? hp.z
                                                                        : 1.0 - hp.z;
                    auto shelfAt = [&](const mc::world::BlockPos& q) -> mc::world::ChestData* {
                        mc::world::Chunk* sc = world.chunk(q.chunk());
                        mc::world::ChestData* sd = sc ? sc->chest(mc::world::blockToLocal(q.x), q.y, mc::world::blockToLocal(q.z)) : nullptr;
                        if (sd && sd->shelf) sc->markDirty();
                        return sd && sd->shelf ? sd : nullptr;
                    };
                    std::array<mc::world::BlockPos, 3> row;
                    const int n = blockUpdates.shelfRow(at, row);
                    if (creg.get(cs, mc::world::properties::powered) == 0) { // powered: the hotbar
                        for (int k = 0; k < n; ++k)
                            if (mc::world::ChestData* sd = shelfAt(row[size_t(k)]))
                                for (int j = 0; j < 3; ++j) {
                                    // (the rightmost 3 or 6 hotbar slots, all 9 for three - wiki)
                                    const int hs = 9 - 3 * n + k * 3 + j;
                                    const mc::world::ItemStack held = inventory.slot(hs);
                                    inventory.setSlot(hs, sd->items[size_t(j)]);
                                    sd->items[size_t(j)] = held;
                                }
                    } else if (mc::world::ChestData* sd = shelfAt(at)) {
                        const int slot = std::clamp(int(u * 3.0), 0, 2);
                        const mc::world::ItemStack held = inventory.selectedStack();
                        inventory.setSlot(inventory.selected(), sd->items[size_t(slot)]);
                        sd->items[size_t(slot)] = held;
                    }
                    playSound(mc::world::Sound::WoodClick, {at.x + 0.5, at.y + 0.5, at.z + 0.5}, 0.8f, 1.2f, true);
                    used = true;
                }
                // Respawn anchors (M29.5; wiki: Respawn Anchor): glowstone charges one (up to 4);
                // used charged, it sets the respawn point in the Nether and explodes anywhere else.
                if (!used && cb == mc::world::blocks::RespawnAnchor) {
                    const int charge = creg.get(cs, mc::world::properties::charges);
                    if (heldName == "minecraft:glowstone" && charge < 4) {
                        if (mayBuild) {
                            world.updateBlock(at, creg.set(cs, mc::world::properties::charges, charge + 1));
                            if (survival) inventory.consumeSelected(1);
                            playSound(mc::world::Sound::Click, {at.x + 0.5, at.y + 0.5, at.z + 0.5}, 1.0f, 1.0f, true);
                            used = true;
                        }
                    } else if (charge > 0 && dimension == Dimension::Nether) {
                        if (bedSpawn != at || spawnDim != Dimension::Nether)
                            chat.addMessage("Respawn point set", 0xFFFFFFFFu, gameTime, gui.batch());
                        bedSpawn = at;
                        spawnDim = Dimension::Nether;
                        used = true;
                    } else if (charge > 0) { // power 5 with fire, like a bed in the Nether
                        world.updateBlock(at, 0);
                        mc::ExplosionTargets t;
                        t.tnt = &primedTnt;
                        t.fire = true;
                        if (survival) {
                            t.player = &player;
                            t.vitals = &vitals;
                        }
                        bedExplosion.explode(world, {at.x + 0.5, at.y + 0.5, at.z + 0.5}, 5.0f, gameRng, droppedItems,
                                             frameEdits, t);
                        used = true;
                    }
                }
                // (M29.7; wiki: Light) a light block clicked with a light item glows a level brighter.
                if (!used && cb == mc::world::blocks::Light && heldName == "minecraft:light") {
                    world.updateBlock(at, creg.set(cs, mc::world::properties::level,
                                                   (creg.get(cs, mc::world::properties::level) + 1) % 16));
                    used = true;
                }
                // Flower pots (M29.4b; wiki: Flower Pot): a plant goes into an empty pot; a
                // planted pot gives its plant back (into the inventory, else dropped).
                if (!used && mayBuild && like == mc::world::blocks::FlowerPot) {
                    const auto& items = mc::world::itemRegistry();
                    if (cb == mc::world::blocks::FlowerPot) {
                        const mc::world::BlockId potted = mc::world::pottedFor(items.item(inventory.selectedStack().item).block);
                        if (potted) {
                            world.updateBlock(at, creg.defaultState(potted));
                            if (survival) inventory.consumeSelected(1);
                            used = true;
                        }
                    } else if (const mc::world::ItemId plant = items.blockItem(mc::world::plantInPot(cb))) {
                        if (inventory.add({plant, 1}) > 0)
                            droppedItems.spawn({at.x + 0.5, at.y + 0.6, at.z + 0.5}, {plant, 1}, gameRng);
                        world.updateBlock(at, creg.defaultState(mc::world::blocks::FlowerPot));
                        used = true;
                    }
                }
                if (used) {
                    frameEdits.push_back(at);
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Campfires (M23.4c): raw food goes on (any lit or unlit campfire with room), a
            // shovel puts it out, flint and steel lights it again (wiki: Campfire).
            if (!dead && clicks.useClick && lastHit) {
                const auto cst = world.getBlock(lastHit->block);
                const auto cb = reg.blockOf(cst);
                if (cb == mc::world::blocks::Campfire || cb == mc::world::blocks::SoulCampfire) {
                    const mc::world::ItemStack held = inventory.selectedStack();
                    const auto& hdef = mc::world::itemRegistry().item(held.item);
                    const bool isLit = reg.get(cst, mc::world::properties::lit) == 0;
                    mc::world::Chunk* cc = world.chunk(lastHit->block.chunk());
                    mc::world::CampfireData* cf =
                        cc ? cc->campfire(mc::world::blockToLocal(lastHit->block.x),
                                          lastHit->block.y,
                                          mc::world::blockToLocal(lastHit->block.z))
                           : nullptr;
                    bool acted = false;
                    if (cf && hdef.food > 0 && mc::smelt(held)) {
                        for (auto& slot : cf->items)
                            if (slot.empty()) {
                                slot = {held.item, 1};
                                if (survival) inventory.consumeSelected(1);
                                cc->markDirty();
                                acted = true;
                                break;
                            }
                    } else if (isLit && hdef.tool == mc::world::ToolType::Shovel) {
                        world.updateBlock(lastHit->block,
                                          reg.set(cst, mc::world::properties::lit, 1));
                        world.playSound(mc::world::Sound::Fizz, lastHit->block.x + 0.5,
                                        lastHit->block.y + 0.5, lastHit->block.z + 0.5);
                        world.levelEvent(mc::world::LevelEvent::Type::Extinguish,
                                         lastHit->block.x + 0.5, lastHit->block.y + 0.5,
                                         lastHit->block.z + 0.5);
                        if (survival)
                            inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                        frameEdits.push_back(lastHit->block);
                        acted = true;
                    } else if (!isLit && hdef.id == "minecraft:flint_and_steel") {
                        world.updateBlock(lastHit->block,
                                          reg.set(cst, mc::world::properties::lit, 0));
                        if (survival)
                            inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                        frameEdits.push_back(lastHit->block);
                        acted = true;
                    }
                    if (acted) {
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            if (!dead && clicks.useClick && lastHit && mayBuild &&
                reg.blockOf(world.getBlock(lastHit->block)) == mc::world::blocks::Tnt) {
                // Flint and steel or a fire charge lights TNT (wiki: TNT).
                const mc::world::ItemStack held = inventory.selectedStack();
                const std::string_view hid = mc::world::itemRegistry().item(held.item).id;
                if (hid == "minecraft:flint_and_steel" || hid == "minecraft:fire_charge") {
                    blockUpdates.primeTnt(lastHit->block);
                    if (survival) {
                        if (hid == "minecraft:fire_charge")
                            inventory.consumeSelected(1);
                        else
                            inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                    }
                    clicks.useClick = false;
                }
            }
            if (!dead && clicks.useClick && lastHit &&
                mayBuild) { // (sneaking only skips block actions)
                const mc::world::ItemStack held = inventory.selectedStack();
                const size_t editsBefore = frameEdits.size();
                if (!held.empty() &&
                    mc::portals::useItem(world, dimension, held.item, lastHit->block, lastHit->face,
                                         frameEdits)) {
                    const auto& def = mc::world::itemRegistry().item(held.item);
                    for (size_t e = editsBefore; e < frameEdits.size(); ++e) // remember lit portals
                        if (mc::world::blockRegistry().blockOf(world.getBlock(frameEdits[e])) ==
                            mc::world::blocks::NetherPortal) {
                            knownPortals.push_back({dimension, frameEdits[e]});
                            break;
                        }
                    if (survival) {
                        if (def.durability > 0) { // flint and steel wears 1 per use
                            inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                        } else {
                            inventory.consumeSelected(1);
                        }
                    }
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Armor and shield (M17.3): armor values each tick; a shield in either hand
            // held up with right-click blocks after 5 ticks (wiki: Shield).
            {
                static const mc::world::ItemId shieldItem =
                    *mc::world::itemRegistry().find("shield");
                const bool shieldInHand =
                    inventory.selectedStack().item == shieldItem ||
                    (inventory.offhand().item == shieldItem &&
                     mc::world::itemRegistry().item(inventory.selectedStack().item).food == 0);
                shieldTicks = !dead && clicks.use && shieldInHand ? shieldTicks + 1 : 0;
                const glm::dvec3 facing(mc::world::forwardFlat(player.yaw()));
                vitals.setArmor(inventory.armorPoints(), inventory.armorToughness());
                {
                    using E = mc::world::Enchantment;
                    int prot[5] = {};
                    for (int piece = 0; piece < 4; ++piece) {
                        const auto& a = inventory.armor(piece);
                        prot[0] += mc::world::enchantLevel(a, E::Protection);
                        prot[1] += mc::world::enchantLevel(a, E::FireProtection);
                        prot[2] += mc::world::enchantLevel(a, E::BlastProtection);
                        prot[3] += mc::world::enchantLevel(a, E::ProjectileProtection);
                        prot[4] += mc::world::enchantLevel(a, E::FeatherFalling);
                    }
                    vitals.setProtection(prot[0], prot[1], prot[2], prot[3], prot[4]);
                }
                vitals.setShield(shieldTicks >= 5, player.eyePosition(1.0), facing);
                if (!dead && clicks.useClick && !decorInFront &&
                    inventory.equipSelected()) { // armor in hand: put it on
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Bows and eggs (M16.4; wiki: Bow, Egg): hold right-click to draw a bow (it
            // needs an arrow in survival), release to shoot; right-click throws an egg.
            {
                const std::string_view heldId =
                    mc::world::itemRegistry().item(inventory.selectedStack().item).id;
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                // A compass used on a lodestone points at it from then on (M28.2a; wiki:
                // Lodestone): one compass of the stack becomes a lodestone compass.
                if (!dead && heldId == "minecraft:compass" && clicks.useClick && lastHit &&
                    mc::world::blockRegistry().blockOf(world.getBlock(lastHit->block)) ==
                        mc::world::blocks::Lodestone) {
                    mc::world::ItemStack bound = inventory.selectedStack();
                    bound.count = 1;
                    bound.extra =
                        mc::world::addLodestoneTarget({lastHit->block, uint8_t(dimension)});
                    announce(adv.onEvent(mc::world::AdvEvent::UsedLodestone));
                    if (inventory.selectedStack().count == 1) {
                        inventory.setSlot(inventory.selected(), bound);
                    } else {
                        if (survival) inventory.consumeSelected(1);
                        if (inventory.add(bound) > 0)
                            droppedItems.spawn(player.position(), bound, gameRng);
                    }
                    playSound(mc::world::blockSound(
                                  mc::world::soundTypeOf(world.getBlock(lastHit->block)),
                                  mc::world::BlockSound::Place),
                              glm::dvec3(lastHit->block.x + 0.5, lastHit->block.y + 0.5,
                                         lastHit->block.z + 0.5),
                              1.0f, 1.0f, true);
                    clicks.useClick = false;
                    clicks.use = false;
                }
                if (!dead && heldId == "minecraft:bow" && clicks.use &&
                    mc::canDrawBow(inventory, survival)) {
                    ++bowTicks;
                    clicks.useClick = false;
                } else if (bowTicks > 0) {
                    if (!dead && heldId == "minecraft:bow" &&
                        mc::releaseBow(inventory, bowTicks, survival, eye, look, projectiles,
                                       gameRng)) {
                        // Vanilla: pitch 1 / (random x 0.4 + 1.2) + power / 2.
                        playSound(mc::world::Sound::BowShoot, eye, 1.0f,
                                  0.75f + mc::bowPower(bowTicks) * 0.5f, false);
                        announce(adv.onEvent(mc::world::AdvEvent::ShotArrow));
                    }
                    bowTicks = 0;
                }
                // Spears (M28.4e): held out (right-click) for up to 5 s, the charge hits what is in
                // front when moving fast - on foot sprinting, faster on a mount.
                if (!dead &&
                    mc::world::itemRegistry().item(inventory.selectedStack().item).tool ==
                        mc::world::ToolType::Spear &&
                    clicks.use && spearCharge < 100) {
                    ++spearCharge;
                    const mc::world::MobData* steed = ridingCart ? findCart() : nullptr;
                    const glm::dvec3 v = steed ? steed->vel : player.velocity();
                    const double speed = glm::length(glm::dvec2(v.x, v.z));
                    const float dmg = mc::spearChargeDamage(
                        mc::world::itemRegistry().item(inventory.selectedStack().item).tier, speed);
                    if (dmg > 0.0f)
                        if (const auto mh =
                                mc::Mobs::raycast(world, eye, look, mc::kSpearReach, ridingCart);
                            mh && (!lastHit || mh->distance < lastHit->distance)) {
                            auto& m = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                            if (m.hurtTime == 0 && m.health > 0.0f &&
                                !mc::world::isHanging(m.type)) {
                                mc::Mobs::attack(m, dmg, player.position());
                                m.vel += glm::dvec3(look.x, 0.2, look.z) * 0.6; // (knocked along)
                                stats.add(mc::world::Stat::DamageDealt, std::lround(dmg * 10.0f));
                                playSound(mc::world::Sound::AttackHit, m.pos, 1.0f, 0.8f, true);
                                if (survival)
                                    inventory.setSlot(
                                        inventory.selected(),
                                        mc::wearItem(inventory.selectedStack(), 1, gameRng));
                            }
                        }
                    clicks.useClick = false;
                } else if (!clicks.use) {
                    spearCharge = 0;
                }
                // Crossbows (M28.4a; wiki: Crossbow): hold right-click to load (an arrow), then
                // a right-click fires; letting go before it is loaded starts over.
                if (!dead && heldId == "minecraft:crossbow") {
                    static const mc::world::ItemId arrowItem =
                        *mc::world::itemRegistry().find("arrow");
                    if (inventory.selectedStack().state == 0) {
                        if (clicks.use && mc::canLoadCrossbow(inventory, survival)) {
                            if (++crossbowTicks >=
                                    mc::crossbowChargeTicks(inventory.selectedStack()) &&
                                mc::loadCrossbow(inventory, survival)) {
                                crossbowTicks = 0;
                                playSound(mc::world::Sound::DoorOpen, eye, 0.5f, 1.4f,
                                          false); // (our click as it locks)
                            }
                            clicks.useClick = false;
                        } else {
                            crossbowTicks = 0;
                        }
                    } else if (clicks.useClick) {
                        if (mc::fireCrossbow(inventory, survival, eye, look, projectiles,
                                             gameRng)) {
                            playSound(mc::world::Sound::BowShoot, eye, 1.0f, 1.1f, false);
                            announce(adv.onEvent(mc::world::AdvEvent::ShotCrossbow));
                        }
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                } else {
                    crossbowTicks = 0;
                }
                // Tridents (M25.3): hold right-click, release to throw - or, with Riptide,
                // to fly along the look when in water or rain.
                if (!dead && heldId == "minecraft:trident" && clicks.use) {
                    ++tridentTicks;
                    clicks.useClick = false;
                } else if (tridentTicks > 0) {
                    if (!dead && heldId == "minecraft:trident") {
                        const mc::world::BlockPos at{int(std::floor(eye.x)), int(std::floor(eye.y)),
                                                     int(std::floor(eye.z))};
                        const bool wet =
                            player.inWater() || (dimension == Dimension::Overworld &&
                                                 mc::world::rainingAt(world, weather, at));
                        const bool throws =
                            tridentTicks >= 10 &&
                            mc::world::enchantLevel(inventory.selectedStack(),
                                                    mc::world::Enchantment::Riptide) == 0;
                        const double launch =
                            mc::releaseTrident(inventory, tridentTicks, survival, wet, eye, look,
                                               projectiles, gameRng);
                        if (throws) announce(adv.onEvent(mc::world::AdvEvent::ThrewTrident));
                        if (launch > 0.0) player.setVelocity(player.velocity() + look * launch);
                        playSound(mc::world::Sound::BowShoot, eye, 1.0f, 0.6f, false);
                    }
                    tridentTicks = 0;
                }
                // Eyes of ender (M18.5) fly toward the nearest stronghold - in the
                // Overworld, unless aimed at an end portal frame (that fills it).
                if (!dead && heldId == "minecraft:ender_eye" && clicks.useClick &&
                    dimension == Dimension::Overworld &&
                    !(lastHit && mc::world::blockRegistry().blockOf(world.getBlock(
                                     lastHit->block)) == mc::world::blocks::EndPortalFrame))
                    if (const auto s = generatorPtr->nearestStronghold(eye.x, eye.z)) {
                        mc::throwEye(inventory, survival, eye, *s, projectiles);
                        clicks.useClick = false;
                    }
                // The debug stick (M29.7; wiki: Debug Stick, creative): left-click picks the next of
                // the block's properties, right-click steps its value (sneaking: back).
                if (!dead && gameMode == 1 && heldId == "minecraft:debug_stick" && lastHit &&
                    (clicks.useClick || clicks.attackClick)) {
                    const mc::world::BlockStateId ds = world.getBlock(lastHit->block);
                    const auto& props = reg.block(reg.blockOf(ds)).properties;
                    static std::vector<uint8_t> chosen(reg.blockCount(), 0); // (per block, as vanilla's item remembers)
                    uint8_t& pick = chosen[reg.blockOf(ds)];
                    char msg[96];
                    if (props.empty()) {
                        std::snprintf(msg, sizeof(msg), "%s has no properties", reg.block(reg.blockOf(ds)).id.c_str());
                    } else if (clicks.attackClick) {
                        pick = uint8_t((pick + 1) % props.size());
                        const auto v = reg.value(ds, props[pick]->name);
                        std::snprintf(msg, sizeof(msg), "selected \"%.*s\" (%.*s)", int(props[pick]->name.size()),
                                      props[pick]->name.data(), int(v ? v->size() : 0), v ? v->data() : "");
                    } else {
                        const mc::world::Property& pr = *props[pick % props.size()];
                        const int n = int(pr.values.size());
                        const int cur = reg.get(ds, pr);
                        const mc::world::BlockStateId next =
                            reg.set(ds, pr, (cur + (player.sneaking() ? n - 1 : 1)) % n);
                        world.setBlock(lastHit->block, next); // (no updates, as vanilla's stick)
                        frameEdits.push_back(lastHit->block);
                        const auto v = reg.value(next, pr.name);
                        std::snprintf(msg, sizeof(msg), "\"%.*s\" to %.*s", int(pr.name.size()), pr.name.data(),
                                      int(v ? v->size() : 0), v ? v->data() : "");
                    }
                    chat.addMessage(msg, 0xFFFFFFFFu, gameTime, gui.batch());
                    clicks.useClick = clicks.attackClick = clicks.attack = false;
                }
                if (!dead && (clicks.useClick || clicks.attackClick) && lastHit &&
                    reg.blockOf(world.getBlock(lastHit->block)) == mc::world::blocks::DragonEgg) {
                    // The egg teleports away when clicked (wiki: Dragon Egg).
                    mc::DragonFight::teleportEgg(world, lastHit->block, gameRng, frameEdits);
                    clicks.useClick = clicks.attackClick = clicks.attack = false;
                }
                int cartKind = -1; // (M29.3e: which minecart item - kCartKinds)
                for (int k = 0; k < 6; ++k)
                    if (heldId.size() > 10 && heldId.substr(10) == mc::world::kCartKinds[k]) cartKind = k;
                if (!dead && cartKind >= 0 && clicks.useClick && lastHit) { // (M21.4)
                    if (mc::Mobs::placeMinecart(world, lastHit->block, gameRng, cartKind) && survival)
                        inventory.consumeSelected(1);
                    if (mc::world::isRail(reg.blockOf(world.getBlock(lastHit->block))))
                        clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:end_crystal" && clicks.useClick && lastHit &&
                    lastHit->face == mc::world::Direction::Up) { // (M20.1)
                    if (mc::Mobs::placeEndCrystal(world, lastHit->block, gameRng) && survival)
                        inventory.consumeSelected(1);
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:ender_pearl" && clicks.useClick) { // (M20.3)
                    if (pearlCooldown == 0) {
                        mc::throwPearl(inventory, survival, eye, player.yaw(), player.pitch(),
                                       projectiles, gameRng);
                        pearlCooldown = 20; // (wiki: 1 s between throws)
                    }
                    clicks.useClick = false;
                }
                if (!dead &&
                    (heldId == "minecraft:splash_potion" ||
                     heldId == "minecraft:lingering_potion") &&
                    clicks.useClick) { // (M19.4; M28.4b lingering)
                    mc::throwSplashPotion(inventory, survival, eye, player.yaw(), player.pitch(),
                                          projectiles, gameRng);
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:wind_charge" && clicks.useClick &&
                    windCooldown == 0) {
                    mc::throwWindCharge(inventory, survival, eye, look, projectiles,
                                        gameRng); // (M26.4c)
                    windCooldown = 10;            // (wiki: half a second between throws)
                    clicks.useClick = false;
                }
                if (!dead &&
                    (heldId == "minecraft:writable_book" || heldId == "minecraft:written_book") &&
                    clicks.useClick && !bookScreen.isOpen()) { // (M28.2c) open the book
                    const mc::world::ItemStack& b = inventory.selectedStack();
                    const mc::world::BookContent content =
                        mc::world::bookContent(b.extra).value_or(mc::world::BookContent{});
                    if (heldId == "minecraft:writable_book")
                        bookScreen.openEdit(content, inventory.selected());
                    else
                        bookScreen.openRead(content);
                    window.setCursorCaptured(false);
                    clicks.useClick = false;
                    clicks.use = false;
                }
                if (!dead && mayBuild && clicks.useClick && lastHit &&
                    (heldId == "minecraft:item_frame" || heldId == "minecraft:glow_item_frame" ||
                     heldId == "minecraft:painting") &&
                    !decorInFront) {
                    // Hanging an item frame or a painting on the clicked face (M28.3a).
                    const auto type = heldId == "minecraft:painting" ? mc::world::MobType::Painting
                                      : heldId == "minecraft:glow_item_frame"
                                          ? mc::world::MobType::GlowItemFrame
                                          : mc::world::MobType::ItemFrame;
                    if (mc::Mobs::placeHanging(world, type, lastHit->block, lastHit->face,
                                               gameRng) &&
                        survival)
                        inventory.consumeSelected(1);
                    clicks.useClick = false;
                    clicks.use = false;
                }
                // A named name tag on a mob names it (M29.3b; wiki: Name Tag): it keeps the
                // name, shows it and no longer despawns. (Unnamed tags do nothing.)
                if (!dead && clicks.useClick && heldId == "minecraft:name_tag" && inventory.selectedStack().name != 0)
                    if (const auto mh = mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                        mh && (!lastHit || mh->distance < lastHit->distance)) {
                        mc::world::MobData& target = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                        if (target.type != mc::world::MobType::EnderDragon && !mc::world::isHanging(target.type) &&
                            target.nameId != inventory.selectedStack().name) {
                            target.nameId = inventory.selectedStack().name;
                            target.persistent = true;
                            world.chunk(mh->chunk)->markDirty();
                            if (survival) inventory.consumeSelected(1);
                        }
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                // Spawn eggs (M29.1e; wiki: Spawn Egg): on a mob of its kind, a baby; on a
                // spawner, the spawner's mob; else the mob stands on the clicked face.
                if (const uint8_t eggOf = mc::world::itemRegistry().item(inventory.selectedStack().item).spawnEgg;
                    !dead && clicks.useClick && eggOf != 0) {
                    const auto type = mc::world::MobType(eggOf - 1);
                    bool used = false;
                    if (const auto mh = mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                        mh && (!lastHit || mh->distance < lastHit->distance)) {
                        const mc::world::MobData& target = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                        if (target.type == type && !mc::world::mobInfo(type).hostile) {
                            mc::world::MobData baby = mc::Mobs::make(type, target.pos, gameRng);
                            baby.age = -24000;
                            baby.woolColour = target.woolColour;
                            baby.color2 = target.color2;
                            used = mc::Mobs::add(world, baby);
                        }
                    } else if (lastHit && mayBuild) {
                        const mc::world::BlockPos b = lastHit->block;
                        if (mc::world::Chunk* c = world.chunk(b.chunk());
                            c && mc::world::blockRegistry().blockOf(world.getBlock(b)) == mc::world::blocks::Spawner) {
                            if (mc::world::SpawnerData* sp =
                                    c->spawner(mc::world::blockToLocal(b.x), b.y, mc::world::blockToLocal(b.z))) {
                                sp->mob = type;
                                c->markDirty();
                                used = true;
                            }
                        } else {
                            const glm::ivec3 n = mc::world::kDirectionNormals[int(lastHit->face)];
                            const glm::dvec3 at(b.x + n.x + 0.5, b.y + n.y, b.z + n.z + 0.5);
                            mc::world::MobData mob = mc::Mobs::make(type, at, gameRng);
                            if (inventory.selectedStack().name != 0) { // (a renamed egg names its mob)
                                mob.nameId = inventory.selectedStack().name;
                                mob.persistent = true;
                            }
                            used = mc::Mobs::add(world, mob);
                        }
                    }
                    if (used && survival) inventory.consumeSelected(1);
                    clicks.useClick = false;
                    clicks.use = false;
                }
                if (!dead && mayBuild && clicks.useClick && lastHit &&
                    heldId == "minecraft:armor_stand") {
                    const glm::ivec3 n = mc::world::kDirectionNormals[int(lastHit->face)];
                    const mc::world::BlockPos cell{lastHit->block.x + n.x, lastHit->block.y + n.y,
                                                   lastHit->block.z + n.z};
                    if (mc::Mobs::placeArmorStand(world, cell, player.yaw(), gameRng) && survival)
                        inventory.consumeSelected(1);
                    clicks.useClick = false;
                    clicks.use = false;
                }
                if (!dead && heldId == "minecraft:firework_rocket" && clicks.useClick &&
                    (player.gliding() || lastHit)) {
                    // (M28.4c; wiki: Firework Rocket) used on a block it flies up from there;
                    // in the air while gliding it pushes the elytra flight.
                    const mc::world::ItemStack rocket = inventory.selectedStack();
                    bool used = false;
                    if (player.gliding()) {
                        const int flight = mc::world::fireworks(rocket.extra)
                                               .value_or(mc::world::Fireworks{})
                                               .flight;
                        elytraBoost =
                            mc::rocketLifetime(flight, gameRng.nextInt(6), gameRng.nextInt(7));
                        used = true;
                    } else {
                        const glm::dvec3 n(mc::world::kDirectionNormals[int(lastHit->face)]);
                        const glm::dvec3 at = eye + look * lastHit->distance + n * 0.15;
                        used = projectiles.launchFirework(at, rocket, false, glm::dvec3(0, 1, 0),
                                                          gameRng);
                    }
                    if (used) {
                        if (survival) inventory.consumeSelected(1);
                        playSound(mc::world::Sound::BowShoot, eye, 0.8f, 0.6f,
                                  false); // (our launch whoosh)
                    }
                    clicks.useClick = false;
                    clicks.use = false;
                }
                if (!dead && heldId == "minecraft:map" && clicks.useClick) {
                    // An empty map becomes a map of where it's used (M28.2b; wiki: Map):
                    // scale 0, centred on the 128-block grid cell around the player.
                    mc::world::ItemStack filled{*mc::world::itemRegistry().find("filled_map"), 1};
                    filled.damage = uint16_t(maps.create(int(std::floor(player.position().x)),
                                                         int(std::floor(player.position().z)), 0,
                                                         uint8_t(dimension)));
                    if (inventory.selectedStack().count == 1 && survival) {
                        inventory.setSlot(inventory.selected(), filled);
                    } else {
                        if (survival) inventory.consumeSelected(1);
                        if (inventory.add(filled) > 0)
                            droppedItems.spawn(player.position(), filled, gameRng);
                    }
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:snowball" && clicks.useClick) { // (M26.5b)
                    mc::throwSnowball(inventory, survival, eye, look, projectiles, gameRng);
                    clicks.useClick = false;
                }
                // Riding a pig or strider (M29.3d), its stick used: a boost (7-42 s), wearing the
                // stick (carrot 7, fungus 1).
                if (!dead && clicks.useClick && ridingCart != 0 &&
                    (heldId == "minecraft:carrot_on_a_stick" || heldId == "minecraft:warped_fungus_on_a_stick"))
                    if (mc::world::MobData* steed = findCart(); steed && mc::world::isStickRidden(steed->type)) {
                        const bool carrot = heldId == "minecraft:carrot_on_a_stick";
                        if (carrot == (steed->type == mc::world::MobType::Pig) && steed->spellTicks == 0) {
                            steed->spellTicks = int16_t(140 + gameRng.nextInt(701));
                            if (survival) { // (worn out, it is a fishing rod again - wiki)
                                const mc::world::ItemStack worn = mc::wearItem(inventory.selectedStack(), carrot ? 7 : 1, gameRng);
                                inventory.setSlot(inventory.selected(),
                                                  worn.empty() ? mc::world::ItemStack{*mc::world::itemRegistry().find("fishing_rod"), 1}
                                                               : worn);
                            }
                        }
                        clicks.useClick = false;
                    }
                // A bundle used in the hand (M29.3f; wiki: Bundle) throws its items out, last first.
                if (!dead && clicks.useClick && (heldId == "minecraft:bundle" || heldId.ends_with("_bundle")) &&
                    inventory.selectedStack().contents != 0) {
                    mc::world::ItemStack bag = inventory.selectedStack();
                    for (mc::world::ItemStack out = mc::takeFromBundle(bag); !out.empty(); out = mc::takeFromBundle(bag))
                        droppedItems.throwFrom(eye, look, out, gameRng);
                    bag.contents = 0;
                    inventory.setSlot(inventory.selected(), bag);
                    clicks.useClick = false;
                }
                // A lily pad goes on the water looked at (M29.4a; wiki: Lily Pad - on a still
                // water source with air above).
                if (!dead && mayBuild && clicks.useClick && heldId == "minecraft:lily_pad")
                    if (const auto wet = mc::world::raycastBlocks(world, eye, look, survival ? 4.5 : 5.0,
                                                                  mc::world::RayFluids::Sources);
                        wet && world.getBlock(wet->block) == reg.defaultState(mc::world::blocks::Water) &&
                        world.getBlock({wet->block.x, wet->block.y + 1, wet->block.z}) == 0) {
                        const mc::world::BlockPos on{wet->block.x, wet->block.y + 1, wet->block.z};
                        world.updateBlock(on, reg.defaultState(mc::world::blocks::LilyPad));
                        frameEdits.push_back(on);
                        if (survival) inventory.consumeSelected(1);
                        clicks.useClick = false;
                    }
                // Cocoa beans go on the side of a jungle log (M29.4b; wiki: Cocoa Beans), the pod
                // facing back toward the log.
                if (!dead && mayBuild && clicks.useClick && heldId == "minecraft:cocoa_beans")
                    if (const auto log = mc::world::raycastBlocks(world, eye, look, survival ? 4.5 : 5.0);
                        log && log->face != mc::world::Direction::Up && log->face != mc::world::Direction::Down &&
                        mc::world::BlockUpdates::jungleLog(reg.blockOf(world.getBlock(log->block)))) {
                        const mc::world::BlockPos on = mc::world::neighbour(log->block, log->face);
                        if (world.getBlock(on) == 0) {
                            const int towardLog = (int(log->face) ^ 1) - 2; // the opposite side: north, south, west, east
                            world.updateBlock(on, reg.set(reg.defaultState(mc::world::blocks::Cocoa),
                                                          mc::world::properties::facing, towardLog));
                            frameEdits.push_back(on);
                            if (survival) inventory.consumeSelected(1);
                        }
                        clicks.useClick = false;
                    }
                // A spyglass held up (M29.3c; wiki: Spyglass): the view zooms to a tenth.
                spyglassUp = !dead && heldId == "minecraft:spyglass" && clicks.use;
                if (spyglassUp) clicks.useClick = false;
                if (!dead && heldId == "minecraft:experience_bottle" && clicks.useClick) { // (M29.3c)
                    mc::throwExperienceBottle(inventory, survival, eye, look, projectiles, gameRng);
                    clicks.useClick = false;
                }
                if (!dead && (heldId == "minecraft:egg" || heldId == "minecraft:blue_egg" || heldId == "minecraft:brown_egg") &&
                    clicks.useClick) {
                    mc::throwEgg(inventory, survival, eye, look, projectiles, gameRng);
                    clicks.useClick = false;
                }
                if (!dead && clicks.useClick &&
                    (heldId.ends_with("_boat") || heldId == "minecraft:bamboo_raft" ||
                     heldId == "minecraft:bamboo_chest_raft")) {
                    // Boats (M25.2b; wiki: Boat): on the water surface in view, else on the
                    // block clicked, facing the way the player looks.
                    const double reach =
                        survival ? mc::world::kSurvivalReach : mc::world::kCreativeReach;
                    const auto hit = mc::world::raycastBlocks(world, eye, look, reach,
                                                              mc::world::RayFluids::Sources);
                    if (hit) {
                        const bool water =
                            mc::world::blockRegistry().blockOf(world.getBlock(hit->block)) ==
                            mc::world::blocks::Water;
                        const mc::world::BlockPos at =
                            water ? hit->block : mc::world::neighbour(hit->block, hit->face);
                        int wood = 0;
                        bool chest = false; // (M26.2: a chest boat)
                        for (int w = 0; w < 10; ++w) {
                            if (mc::world::boatId(w) == heldId) wood = w;
                            if (mc::world::chestBoatId(w) == heldId) wood = w, chest = true;
                        }
                        if (mc::Mobs::placeBoat(
                                world, {at.x + 0.5, at.y + (water ? 0.8 : 0.0), at.z + 0.5},
                                player.yaw(), wood, gameRng, chest) &&
                            survival)
                            inventory.consumeSelected(1);
                    }
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:goat_horn" && clicks.useClick) {
                    // A goat horn (M26.3; wiki: Goat Horn): its call, heard 256 blocks away,
                    // then 7 s before it can sound again.
                    if (hornCooldown == 0) {
                        const mc::world::ItemStack horn = inventory.selectedStack();
                        world.playSound(static_cast<mc::world::Sound>(
                                            int(mc::world::Sound::GoatHorn0) + horn.damage % 8),
                                        player.position().x, player.position().y + 1.5,
                                        player.position().z, 16.0f);
                        hornCooldown = 140;
                    }
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:fishing_rod" &&
                    clicks.useClick) { // (M25.2: cast / reel in)
                    const mc::world::ItemStack rod = inventory.selectedStack();
                    if (fishing.active()) {
                        if (fishing.biting()) {
                            stats.add(mc::world::Stat::FishCaught);
                            announce(adv.onEvent(mc::world::AdvEvent::Fished));
                        }
                        const int wear =
                            fishing.reel(world, player.position(), droppedItems, &orbs, gameRng);
                        if (survival && wear > 0)
                            inventory.setSlot(inventory.selected(),
                                              mc::wearItem(rod, wear, gameRng));
                    } else {
                        fishing.cast( // (M29.2a: the Luck effect adds to Luck of the Sea, Unluck takes away)
                            eye, look, mc::world::enchantLevel(rod, mc::world::Enchantment::Lure),
                            std::max(0, mc::world::enchantLevel(rod, mc::world::Enchantment::LuckOfTheSea) +
                                            vitals.effectLevel(mc::world::Effect::Luck) -
                                            vitals.effectLevel(mc::world::Effect::Unluck)),
                            gameRng);
                        playSound(mc::world::Sound::BowShoot, eye, 0.5f, 0.4f,
                                  true); // (the cast's whoosh)
                    }
                    clicks.useClick = false;
                }
            }
            // Signs (M23.3c): a dye recolours the text, otherwise right-click edits it.
            if (!dead && clicks.useClick && lastHit && !player.sneaking()) {
                const auto st = world.getBlock(lastHit->block);
                const auto kind =
                    mc::world::blockRegistry().kind(mc::world::blockRegistry().blockOf(st));
                if (kind == mc::world::BlockKind::Sign || kind == mc::world::BlockKind::WallSign ||
                    kind == mc::world::BlockKind::HangingSign ||
                    kind == mc::world::BlockKind::WallHangingSign) {
                    mc::world::Chunk* sc = world.chunk(lastHit->block.chunk());
                    mc::world::SignData* sd =
                        sc ? sc->sign(mc::world::blockToLocal(lastHit->block.x), lastHit->block.y,
                                      mc::world::blockToLocal(lastHit->block.z))
                           : nullptr;
                    if (sd && !sd->waxed) {
                        const std::string_view held =
                            mc::world::itemRegistry().item(inventory.selectedStack().item).id;
                        int dye = -1;
                        for (int c = 0; c < 16; ++c)
                            if (held.size() > 10 &&
                                held.substr(10) == std::string(mc::world::kDyeColours[c]) + "_dye")
                                dye = c;
                        if (dye >= 0) {
                            if (sd->front.colour != dye) {
                                sd->front.colour = uint8_t(dye);
                                sc->markDirty();
                                if (survival) inventory.consumeSelected(1);
                            }
                        } else if (!screenshotMode) {
                            signEditor.open(lastHit->block, sd->front);
                            window.setCursorCaptured(false);
                        }
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            // Composters and cauldrons (M23.5; wiki: Composter, Cauldron): a full composter
            // gives its bone meal, compostables go in; buckets and bottles fill or empty a
            // cauldron. Survival swaps one held item for the result; creative keeps it and
            // gets the result once.
            if (!dead && clicks.useClick && lastHit && !player.sneaking()) {
                const mc::world::BlockPos at = lastHit->block;
                const mc::world::BlockId hb = reg.blockOf(world.getBlock(at));
                const mc::world::ItemStack held = inventory.selectedStack();
                bool acted = false;
                if (hb ==
                    mc::world::blocks::Jukebox) { // (M23.6) eject the disc, or put the held one in
                    mc::world::Chunk* jc = world.chunk(at.chunk());
                    mc::world::JukeboxData* jd =
                        jc ? jc->jukebox(mc::world::blockToLocal(at.x), at.y,
                                         mc::world::blockToLocal(at.z))
                           : nullptr;
                    if (jd && !jd->record.empty()) {
                        droppedItems.spawn({at.x + 0.5, at.y + 1.05, at.z + 0.5}, jd->record,
                                           gameRng);
                        jd->record = {};
                        jd->playing = false;
                        world.updateBlock(
                            at, reg.set(world.getBlock(at), mc::world::properties::hasRecord, 1));
                        jc->markDirty();
                        blockUpdates.jukeboxChanged(at);
                        acted = true;
                    } else if (jd && mc::discIndex(held.item) >= 0) {
                        jd->record = held;
                        jd->record.count = 1;
                        jd->ticks = 0;
                        jd->playing = true;
                        if (survival) inventory.consumeSelected(1);
                        world.updateBlock(
                            at, reg.set(world.getBlock(at), mc::world::properties::hasRecord, 0));
                        jc->markDirty();
                        blockUpdates.jukeboxChanged(at);
                        acted = true;
                    }
                } else if ((hb == mc::world::blocks::BeeNest || hb == mc::world::blocks::Beehive) &&
                           !held.empty() &&
                           reg.get(world.getBlock(at), mc::world::properties::honeyLevel) == 5 &&
                           (mc::world::itemRegistry().item(held.item).id == "minecraft:shears" ||
                            mc::world::itemRegistry().item(held.item).id ==
                                "minecraft:glass_bottle")) {
                    // Harvesting a full hive (M26.3b; wiki: Beehive): shears cut 3 honeycombs,
                    // a bottle fills with honey. Without a campfire's smoke below, the
                    // bees inside come out angry.
                    const bool shearing =
                        mc::world::itemRegistry().item(held.item).id == "minecraft:shears";
                    if (shearing) {
                        droppedItems.spawn({at.x + 0.5, at.y + 1.05, at.z + 0.5},
                                           {*mc::world::itemRegistry().find("honeycomb"), 3},
                                           gameRng);
                        if (survival)
                            inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                    } else {
                        if (survival) inventory.consumeSelected(1);
                        const mc::world::ItemStack honey{
                            *mc::world::itemRegistry().find("honey_bottle"), 1};
                        if (inventory.add(honey) > 0)
                            droppedItems.spawn(player.position(), honey, gameRng);
                    }
                    world.updateBlock(
                        at, reg.set(world.getBlock(at), mc::world::properties::honeyLevel, 0));
                    frameEdits.push_back(at);
                    if (!mc::world::hiveSmoked(world, at))
                        if (mc::world::Chunk* hc = world.chunk(at.chunk()))
                            if (mc::world::BeehiveData* hd =
                                    hc->beehive(mc::world::blockToLocal(at.x), at.y,
                                                mc::world::blockToLocal(at.z))) {
                                mc::world::releaseBees(world, at, *hd, true);
                                world.markTicking(at.chunk());
                            }
                    acted = true;
                } else if (hb == mc::world::blocks::SweetBerryBush ||
                           hb == mc::world::blocks::CaveVines ||
                           hb == mc::world::blocks::CaveVinesPlant) { // (M26.3, M27.2) picking
                                                                      // berries
                    if (const int n = mc::world::BlockUpdates::pickBerries(world, at, gameRng);
                        n > 0) {
                        droppedItems.spawn(
                            {at.x + 0.5, at.y + 0.5, at.z + 0.5},
                            {*mc::world::itemRegistry().find(hb == mc::world::blocks::SweetBerryBush
                                                                 ? "sweet_berries"
                                                                 : "glow_berries"),
                             uint8_t(n)},
                            gameRng);
                        frameEdits.push_back(at);
                        acted = true;
                    }
                } else if (hb == mc::world::blocks::Vault) { // (M27.4d; wiki: Vault - a trial key
                                                             // opens it once)
                    const mc::world::BlockStateId vs = world.getBlock(at);
                    const bool ominousVault =
                        reg.get(vs, mc::world::properties::ominous) == 0; // (M28.4d)
                    if (reg.get(vs, mc::world::properties::vaultState) == 1 &&
                        held.item == *mc::world::itemRegistry().find(
                                         ominousVault ? "ominous_trial_key" : "trial_key")) {
                        if (survival) inventory.consumeSelected(1);
                        std::array<mc::world::ItemStack, 27> loot{};
                        mc::world::fillChest(ominousVault ? mc::world::LootTable::TrialVaultOminous
                                                          : mc::world::LootTable::TrialVault,
                                             gameRng, loot);
                        for (const auto& it : loot)
                            if (!it.empty())
                                droppedItems.spawn({at.x + 0.5, at.y + 1.1, at.z + 0.5}, it,
                                                   gameRng);
                        world.updateBlock(at, reg.set(vs, mc::world::properties::vaultState,
                                                      0)); // (spent: ours for good)
                        announce(adv.onEvent(ominousVault ? mc::world::AdvEvent::OpenedOminousVault
                                                          : mc::world::AdvEvent::OpenedVault));
                        frameEdits.push_back(at);
                        acted = true;
                    }
                } else if (hb == mc::world::blocks::Composter) {
                    if (const mc::world::ItemStack meal = blockUpdates.takeCompost(at);
                        !meal.empty()) {
                        droppedItems.spawn({at.x + 0.5, at.y + 1.05, at.z + 0.5}, meal, gameRng);
                        acted = true;
                    } else if (!held.empty() && blockUpdates.compost(at, held.item)) {
                        if (survival) inventory.consumeSelected(1);
                        acted = true;
                    }
                } else if (const auto give = blockUpdates.useCauldron(at, held)) {
                    if (survival && held.count == 1) {
                        inventory.setSlot(inventory.selected(), *give);
                    } else {
                        bool carried = false;
                        for (int sl = 0; sl < mc::Inventory::kSlots && !survival; ++sl)
                            carried = carried || (inventory.slot(sl).item == give->item &&
                                                  inventory.slot(sl).potion == give->potion);
                        if (survival) inventory.consumeSelected(1);
                        if (!carried && inventory.add(*give) > 0)
                            droppedItems.spawn(player.position() + glm::dvec3(0, 1, 0), *give,
                                               gameRng);
                    }
                    acted = true;
                }
                if (acted) {
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Hoes and bone meal (M17.1; wiki: Hoe, Bone Meal) on the targeted block.
            if (!dead && clicks.useClick && lastHit && mayBuild &&
                !inventory.selectedStack().empty()) {
                const mc::world::ItemStack held = inventory.selectedStack();
                const auto& def = mc::world::itemRegistry().item(held.item);
                static const mc::world::ItemId boneMealItem =
                    *mc::world::itemRegistry().find("bone_meal");
                static const mc::world::ItemId honeycombItem =
                    *mc::world::itemRegistry().find("honeycomb");
                static const mc::world::ItemId shearsItem =
                    *mc::world::itemRegistry().find("shears");
                if (held.item == shearsItem && !player.sneaking() &&
                    reg.blockOf(world.getBlock(lastHit->block)) == mc::world::blocks::Pumpkin) {
                    // Shears carve a pumpkin: the face toward the clicked side (or the
                    // player, from above or below) (wiki: Pumpkin › Carving; M24.3).
                    const auto face = lastHit->face;
                    const float yaw = std::fmod(std::fmod(player.yaw(), 360.0f) + 360.0f, 360.0f);
                    static constexpr const char* kToward[4] = {"north", "east", "south", "west"};
                    const char* facing = face == mc::world::Direction::North   ? "north"
                                         : face == mc::world::Direction::South ? "south"
                                         : face == mc::world::Direction::West  ? "west"
                                         : face == mc::world::Direction::East
                                             ? "east"
                                             : kToward[int(std::floor((yaw + 45.0f) / 90.0f)) % 4];
                    const auto carved = reg.with(reg.defaultState(mc::world::blocks::CarvedPumpkin),
                                                 "facing", facing);
                    world.updateBlock(lastHit->block, *carved);
                    frameEdits.push_back(lastHit->block);
                    if (survival)
                        inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                    clicks.useClick = false;
                    clicks.use = false;
                } else if (held.item == honeycombItem &&
                           mc::world::BlockUpdates::waxCopper(world, lastHit->block)) { // (M23.4b)
                    frameEdits.push_back(lastHit->block);
                    announce(adv.onEvent(mc::world::AdvEvent::Waxed));
                    if (survival) inventory.consumeSelected(1);
                    clicks.useClick = false;
                    clicks.use = false;
                } else if (def.tool == mc::world::ToolType::Axe && !player.sneaking() &&
                           mc::world::blockRegistry().blockOf(world.getBlock(lastHit->block)) ==
                               mc::world::blocks::CopperGolemStatue) {
                    // (M29.6; wiki: Copper Golem Statue) a bare statue scraped with an axe wakes
                    // up as a copper golem (aged or waxed ones are scraped like copper first).
                    world.updateBlock(lastHit->block, 0);
                    mc::Mobs::add(world, mc::Mobs::make(mc::world::MobType::CopperGolem,
                                                        {lastHit->block.x + 0.5, double(lastHit->block.y),
                                                         lastHit->block.z + 0.5},
                                                        gameRng));
                    frameEdits.push_back(lastHit->block);
                    if (survival) inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                    clicks.useClick = false;
                    clicks.use = false;
                } else if (def.tool == mc::world::ToolType::Axe && !player.sneaking() &&
                           mc::world::BlockUpdates::strip(world, lastHit->block)) { // (M23.3b)
                    frameEdits.push_back(lastHit->block);
                    if (survival)
                        inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                    clicks.useClick = false;
                    clicks.use = false;
                } else if (def.tool == mc::world::ToolType::Hoe &&
                           mc::world::BlockUpdates::till(world, lastHit->block, lastHit->face)) {
                    frameEdits.push_back(lastHit->block);
                    if (survival)
                        inventory.setSlot(inventory.selected(),
                                          mc::wearItem(held, 1, gameRng)); // 1 per block
                    clicks.useClick = false;
                    clicks.use = false;
                } else if (held.item == boneMealItem && blockUpdates.boneMeal(lastHit->block)) {
                    if (survival) inventory.consumeSelected(1);
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Trading (M24.2): right-click an adult villager with a profession and trades.
            if (!dead && clicks.useClick && !container.isOpen()) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh =
                        mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance)) {
                    auto& mob = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    if ((mob.type == mc::world::MobType::Villager ||
                         mob.type == mc::world::MobType::WanderingTrader) &&
                        !mob.isBaby() && !mob.sleeping && mob.offerCount > 0) {
                        traderUuid = mob.uuidHi;
                        mob.tradingTicks = 5;
                        container.openTrading(&mob);
                        window.setCursorCaptured(false);
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            // Minecart kinds (M29.3e): a chest or hopper cart opens on a right-click; a furnace
            // cart takes coal or charcoal (+3600 ticks of fuel, pushing away from the player).
            if (!dead && clicks.useClick) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh = mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance)) {
                    auto& cart = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    if (cart.type == mc::world::MobType::Minecart && cart.decor != 0) {
                        if (cart.decor == 5 && gameMode == 1) { // (M29.7) a command cart's command
                            auto& ms = shared.menuState;
                            ms.command = std::string(mc::world::nameText(cart.commandId));
                            ms.commandOutput.clear();
                            ms.commandMode = 0;
                            ms.commandConditional = ms.commandAlways = false;
                            ms.screen = mc::ui::MenuScreen::CommandBlock;
                            commandEditingCart = cart.uuidHi;
                            window.setCursorCaptured(false);
                        } else if (cart.hasChest) {
                            openMountScreen(cart.uuidHi);
                        } else if (const std::string_view fuelId = mc::world::itemRegistry().item(inventory.selectedStack().item).id;
                                   cart.decor == 2 && (fuelId == "minecraft:coal" || fuelId == "minecraft:charcoal")) {
                            cart.temper = int16_t(std::min(32000, cart.temper + 3600));
                            const glm::dvec3 away = cart.pos - player.position();
                            cart.home = std::abs(away.x) > std::abs(away.z) ? glm::ivec3(away.x > 0 ? 1 : -1, 0, 0)
                                                                            : glm::ivec3(0, 0, away.z > 0 ? 1 : -1);
                            if (survival) inventory.consumeSelected(1);
                            world.chunk(mh->chunk)->markDirty();
                        }
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            // A tamed mount's or a chest boat's screen (M26.2): sneak + right-click.
            if (!dead && clicks.useClick && player.sneaking()) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh =
                        mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance)) {
                    const auto& mob = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    if ((mc::world::isMount(mob.type) &&
                         (mob.tamed || mc::world::isCamel(mob.type)) && !mob.isBaby()) ||
                        (mob.type == mc::world::MobType::Boat && mob.hasChest)) {
                        openMountScreen(mob.uuidHi);
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            // Getting into a minecart (M21.4): right-click it (not sneaking).
            if (!dead && clicks.useClick && !player.sneaking() && ridingCart == 0) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh =
                        mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance)) {
                    auto& mob = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    if ((mob.type == mc::world::MobType::Minecart ||
                         mob.type == mc::world::MobType::Boat) &&
                        !mob.ridden) {
                        mob.ridden = true;
                        ridingCart = mob.uuidHi;
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            if (decorClick) clicks.useClick = true; // (for the decorations and leads below)
            // Right-click on an item frame: put the held item in, or turn it (M28.3a).
            if (!dead && clicks.useClick) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh =
                        mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance) &&
                    (mc::world::isHanging(world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type) ||
                     world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type ==
                         mc::world::MobType::ArmorStand)) {
                    auto& m = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    if (m.type == mc::world::MobType::ArmorStand) { // (M28.3b) dressing it
                        mc::world::ItemStack held = inventory.selectedStack();
                        // (creative keeps what it puts on, but an empty hand takes the piece)
                        const bool emptyHand = held.empty();
                        if (gameMode != 2 &&
                            mc::Mobs::useArmorStand(world, m, held,
                                                    eye.y + look.y * mh->distance) &&
                            (survival || emptyHand))
                            inventory.setSlot(inventory.selected(), held);
                    } else if (mc::Mobs::useItemFrame(world, m, inventory.selectedStack()) &&
                               survival) {
                        inventory.consumeSelected(1);
                    }
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Leads (M28.3c; wiki: Lead): a lead on a mob in front puts it on the player's lead;
            // clicking a mob already on it lets it go (the lead drops); clicking a knot ties the
            // player's mobs to it or takes its mobs back; clicking a fence ties them there.
            if (!dead && clicks.useClick) {
                static const mc::world::ItemId leadItem = *mc::world::itemRegistry().find("lead");
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                const auto mh =
                    mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                bool used = false;
                if (mh && (!lastHit || mh->distance < lastHit->distance)) {
                    mc::world::Chunk* lc = world.chunk(mh->chunk);
                    auto& m = lc->mobs()[size_t(mh->index)];
                    if (m.type == mc::world::MobType::LeashKnot) {
                        if (mc::Mobs::tieToFence(world, {m.home.x, m.home.y, m.home.z},
                                                 player.position(), gameRng) == 0)
                            mc::Mobs::takeFromKnot(world, m);
                        used = true;
                    } else if (m.leash == 1) {
                        m.leash = 0;
                        droppedItems.spawn(m.pos + glm::dvec3(0.0, 0.5, 0.0), {leadItem, 1},
                                           gameRng);
                        used = true;
                    } else if (inventory.selectedStack().item == leadItem &&
                               mc::Mobs::leashToPlayer(m)) {
                        if (survival) inventory.consumeSelected(1);
                        used = true;
                    }
                    if (used) lc->markDirty();
                } else if (lastHit && !player.sneaking() &&
                           mc::Mobs::tieToFence(world, lastHit->block, player.position(), gameRng) >
                               0) {
                    used = true;
                }
                if (used) {
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Feeding and shearing animals (M16.3): right-click the mob in front.
            // (M26.1: an empty hand makes a tamed pet sit or stand)
            if (!dead && clicks.useClick) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh =
                        mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance) &&
                    (!inventory.selectedStack().empty() ||
                     (mc::world::isPet(world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type) &&
                      world.chunk(mh->chunk)->mobs()[size_t(mh->index)].tamed) ||
                     world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type ==
                         mc::world::MobType::Allay ||
                     mc::world::isMount(world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type) ||
                     mc::world::isStickRidden(world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type))) {
                    auto& mob = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    const mc::world::ItemStack held = inventory.selectedStack();
                    const bool wasTamed = mob.tamed;
                    auto use = mc::Mobs::interact(mob, held.item, gameRng, droppedItems);
                    if (!wasTamed && mob.tamed) announce(adv.onEvent(mc::world::AdvEvent::Tamed));
                    if (use == mc::Mobs::Use::Ride &&
                        (ridingCart != 0 || player.sneaking())) { // (one ride at a time)
                        mob.ridden = false;
                        use = mc::Mobs::Use::None;
                    }
                    if (use == mc::Mobs::Use::Ride) ridingCart = mob.uuidHi; // (M26.2: on a mount)
                    if (use != mc::Mobs::Use::None) {
                        world.chunk(mh->chunk)->markDirty();
                        if (survival && use == mc::Mobs::Use::Fed) {
                            inventory.consumeSelected(1);
                            if (mc::world::itemRegistry().item(held.item).id ==
                                "minecraft:tropical_fish_bucket") // (M26.3c: axolotls)
                                inventory.add({*mc::world::itemRegistry().find("water_bucket"), 1});
                        }
                        if (survival && use == mc::Mobs::Use::Sheared) // shears wear 1 per sheep
                            inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
                        if (use == mc::Mobs::Use::Stew) { // (M29.1c) a bowl filled at a mooshroom
                            const mc::world::ItemStack stew{*mc::world::itemRegistry().find("mushroom_stew"), 1};
                            if (survival && held.count == 1) {
                                inventory.setSlot(inventory.selected(), stew);
                            } else {
                                if (survival) inventory.consumeSelected(1);
                                if (inventory.add(stew) > 0) droppedItems.spawn(player.position(), stew, gameRng);
                            }
                        }
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            // Buckets (M14; wiki: Bucket): fill from a source or a cow, empty into the world.
            if (!dead && clicks.useClick && !inventory.selectedStack().empty()) {
                const mc::world::ItemStack held = inventory.selectedStack();
                const std::string_view heldId = mc::world::itemRegistry().item(held.item).id;
                // A usable block (lever, button...) takes the click first unless sneaking.
                const bool blockUse =
                    lastHit && !player.sneaking() &&
                    mc::world::BlockUpdates::usable(world.getBlock(lastHit->block));
                // A glass bottle in a cloud of dragon's breath bottles it (M28.4b; wiki: Dragon's
                // Breath).
                if (heldId == "minecraft:glass_bottle" && clicks.useClick && !blockUse)
                    for (const auto& c : projectiles.clouds()) {
                        const glm::dvec3 f = player.position();
                        if (c.potion != 0 ||
                            glm::length(glm::dvec2(f.x - c.pos.x, f.z - c.pos.z)) >
                                c.radius + 1.0 ||
                            std::abs(f.y - c.pos.y) > 2.0)
                            continue;
                        static const mc::world::ItemId breathItem =
                            *mc::world::itemRegistry().find("dragon_breath");
                        if (survival) inventory.consumeSelected(1);
                        if (inventory.add({breathItem, 1}) > 0)
                            droppedItems.spawn(player.position(), {breathItem, 1}, gameRng);
                        clicks.useClick = false;
                        break;
                    }
                // A glass bottle fills from water (wiki: Glass Bottle): a water bottle.
                if (heldId == "minecraft:glass_bottle" && clicks.useClick && !blockUse) {
                    const glm::dvec3 eye = player.eyePosition(1.0);
                    const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                    const double reach =
                        survival ? mc::world::kSurvivalReach : mc::world::kCreativeReach;
                    for (double t = 0.0; t <= reach; t += 0.1) {
                        const glm::dvec3 q = eye + look * t;
                        const mc::world::BlockPos b{int(std::floor(q.x)), int(std::floor(q.y)),
                                                    int(std::floor(q.z))};
                        const mc::world::BlockStateId s = world.getBlock(b);
                        if (mc::world::blockRegistry().blockOf(s) == mc::world::blocks::Water) {
                            static const mc::world::ItemId potionItem =
                                *mc::world::itemRegistry().find("potion");
                            mc::world::ItemStack water{potionItem, 1};
                            water.potion = static_cast<uint8_t>(mc::world::Potion::Water);
                            if (survival) inventory.consumeSelected(1);
                            if (inventory.add(water) > 0)
                                droppedItems.spawn(player.position(), water, gameRng);
                            clicks.useClick = false;
                            break;
                        }
                        if (mc::world::blockRegistry().collides(s)) break;
                    }
                }
                // Brushing (M27.5; wiki: Brush, Suspicious Sand): held on suspicious sand or
                // gravel it dusts the block a stage every 10 ticks; after the last, the
                // block gives up its item (out of the brushed face) and the brush wears.
                if (heldId == "minecraft:brush" && clicks.use && lastHit && !blockUse &&
                    mc::world::isSuspicious(reg.blockOf(world.getBlock(lastHit->block)))) {
                    clicks.useClick = false;
                    if (++brushTicks % 10 == 0) {
                        const mc::world::BlockPos bp = lastHit->block;
                        const mc::world::BlockStateId bs = world.getBlock(bp);
                        const int d = reg.get(bs, mc::world::properties::dusted);
                        if (d < 3) { // (a state change only: the renderer re-meshes it, no relight)
                            world.updateBlock(bp,
                                              reg.set(bs, mc::world::properties::dusted, d + 1));
                        } else {
                            mc::world::ItemStack found;
                            if (mc::world::Chunk* bc = world.chunk(bp.chunk()))
                                if (const mc::world::BrushableData* bd =
                                        bc->brushable(mc::world::blockToLocal(bp.x), bp.y,
                                                      mc::world::blockToLocal(bp.z))) {
                                    found = bd->item;
                                    if (found.empty() && bd->table != 255)
                                        found = mc::world::rollOne(mc::world::LootTable(bd->table),
                                                                   gameRng);
                                }
                            const glm::ivec3 n = mc::world::normal(lastHit->face);
                            if (!found.empty())
                                droppedItems.spawn({bp.x + 0.5 + n.x * 0.7, bp.y + 0.5 + n.y * 0.7,
                                                    bp.z + 0.5 + n.z * 0.7},
                                                   found, gameRng);
                            world.updateBlock(
                                bp, reg.defaultState(reg.blockOf(bs) ==
                                                             mc::world::blocks::SuspiciousSand
                                                         ? mc::world::blocks::Sand
                                                         : mc::world::blocks::Gravel));
                            if (survival)
                                inventory.setSlot(inventory.selected(),
                                                  mc::wearItem(held, 1, gameRng));
                            brushTicks = 0;
                            frameEdits.push_back(bp);
                        }
                    }
                } else if (!clicks.use) {
                    brushTicks = 0;
                }
                // A water bottle poured on dirt makes mud (M27.1; wiki: Mud).
                if (heldId == "minecraft:potion" &&
                    held.potion == static_cast<uint8_t>(mc::world::Potion::Water) &&
                    clicks.useClick && lastHit && !blockUse) {
                    const mc::world::BlockId hb =
                        mc::world::blockRegistry().blockOf(world.getBlock(lastHit->block));
                    if (hb == mc::world::blocks::Dirt || hb == mc::world::blocks::CoarseDirt) {
                        world.updateBlock(lastHit->block, mc::world::blockRegistry().defaultState(
                                                              mc::world::blocks::Mud));
                        frameEdits.push_back(lastHit->block);
                        if (survival) {
                            static const mc::world::ItemId bottle =
                                *mc::world::itemRegistry().find("glass_bottle");
                            inventory.setSlot(inventory.selected(), {bottle, 1});
                        }
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
                if (heldId.ends_with("bucket") && heldId != "minecraft:milk_bucket" && !blockUse) {
                    const glm::dvec3 eye = player.eyePosition(1.0);
                    const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                    const double reach =
                        survival ? mc::world::kSurvivalReach : mc::world::kCreativeReach;
                    std::optional<mc::BucketResult> result;
                    if (heldId == "minecraft:bucket")
                        if (const auto mh = mc::Mobs::raycast(world, eye, look,
                                                              survival ? 3.0 : 5.0, ridingCart);
                            mh &&
                            (!lastHit || mh->distance < lastHit->distance) && // not through walls
                            (world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type ==
                                 mc::world::MobType::Cow ||
                             world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type ==
                                 mc::world::MobType::Goat || // (M26.3: goats give milk too)
                             world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type ==
                                 mc::world::MobType::Mooshroom)) // (M29.1c)
                            result =
                                mc::BucketResult{*mc::world::itemRegistry().find("milk_bucket")};
                    // A water bucket scoops up a fish in front (M25.2): a bucket of that fish.
                    bool scooped = false;
                    if (heldId == "minecraft:water_bucket")
                        if (const auto mh = mc::Mobs::raycast(world, eye, look,
                                                              survival ? 3.0 : 5.0, ridingCart);
                            mh && (!lastHit || mh->distance < lastHit->distance)) {
                            auto& fish = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                            if (const mc::world::ItemId bucketItem = mc::fishBucketFor(fish.type)) {
                                fish.health = 0.0f;
                                fish.deathTime = 19; // (gone next tick: not a death)
                                fish.lastHurtByPlayer = false;
                                world.chunk(mh->chunk)->markDirty();
                                result = mc::BucketResult{bucketItem};
                                scooped = true;
                            }
                        }
                    if (!result && mayBuild)
                        result = mc::useBucket(world, held.item, eye, look, reach, frameEdits);
                    if (result && result->fish != mc::world::MobType::Count) { // its fish swims off
                        mc::world::MobData fish = mc::Mobs::make(
                            result->fish,
                            {result->at.x + 0.5, double(result->at.y) + 0.1, result->at.z + 0.5},
                            gameRng);
                        fish.fromBucket = true;
                        mc::Mobs::add(world, fish);
                    }
                    if (result) {
                        if (!result->washed.empty() && survival) {
                            const mc::world::BlockPos w = frameEdits.back();
                            droppedItems.spawn({w.x + 0.5, w.y + 0.25, w.z + 0.5}, result->washed,
                                               gameRng);
                        }
                        const mc::world::ItemStack extra =
                            mc::applyBucket(inventory, result->filled, survival || scooped);
                        if (!extra.empty())
                            droppedItems.spawn(player.position() + glm::dvec3(0, 1, 0), extra,
                                               gameRng);
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            // Portals (wiki: Nether portal - 4 s inside in survival, at once in creative;
            // End portal - at once). Arriving players step out before they can go back.
            if (!dead && !flatWorld && !pendingTravel && !arrival) {
                const mc::Aabb box = player.box();
                const mc::world::BlockPos portalFeet{
                    int(std::floor(feet.x)), int(std::floor(feet.y)), int(std::floor(feet.z))};
                if (mc::portals::touching(world, box, mc::world::blocks::EndPortal)) {
                    if (!portalCooldown)
                        pendingTravel = Travel{dimension == Dimension::End ? Dimension::Overworld
                                                                           : Dimension::End,
                                               dimension == Dimension::End ? Travel::Via::Respawn
                                                                           : Travel::Via::EndPortal,
                                               portalFeet};
                } else if (dimension != Dimension::End &&
                           mc::portals::touching(world, box, mc::world::blocks::NetherPortal)) {
                    if (!portalCooldown && ++portalTicks >= (survival ? 80 : 1))
                        pendingTravel = Travel{dimension == Dimension::Nether ? Dimension::Overworld
                                                                              : Dimension::Nether,
                                               Travel::Via::NetherPortal, portalFeet};
                } else {
                    portalTicks = 0;
                    portalCooldown = false;
                }
            }
            // Hitting an item frame takes its item out; then (or a painting) it breaks
            // (M28.3a; wiki: Item Frame, Painting). Creative breaks drop nothing.
            if (!dead && clicks.attackClick) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh =
                        mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance) &&
                    (mc::world::isHanging(world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type) ||
                     world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type ==
                         mc::world::MobType::ArmorStand ||
                     world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type ==
                         mc::world::MobType::LeashKnot)) {
                    mc::world::Chunk* hc = world.chunk(mh->chunk);
                    auto& m = hc->mobs()[size_t(mh->index)];
                    if (m.type ==
                        mc::world::MobType::LeashKnot) { // (M28.3c) the knot breaks, its leads drop
                        if (gameMode != 2 && m.health > 0.0f)
                            mc::Mobs::breakKnot(world, m, droppedItems, gameRng);
                    } else if (m.type ==
                               mc::world::MobType::ArmorStand) { // (M28.3b) two quick hits
                        if (gameMode != 2 && m.health > 0.0f)
                            mc::Mobs::hitArmorStand(world, m, !survival);
                    } else if (gameMode != 2 && m.health > 0.0f &&
                               !mc::Mobs::popFrameItem(world, m, droppedItems, gameRng)) {
                        m.health = 0.0f;
                        m.lastHurtByPlayer = true;
                        if (!survival) { // (gone without its item)
                            m.deathTime = 19;
                            if (mc::world::Chunk* sc = world.chunk(
                                    {mc::world::blockToChunk(int(std::floor(m.pos.x))),
                                     mc::world::blockToChunk(int(std::floor(m.pos.z)))}))
                                sc->removeMobStore(m.uuidHi);
                        }
                    }
                    hc->markDirty();
                    clicks.attackClick = false;
                    clicks.attack = false;
                }
            }
            // Attacking a mob in front of the targeted block (wiki: Melee attack):
            // the held item's attack damage (hand: 1), knockback away from us.
            if (!dead && clicks.attackClick) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                const bool spearHeld =
                    mc::world::itemRegistry().item(inventory.selectedStack().item).tool ==
                    mc::world::ToolType::Spear; // (M28.4e: a jab reaches farther)
                const double reach = spearHeld  ? (survival ? mc::kSpearReach : 5.0)
                                     : survival ? 3.0
                                                : 5.0; // wiki: entity interaction range
                // Lunge (wiki): not while riding, gliding, in water or below 7 hunger; 4
                // exhaustion a level and 1 durability a use.
                if (spearHeld && survival && ridingCart == 0 && !player.gliding() &&
                    !player.inWater() && vitals.food() >= 7)
                    if (const int lunge = mc::world::enchantLevel(inventory.selectedStack(),
                                                                  mc::world::Enchantment::Lunge);
                        lunge > 0) {
                        const glm::dvec3 f(std::cos(glm::radians(player.yaw() + 90.0)), 0.0,
                                           std::sin(glm::radians(player.yaw() + 90.0)));
                        player.setVelocity(
                            player.velocity() +
                            f * mc::lungeImpulse(lunge)); // (Lunge: forward with the jab)
                        vitals.exhaust(4.0f * float(lunge));
                        inventory.setSlot(inventory.selected(),
                                          mc::wearItem(inventory.selectedStack(), 1, gameRng));
                    }
                if (const auto mh = mc::Mobs::raycast(world, eye, look, reach, ridingCart);
                    mh && (!lastHit || mh->distance < lastHit->distance) &&
                    !(spearHeld && survival &&
                      mh->distance < mc::kSpearMinReach)) { // (a spear can't jab that close)
                    auto& m = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    const auto& stack = inventory.selectedStack();
                    const auto& held = mc::world::itemRegistry().item(stack.item);
                    // Weapon enchantments (wiki): Sharpness +0.5 per level +0.5, Smite and
                    // Bane of Arthropods +2.5 per level against their mobs.
                    using E = mc::world::Enchantment;
                    const bool hit = m.hurtTime == 0 && m.deathTime == 0;
                    // A critical hit: falling, not on the ground, in water, flying, gliding,
                    // riding or slow falling - 150% of the base damage, before enchantments
                    // add theirs (wiki: Damage › Critical hit).
                    const bool crit = !player.onGround() && player.velocity().y < 0.0 &&
                                      !player.inWater() && !player.flying() && !player.gliding() &&
                                      ridingCart == 0 && !player.sprinting() &&
                                      vitals.effectLevel(mc::world::Effect::SlowFalling) == 0;
                    mc::MeleeHit mhit;
                    mhit.itemDamage = stack.empty() ? 1.0f : held.attackDamage;
                    mhit.strength = vitals.effectLevel(mc::world::Effect::Strength);
                    mhit.weakness = vitals.effectLevel(mc::world::Effect::Weakness);
                    mhit.critical = crit;
                    mhit.sharpness = mc::world::enchantLevel(stack, E::Sharpness);
                    mhit.smite = mc::world::enchantLevel(stack, E::Smite);
                    mhit.bane = mc::world::enchantLevel(stack, E::BaneOfArthropods);
                    mhit.undead = mc::world::isUndead(m.type);
                    mhit.arthropod = m.type == mc::world::MobType::Spider ||
                                     m.type == mc::world::MobType::CaveSpider ||
                                     m.type == mc::world::MobType::Silverfish ||
                                     m.type == mc::world::MobType::Bee;
                    mhit.impaling = mc::world::enchantLevel(stack, E::Impaling);
                    mhit.aquatic =
                        mc::world::mobInfo(m.type).swims || m.type == mc::world::MobType::Turtle;
                    float dmg = mc::meleeDamage(mhit);
                    // The mace's smash (M28.4d): a falling hit adds by the height fallen, ends the
                    // fall, knocks back what stands around, and Wind Burst throws the player up.
                    static const mc::world::ItemId maceItem =
                        *mc::world::itemRegistry().find("mace");
                    if (stack.item == maceItem && hit && !player.onGround() &&
                        airPeakY - player.position().y > 1.5) {
                        const double fallen = airPeakY - player.position().y;
                        dmg +=
                            mc::maceSmashBonus(fallen, mc::world::enchantLevel(stack, E::Density));
                        vitals.resetFall();
                        airPeakY = player.position().y;
                        const glm::dvec3 at = m.pos;
                        world.levelEvent(mc::world::LevelEvent::Type::Explosion, at.x, at.y, at.z,
                                         5); // (a small dust ring)
                        playSound(mc::world::Sound::Explode, at, 0.5f, 1.4f, true);
                        const mc::world::ChunkPos sc{
                            mc::world::blockToChunk(int(std::floor(at.x))),
                            mc::world::blockToChunk(int(std::floor(at.z)))};
                        for (int dz = -1; dz <= 1; ++dz)
                            for (int dx = -1; dx <= 1; ++dx)
                                if (mc::world::Chunk* kc = world.chunk({sc.x + dx, sc.z + dz}))
                                    for (auto& o : kc->mobs()) {
                                        const glm::dvec3 away = o.pos - at;
                                        const double d = glm::length(away);
                                        if (&o == &m || d > 3.5 || d < 1e-3 || o.health <= 0.0f ||
                                            mc::world::isHanging(o.type) || o.tamed)
                                            continue;
                                        o.vel += glm::dvec3(away.x / d, 0.0, away.z / d) *
                                                     ((3.5 - d) * 0.7) +
                                                 glm::dvec3(0.0, 0.3, 0.0);
                                    }
                        if (const int wind = mc::world::enchantLevel(stack, E::WindBurst);
                            wind > 0) {
                            const glm::dvec3 v = player.velocity();
                            player.setVelocity({v.x, mc::windBurstLift(wind), v.z});
                        }
                    }
                    if (m.type == mc::world::MobType::EnderDragon) // (the head takes it all)
                        dmg = mc::Mobs::dragonDamage(m, dmg, eye + look * mh->distance);
                    if (hit && !crit)
                        playSound(mc::world::Sound::AttackHit, eye + look * mh->distance, 1.0f,
                                  1.0f, true);
                    if (crit && hit) {
                        const glm::dvec3 at = eye + look * mh->distance;
                        world.levelEvent(mc::world::LevelEvent::Type::Crit, at.x, at.y, at.z);
                    }
                    m.looting = static_cast<uint8_t>(mc::world::enchantLevel(stack, E::Looting));
                    mc::Mobs::attack(m, dmg, player.position());
                    // A sweep (M29.2b; wiki: Sword › Sweep attack): a sword hit on the ground,
                    // not sprinting and not a critical, also hits the mobs within a block of the
                    // target for 1 (+ Sweeping Edge's L/(L+1) of the hit).
                    if (held.tool == mc::world::ToolType::Sword && player.onGround() && !player.sprinting() && !crit) {
                        const int se = mc::world::enchantLevel(stack, E::SweepingEdge);
                        const float sweep = 1.0f + dmg * float(se) / float(se + 1);
                        const mc::Aabb mb = mc::Mobs::box(m);
                        const mc::Aabb near{mb.min - glm::dvec3(1.0, 0.25, 1.0), mb.max + glm::dvec3(1.0, 0.25, 1.0)};
                        const uint64_t target = m.uuidHi;
                        for (int dz = -1; dz <= 1; ++dz)
                            for (int dx = -1; dx <= 1; ++dx)
                                if (mc::world::Chunk* sc = world.chunk({mh->chunk.x + dx, mh->chunk.z + dz}))
                                    for (mc::world::MobData& o : sc->mobs())
                                        if (o.uuidHi != target && o.health > 0.0f && !mc::world::isHanging(o.type) &&
                                            o.type != mc::world::MobType::ArmorStand && o.type != mc::world::MobType::LeashKnot &&
                                            o.type != mc::world::MobType::Boat && o.type != mc::world::MobType::Minecart &&
                                            !(mc::world::isPet(o.type) && o.tamed) && mc::Mobs::box(o).intersects(near))
                                            mc::Mobs::attack(o, sweep, player.position());
                        world.levelEvent(mc::world::LevelEvent::Type::Crit, m.pos.x, m.pos.y + 1.0, m.pos.z);
                    }
                    stats.add(mc::world::Stat::DamageDealt, std::lround(dmg * 10.0f));
                    if (dmg >= 100.0f && stack.item == *mc::world::itemRegistry().find(
                                                           "mace")) // (Over-Overkill: 50 hearts)
                        announce(adv.onEvent(mc::world::AdvEvent::MaceSmash50));
                    playerTargetUuid = m.uuidHi; // (tamed wolves join in - M26.1)
                    playerTargetTicks = 0;
                    if (m.type == mc::world::MobType::Villager) { // golems defend villagers (wiki)
                        const mc::world::ChunkPos vc{
                            mc::world::blockToChunk(int(std::floor(m.pos.x))),
                            mc::world::blockToChunk(int(std::floor(m.pos.z)))};
                        for (int dz = -1; dz <= 1; ++dz)
                            for (int dx = -1; dx <= 1; ++dx)
                                if (mc::world::Chunk* gc = world.chunk({vc.x + dx, vc.z + dz}))
                                    for (auto& g : gc->mobs())
                                        if (g.type == mc::world::MobType::IronGolem &&
                                            !g.playerCreated && glm::length(g.pos - m.pos) < 16.0) {
                                            g.angry = true;
                                            g.angerTicks = 600;
                                        }
                    }
                    if (hit) {
                        // Knockback: farther per level; Fire Aspect: alight 4 s per level.
                        if (const int kb = mc::world::enchantLevel(stack, E::Knockback)) {
                            glm::dvec2 d(m.pos.x - player.position().x,
                                         m.pos.z - player.position().z);
                            if (glm::length(d) > 1e-6) d = glm::normalize(d) * (0.5 * kb);
                            m.vel += glm::dvec3(d.x, 0.1, d.y);
                        }
                        if (const int fa = mc::world::enchantLevel(stack, E::FireAspect))
                            m.fireTicks = std::max<int16_t>(m.fireTicks, int16_t(80 * fa));
                        if (survival &&
                            (held.tool != mc::world::ToolType::None ||
                             stack.item == maceItem)) // swords wear 1 per hit, tools 2 (wiki)
                            inventory.setSlot(
                                inventory.selected(),
                                mc::wearItem(stack,
                                             held.tool == mc::world::ToolType::Sword ||
                                                     held.tool == mc::world::ToolType::Spear ||
                                                     stack.item == maceItem
                                                 ? 1
                                                 : 2,
                                             gameRng));
                    }
                    if (survival) vitals.exhaust(0.1f); // wiki: attacking
                    clicks.attackClick = false;
                    clicks.attack = false;
                }
            }
            // Act on the block the outline showed on the last frame (vanilla).
            if (dead) {
                changedBlocks.clear();
            } else if (survival) {
                const mc::world::BlockPos eyeBlock{int(std::floor(feet.x)),
                                                   int(std::floor(feet.y + player.eyeHeight())),
                                                   int(std::floor(feet.z))};
                const bool eyesInWater =
                    mc::pointInFluid(world, player.eyePosition(1.0), mc::world::blocks::Water);
                // Aqua Affinity: no slower mining under water (wiki).
                interaction.setPlaceContents(
                    inventory.selectedStack().contents); // (shulker boxes, M23.6)
                interaction.setPlaceExtra(
                    inventory.selectedStack().extra); // (banner layers, M28.3d)
                const size_t silverfishBefore = blockUpdates.silverfishOut().size();
                const bool silk = mc::world::enchantLevel(inventory.selectedStack(),
                                                          mc::world::Enchantment::SilkTouch) > 0;
                interaction.tickSurvival(
                    world, player, lastHit, inventory, vitals, clicks,
                    eyesInWater && !mc::world::enchantLevel(inventory.armor(0),
                                                            mc::world::Enchantment::AquaAffinity),
                    gameRng, changedBlocks, drops);
                if (silk) // (wiki: Infested Block - Silk Touch takes the block, silverfish and all)
                    blockUpdates.silverfishOut().resize(silverfishBefore);
                for (const auto& d : drops)
                    droppedItems.spawn(d.pos, d.stack, gameRng);
                if (interaction.takeChorusTeleport())
                    if (const auto to = mc::chorusTeleport(world, player.position(), gameRng)) {
                        player.setPosition(*to);
                        vitals.resetFall(); // (vanilla: the fall is reset by the teleport)
                    }
            } else {
                interaction.tickDrinking(inventory, vitals, clicks.use || clicks.useClick, false);
                interaction.setPlaceContents(inventory.selectedStack().contents);
                interaction.setPlaceExtra(inventory.selectedStack().extra);
                const size_t silverfishBefore = blockUpdates.silverfishOut().size();
                interaction.tick(
                    world, player, lastHit, inventory.placeState(), clicks, changedBlocks, &drops,
                    mc::world::itemRegistry().item(inventory.selectedStack().item).tool ==
                        mc::world::ToolType::Sword);
                blockUpdates.silverfishOut().resize(
                    silverfishBefore); // (creative breaks release none - wiki)
                for (const auto& d : drops)
                    droppedItems.spawn(d.pos, d.stack, gameRng);
            }
            frameEdits.insert(frameEdits.end(), changedBlocks.begin(), changedBlocks.end());
            if (droppedItems.tick(world, player.box(), !dead && gameMode != 3, inventory) >
                0) // vanilla pitch ((r - r) x 0.7 + 1) x 2
                playSound(mc::world::Sound::ItemPickup, player.position(), 1.0f, 1.0f, false);
            // Game rules read the tick's own time, not the renderer's interpolated value.
            const bool overworld = dimension == Dimension::Overworld;
            const double tickSkyDarken = mc::world::skyDarken(mc::world::celestialAngle(dayTime),
                                                              overworld ? weather.rain : 0.0,
                                                              overworld ? weather.thunder : 0.0);
            mc::Mobs::Context mobCtx{world,
                                     player,
                                     vitals,
                                     survival,
                                     dead,
                                     dayTime,
                                     float(tickSkyDarken),
                                     gameRng,
                                     droppedItems,
                                     true, // (the End: endermen, M20)
                                     inventory.selectedStack().item,
                                     &frameEdits,
                                     &projectiles,
                                     &orbs};
            mobCtx.tnt = &primedTnt;
            mobCtx.naturalSpawning = rules.spawnMobs;
            mobCtx.mobDrops = rules.mobDrops;
            mobCtx.mobGriefing = rules.mobGriefing;
            mobCtx.spawnPhantoms = rules.spawnPhantoms;
            mobCtx.difficulty = difficulty;
            mobCtx.thorns = 0;
            for (int piece = 0; piece < 4; ++piece)
                mobCtx.thorns += mc::world::enchantLevel(inventory.armor(piece), mc::world::Enchantment::Thorns);
            mobCtx.worldSeed = seed;
            mobCtx.weather = overworld ? &weather : nullptr;
            mobCtx.thundering = overworld && weather.raining && weather.thunder > 0.9f;
            mobCtx.raidCentre =
                overworld && raid.active() && raid.loaded() ? &raid.centre() : nullptr;
            mobCtx.raidId = raid.id();
            if (playerTargetUuid != 0 && ++playerTargetTicks > 100) playerTargetUuid = 0;
            mobCtx.playerTargetUuid = playerTargetUuid;
            mobCtx.timeSinceRest = vitals.timeSinceRest(); // (M26.4a: phantoms)
            mobCtx.playerAttackerUuid = mobs.playerAttacker();
            for (int piece = 0; piece < 4;
                 ++piece) // piglins: any golden armor piece (wiki: Piglin)
                if (!inventory.armor(piece).empty() && mc::world::itemRegistry()
                                                           .item(inventory.armor(piece).item)
                                                           .id.starts_with("minecraft:golden_"))
                    mobCtx.wearsGold = true;
            // Scheduled block ticks, random ticks within the simulation distance, block
            // events (vanilla: before entities).
            {
                const mc::world::BlockPos at{int(std::floor(feet.x)), 0, int(std::floor(feet.z))};
                // Ticking chunks: within the simulation distance, never past what's loaded.
                blockUpdates.setRandomTicks(
                    at.chunk(), std::min(mobs.simulationDistance(), opts->renderDistance),
                    rules.randomTickSpeed); // (game rule random_tick_speed)
                blockUpdates.setTntExplodes(rules.tntExplodes);
                blockUpdates.setPlayer(feet);
                blockUpdates.setSkyDarken(overworld ? int(tickSkyDarken) : 0);
                blockUpdates.setDayTime(dayTime);
                blockUpdates.setWeather(overworld ? &weather : nullptr);
            }
            {
                // Pressure plates (M21.1): everything standing on one this tick presses it
                // (its box over the plate's 14x14 middle, feet in the plate's cell).
                auto pressAt = [&](const glm::dvec3& f, double half, bool item, bool cart = false) {
                    const mc::world::BlockPos c{int(std::floor(f.x)), int(std::floor(f.y + 0.01)),
                                                int(std::floor(f.z))};
                    const double fx = f.x - c.x, fz = f.z - c.z;
                    if (fx + half < 1.0 / 16.0 || fx - half > 15.0 / 16.0 ||
                        fz + half < 1.0 / 16.0 || fz - half > 15.0 / 16.0)
                        return;
                    blockUpdates.pressPlate(c, item, cart);
                    if (!item && reg.blockOf(world.getBlock(c)) == mc::world::blocks::BigDripleaf)
                        blockUpdates.tiltDripleaf(c); // (M27.2: it tips under them)
                };
                if (!dead && !player.flying()) {
                    pressAt(player.position(), 0.3, false);
                    // (M27.3) stepping on a shrieker sets it off, unless sneaking
                    const glm::dvec3 f = player.position();
                    const mc::world::BlockPos c{int(std::floor(f.x)), int(std::floor(f.y - 0.01)),
                                                int(std::floor(f.z))};
                    if (player.onGround() && !player.sneaking() &&
                        reg.blockOf(world.getBlock(c)) == mc::world::blocks::SculkShrieker)
                        blockUpdates.shriek(c);
                }
                world.forEachTickingChunk([&](mc::world::Chunk& c) {
                    for (const auto& m : c.mobs()) {
                        const bool cart = m.type == mc::world::MobType::Minecart;
                        const auto& info = mc::world::mobInfo(m.type);
                        if (m.health <= 0.0f || (info.flies && !cart)) continue;
                        // (only when it stands on a plate: read from its own chunk, no lookup)
                        const int fy = int(std::floor(m.pos.y + 0.01));
                        if (!c.height().contains(fy)) continue;
                        const mc::world::BlockId under =
                            reg.blockOf(c.get(mc::world::blockToLocal(int(std::floor(m.pos.x))), fy,
                                              mc::world::blockToLocal(int(std::floor(m.pos.z)))));
                        if (!mc::world::BlockUpdates::isPressurePlate(under) &&
                            under != mc::world::blocks::BigDripleaf && under != mc::world::blocks::Tripwire)
                            continue;
                        pressAt(m.pos, info.width * 0.5, false, cart);
                    }
                });
                for (const auto& it : droppedItems.items())
                    pressAt(it.pos, 0.125, true);
                blockUpdates.settlePlates();
            }
            blockUpdates.tick();
            if (!blockUpdates.commandRuns().empty()) { // (M29.7) command blocks that fired
                mc::CommandContext ctx{player,   inventory,   dayTime,  gameTime,
                                       seed,     &survival,   &vitals,  &world,
                                       &gameRng, &frameEdits, &weather, &commandBolts};
                ctx.rules = &rules;
                ctx.difficulty = &difficulty;
                ctx.gameMode = &gameMode;
                mc::runCommandBlocks(world, blockUpdates.commandRuns(), ctx);
            }
            // Lightning (M22.1; wiki: Lightning): storms strike in BlockUpdates (fire
            // placed there); here the bolt hurts what stands near and is drawn.
            {
                auto strike = [&](const mc::world::BlockPos& b) {
                    const glm::dvec3 at(b.x + 0.5, b.y, b.z + 0.5);
                    mc::Mobs::strikeLightning(world, at);
                    const mc::Aabb zone{at - glm::dvec3(3.0), at + glm::dvec3(3.0, 9.0, 3.0)};
                    if (survival && !dead && player.box().intersects(zone)) {
                        vitals.attacked(5.0f, nullptr, mc::Vitals::Hit::Fire);
                        vitals.setOnFire(160); // 8 s
                    }
                    if (bolts.size() < bolts.capacity())
                        bolts.push_back({at, uint32_t(gameRng.nextLong()), 8});
                    playSound(mc::world::Sound::Thunder, at, 1.0f, 1.0f,
                              true); // (heard everywhere)
                    skyFlash = 2;
                };
                for (const auto& b : blockUpdates.lightning()) {
                    strike(b);
                    // (M29.1b) a storm's bolt may leave a skeleton trap horse.
                    mc::Mobs::spawnSkeletonTrap(world, glm::dvec3(b.x + 0.5, b.y, b.z + 0.5), difficulty, gameRng);
                }
                for (const glm::dvec3& t : mobs.trapBolts()) { // (a sprung trap: a harmless bolt)
                    if (bolts.size() < bolts.capacity()) bolts.push_back({t, uint32_t(gameRng.nextLong()), 8});
                    playSound(mc::world::Sound::Thunder, t, 1.0f, 1.0f, true);
                    skyFlash = 2;
                }
                for (const auto& b : commandBolts) {
                    blockUpdates.strikeLightning(b); // (its fire)
                    strike(b);
                }
                commandBolts.clear();
            }
            // Blocks a piston moves carry whatever is in their way (M21.5; wiki: Piston):
            // half a block a tick for the 2 ticks of the move.
            for (const auto& mv : blockUpdates.moving()) {
                if (mv.visual) continue;
                const glm::dvec3 step(glm::dvec3(mc::world::normal(mv.dir)) * 0.5);
                const mc::Aabb cell{{double(mv.to.x), double(mv.to.y), double(mv.to.z)},
                                    {mv.to.x + 1.0, mv.to.y + 1.0, mv.to.z + 1.0}};
                // (as a push: the entity's own movement then collides with blocks)
                auto carry = [&](glm::dvec3 v) {
                    for (int a = 0; a < 3; ++a)
                        if (step[a] != 0.0)
                            v[a] = step[a] > 0 ? std::max(v[a], step[a]) : std::min(v[a], step[a]);
                    return v;
                };
                if (!dead && player.box().intersects(cell))
                    player.setVelocity(carry(player.velocity()));
                if (mc::world::Chunk* mc0 = world.chunk(mv.to.chunk()))
                    for (auto& m : mc0->mobs())
                        if (mc::Mobs::box(m).intersects(cell)) m.vel = carry(m.vel);
            }
            { // Dispensers and droppers that fired (M21.3b).
                mc::DispenseContext dctx{world,     blockUpdates, droppedItems, projectiles,
                                         primedTnt, gameRng,      frameEdits};
                for (const mc::world::BlockPos& b : blockUpdates.dispensed())
                    mc::dispense(dctx, b);
                blockUpdates.dispensed().clear();
            }
            // TNT (M21.1b): lit blocks become primed TNT; fuses run out and explode.
            for (const mc::world::BlockPos& b : blockUpdates.primedTnt())
                primedTnt.prime(b, 80, gameRng);
            blockUpdates.primedTnt().clear();
            primedTnt.tick(world);
            for (const glm::dvec3& at : primedTnt.explosions()) {
                mc::ExplosionTargets t;
                t.tnt = &primedTnt;
                t.dropAll = true;
                t.blockDrops = rules.blockDrops;
                if (survival && !dead) {
                    t.player = &player;
                    t.vitals = &vitals;
                }
                tntBlast.explode(world, at, 4.0f, gameRng, droppedItems, frameEdits, t);
            }
            frameEdits.insert(frameEdits.end(), blockUpdates.changed().begin(),
                              blockUpdates.changed().end());
            blockUpdates.changed().clear();
            frameRemesh.insert(frameRemesh.end(), blockUpdates.remeshOnly().begin(),
                               blockUpdates.remeshOnly().end());
            frameSettling.insert(frameSettling.end(), blockUpdates.settling().begin(),
                                 blockUpdates.settling().end());
            blockUpdates.settling().clear();
            blockUpdates.remeshOnly().clear();
            for (const auto& d : blockUpdates.drops()) {
                if (!rules.blockDrops) break; // (game rule block_drops)
                const glm::dvec3 where{d.pos.x + 0.5, d.pos.y + 0.25, d.pos.z + 0.5};
                if (d.loot) { // the block's own loot, as if broken by hand
                    lootScratch.clear();
                    mc::blockDrops(d.loot, {}, gameRng, lootScratch);
                    for (const auto& stack : lootScratch)
                        droppedItems.spawn(where, stack, gameRng);
                } else {
                    droppedItems.spawn(where, d.stack, gameRng);
                }
            }
            blockUpdates.drops().clear();
            for (const auto& h :
                 blockUpdates.hatched()) // (M25.3b: baby turtles from eggs, at home there)
                for (int k = 0; k < h.count; ++k) {
                    if (h.type == mc::world::MobType::HappyGhast) { // (M26.5b: a soaked dried
                                                                    // ghast's ghastling)
                        mc::world::MobData g = mc::Mobs::make(
                            mc::world::MobType::HappyGhast,
                            {h.pos.x + 0.5, double(h.pos.y), h.pos.z + 0.5}, gameRng);
                        g.age = -24000;
                        g.persistent = true;
                        mc::Mobs::add(world, g);
                        continue;
                    }
                    if (h.type ==
                        mc::world::MobType::Sniffer) { // (M27.5c: a snifflet out of its egg)
                        mc::world::MobData s = mc::Mobs::make(
                            mc::world::MobType::Sniffer,
                            {h.pos.x + 0.5, double(h.pos.y), h.pos.z + 0.5}, gameRng);
                        s.age = -48000; // (wiki: grows up in 40 minutes)
                        s.persistent = true;
                        mc::Mobs::add(world, s);
                        continue;
                    }
                    if (h.type ==
                        mc::world::MobType::Creaking) { // (M27.1c: an awake heart's creaking)
                        mc::Mobs::spawnCreaking(world, h.pos, player.position(), gameRng);
                        continue;
                    }
                    if (h.type ==
                        mc::world::MobType::Tadpole) { // (M26.3c: frogspawn, into its water)
                        mc::Mobs::add(world, mc::Mobs::make(mc::world::MobType::Tadpole,
                                                            {h.pos.x + 0.3 + 0.1 * k, h.pos.y - 0.6,
                                                             h.pos.z + 0.5},
                                                            gameRng));
                        continue;
                    }
                    mc::world::MobData baby =
                        mc::Mobs::make(mc::world::MobType::Turtle,
                                       {h.pos.x + 0.5, double(h.pos.y), h.pos.z + 0.5}, gameRng);
                    baby.age = -24000;
                    baby.home = {h.pos.x, h.pos.y, h.pos.z};
                    baby.persistent = true;
                    mc::Mobs::add(world, baby);
                }
            blockUpdates.hatched().clear();
            // Shriekers (M27.3; wiki: Sculk Shrieker): one that can summon warns the player
            // (Darkness for 12 s) and the 4th warning calls a warden.
            for (const auto& sh : blockUpdates.shrieks()) {
                const glm::dvec3 c(sh.pos.x + 0.5, sh.pos.y + 0.5, sh.pos.z + 0.5);
                world.levelEvent(mc::world::LevelEvent::Type::Note, c.x, c.y + 0.6, c.z,
                                 0); // (a visible cue)
                if (!sh.canSummon || !survival || dead || glm::length(player.position() - c) > 40.0)
                    continue;
                const int warned = vitals.wardenWarn();
                if (warned < 0) continue; // (within its 10 s: nothing)
                vitals.addEffect(mc::world::Effect::Darkness, 0, 240);
                if (warned > 0 && rules.spawnWardens)
                    mc::Mobs::summonWarden(world, sh.pos, gameRng);
            }
            blockUpdates.shrieks().clear();
            for (const auto& sp :
                 blockUpdates.silverfishOut()) // (M26.4a: out of a broken infested block)
                mc::Mobs::add(world,
                              mc::Mobs::make(mc::world::MobType::Silverfish,
                                             {sp.x + 0.5, double(sp.y), sp.z + 0.5}, gameRng));
            blockUpdates.silverfishOut().clear();
            // Sand/gravel that lost its support falls as an entity (M16).
            for (const auto& f : blockUpdates.fallingStarts())
                fallingBlocks.spawn(f.pos, f.state);
            blockUpdates.fallingStarts().clear();
            fallingBlocks.tick(world, droppedItems, gameRng, frameEdits);
            // Falling stalactites (M27.2b) skewer whoever stands where they land.
            for (const auto& hit : fallingBlocks.impacts()) {
                const mc::Aabb spot{hit.pos - glm::dvec3(0.5, 0.0, 0.5),
                                    hit.pos + glm::dvec3(0.5, 2.0, 0.5)};
                if (survival && !dead && player.box().intersects(spot))
                    vitals.attacked(hit.damage, nullptr);
                if (mc::world::Chunk* hc =
                        world.chunk({mc::world::blockToChunk(int(std::floor(hit.pos.x))),
                                     mc::world::blockToChunk(int(std::floor(hit.pos.z)))}))
                    for (auto& m : hc->mobs())
                        if (m.health > 0.0f && mc::Mobs::box(m).intersects(spot)) {
                            mc::Mobs::attack(m, hit.damage, hit.pos);
                            m.lastHurtByPlayer = false; // (not the player's kill)
                        }
            }
            fallingBlocks.impacts().clear();
            // Wear from this tick's hits (armor pieces; the shield that blocked).
            if (const int wear = vitals.takeArmorWear(); wear > 0 && survival)
                inventory.wearArmor(wear, gameRng);
            if (const int wear = vitals.takeShieldWear(); wear > 0 && survival) {
                static const mc::world::ItemId shieldItem =
                    *mc::world::itemRegistry().find("shield");
                auto wearShield = [&](mc::world::ItemStack s) {
                    return mc::wearItem(s, wear, gameRng);
                };
                if (inventory.selectedStack().item == shieldItem)
                    inventory.setSlot(inventory.selected(), wearShield(inventory.selectedStack()));
                else if (inventory.offhand().item == shieldItem)
                    inventory.setOffhand(wearShield(inventory.offhand()));
            }
            // The line breaks when the rod leaves the hand (vanilla) or the player dies.
            if (dead || mc::world::itemRegistry().item(inventory.selectedStack().item).id !=
                            "minecraft:fishing_rod")
                fishing.cancel();
            fishing.tick(world, player.position(), gameRng);
            projectiles.setThundering(dimension == Dimension::Overworld && weather.raining &&
                                      weather.thunder > 0.9f);
            projectiles.tick(world, player, !dead ? &vitals : nullptr, inventory, survival,
                             gameRng);
            // Ghast fireballs explode (wiki: Fireball - power 1, incendiary: fire on a
            // third of the open spots around it); blaze fireballs lit blocks.
            for (const mc::world::BlockPos& b :
                 projectiles.channeled()) // (Channeling: next tick's bolts)
                commandBolts.push_back(b);
            // Wind bursts (M26.4c; wiki: Wind Charge): what is within 2.5 blocks is thrown away
            // from the burst - a player's own charge under their feet sends them high (and
            // spares their fall) - and doors, trapdoors, gates, buttons and levers within
            // a block are worked.
            for (const auto& fb : projectiles.fireworkBursts()) { // (M28.4c) sparks and a bang
                world.levelEvent(mc::world::LevelEvent::Type::Firework, fb.pos.x, fb.pos.y,
                                 fb.pos.z, fb.fireworks);
                playSound(mc::world::Sound::Explode, fb.pos, 0.4f, 1.6f, false);
            }
            for (const auto& wb : projectiles.windBursts()) {
                const double radius = 2.5;
                const glm::dvec3 centre = player.position() + glm::dvec3(0.0, 0.9, 0.0);
                const glm::dvec3 d = centre - wb.pos;
                const double l = glm::length(d);
                if (!dead && l < radius) {
                    const double f = 1.0 - l / radius;
                    const glm::dvec3 away = l > 1e-6 ? d / l : glm::dvec3(0, 1, 0);
                    player.setVelocity(player.velocity() + away * (1.4 * f) +
                                       glm::dvec3(0.0, 0.5 * f, 0.0));
                    vitals.resetFall();
                }
                const mc::world::ChunkPos wc{mc::world::blockToChunk(int(std::floor(wb.pos.x))),
                                             mc::world::blockToChunk(int(std::floor(wb.pos.z)))};
                for (int dz = -1; dz <= 1; ++dz)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (mc::world::Chunk* ch = world.chunk({wc.x + dx, wc.z + dz}))
                            for (auto& mob : ch->mobs()) {
                                const glm::dvec3 md = mob.pos + glm::dvec3(0.0, 0.5, 0.0) - wb.pos;
                                const double ml = glm::length(md);
                                if (ml >= radius || mob.type == mc::world::MobType::Breeze)
                                    continue;
                                const double f = 1.0 - ml / radius;
                                mob.vel += (ml > 1e-6 ? md / ml : glm::dvec3(0, 1, 0)) * (1.2 * f) +
                                           glm::dvec3(0.0, 0.4 * f, 0.0);
                            }
                const mc::world::BlockPos bc{int(std::floor(wb.pos.x)), int(std::floor(wb.pos.y)),
                                             int(std::floor(wb.pos.z))};
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx) {
                            const mc::world::BlockPos q{bc.x + dx, bc.y + dy, bc.z + dz};
                            const auto qs = world.getBlock(q);
                            const auto qb = reg.likeOf(reg.blockOf(qs));
                            if ((qb == mc::world::blocks::OakDoor ||
                                 qb == mc::world::blocks::OakTrapdoor ||
                                 qb == mc::world::blocks::OakFenceGate ||
                                 qb == mc::world::blocks::Lever ||
                                 qb == mc::world::blocks::OakButton ||
                                 qb == mc::world::blocks::StoneButton) &&
                                (qb != mc::world::blocks::OakDoor ||
                                 reg.get(qs, mc::world::properties::doorHalf) == 1) &&
                                blockUpdates.use(q))
                                frameEdits.push_back(q);
                        }
                playSound(mc::world::Sound::Explode, wb.pos, 0.3f, 1.6f, false);
            }
            for (const glm::dvec3& at :
                 projectiles.witherBlasts()) // (M26.4b: wither skulls, power 1, no fire)
                fireballBlast.explode(world, at, 1.0f, gameRng, droppedItems, frameEdits,
                                      {survival && !dead ? &player : nullptr, &vitals, true,
                                       &primedTnt, false, rules.mobGriefing, rules.blockDrops});
            for (const glm::dvec3& at : projectiles.explosions()) {
                fireballBlast.explode(world, at, 1.0f, gameRng, droppedItems, frameEdits,
                                      {survival && !dead ? &player : nullptr, &vitals, true,
                                       &primedTnt, false, rules.mobGriefing, rules.blockDrops});
                if (!rules.mobGriefing) continue; // (no fire either)
                const mc::world::BlockPos c{int(std::floor(at.x)), int(std::floor(at.y)),
                                            int(std::floor(at.z))};
                for (int dx = -2; dx <= 2; ++dx)
                    for (int dy = -2; dy <= 2; ++dy)
                        for (int dz = -2; dz <= 2; ++dz) {
                            const mc::world::BlockPos q{c.x + dx, c.y + dy, c.z + dz};
                            if (!world.isInHeight(q.y) || world.getBlock(q) != 0 ||
                                gameRng.nextInt(3) != 0 ||
                                !mc::world::BlockUpdates::fireCanStay(world, q))
                                continue;
                            world.updateBlock(q, mc::world::BlockUpdates::fireState(0));
                            frameEdits.push_back(q);
                        }
            }
            frameEdits.insert(frameEdits.end(), projectiles.edits().begin(),
                              projectiles.edits().end());
            projectiles.edits().clear();
            // Ender pearls (M20.3): the player goes where one lands (5 damage), or
            // through the end gateway it went into.
            auto gatewayTravel = [&](const mc::world::BlockPos& g) {
                const auto* eg = dynamic_cast<const mc::world::EndGenerator*>(generatorPtr.get());
                if (!eg) return;
                if (const auto to = dragonFight.gatewayTarget(*eg, g)) {
                    player.setPosition(*to);
                    announce(adv.onEvent(mc::world::AdvEvent::EnterGateway));
                    vitals.resetFall();
                }
            };
            for (const mc::PearlLanding& pl : projectiles.pearls()) {
                if (dead) break;
                if (pl.gateway) {
                    gatewayTravel(pl.gatewayBlock);
                } else {
                    player.setPosition(pl.pos);
                    vitals.resetFall();
                    // (vanilla: like a fall - armour doesn't help, Feather Falling does)
                    if (survival)
                        vitals.damage(
                            vitals.protectionReduced(5.0f, mc::Vitals::Hit::Generic, true), false);
                }
            }
            if (!dead && dimension == Dimension::End &&
                mc::portals::touching(world, player.box(), mc::world::blocks::EndGateway)) {
                // Walking (or flying) into a gateway works too (vanilla: any entity).
                const glm::dvec3 f = player.position();
                for (int dy = 0; dy <= 2; ++dy)
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx) {
                            const mc::world::BlockPos b{int(std::floor(f.x)) + dx,
                                                        int(std::floor(f.y)) + dy,
                                                        int(std::floor(f.z)) + dz};
                            if (reg.blockOf(world.getBlock(b)) == mc::world::blocks::EndGateway) {
                                gatewayTravel(b);
                                dy = dz = dx = 3; // (once)
                            }
                        }
            }
            for (const glm::dvec3& at : projectiles.eyeDrops()) {
                static const mc::world::ItemId eyeItem =
                    *mc::world::itemRegistry().find("ender_eye");
                droppedItems.spawn(at, {eyeItem, 1}, gameRng);
            }
            // Experience: ores just mined, furnace output taken, orbs collected (M17.5).
            if (const int xp = interaction.takeExperience(); xp > 0) {
                const mc::world::BlockPos b = interaction.experienceAt();
                orbs.drop({b.x + 0.5, b.y + 0.5, b.z + 0.5}, xp, gameRng);
            }
            // Bottles o' enchanting that broke (M29.3c; wiki: 3-11 experience).
            for (const glm::dvec3& at : projectiles.xpBottles())
                orbs.drop(at, 3 + int(gameRng.nextInt(5)) + int(gameRng.nextInt(5)), gameRng);
            for (const auto& hit : projectiles.targetHits()) // (M29.5: target blocks)
                blockUpdates.hitTarget(hit.block, hit.strength, hit.ticks);
            // Furnace output taken: its stored recipe uses become orbs at the player (vanilla).
            if (const int xp = mc::recipesExperience(container.takeRecipes(), gameRng); xp > 0)
                orbs.drop(player.position() + glm::dvec3(0.0, 0.5, 0.0), xp, gameRng);
            // Enchanting and anvils (M17.5): levels spent, a new seed after enchanting,
            // and the anvil's wear: 12% a use - anvil, chipped, damaged, gone (wiki).
            if (const int spent = container.takeLevelsSpent(); spent > 0) vitals.spendLevels(spent);
            const glm::dvec3 blockCentre(containerBlock.x + 0.5, containerBlock.y + 0.5,
                                         containerBlock.z + 0.5);
            if (container.takeEnchanted()) {
                stats.add(mc::world::Stat::EnchantItem);
                announce(adv.onEvent(mc::world::AdvEvent::Enchanted));
                vitals.setEnchantSeed(gameRng.nextLong() & 0xFFFFFFFFull);
                playSound(mc::world::Sound::Enchant, blockCentre, 1.0f, 1.0f, true);
            }
            if (const int traded = container.takeTrades(); traded > 0) {
                stats.add(mc::world::Stat::TradedWithVillager, traded);
                announce(adv.onEvent(mc::world::AdvEvent::Traded));
            }
            for (const mc::world::ItemStack& c : container.crafted())
                stats.addItem(mc::world::ItemStat::Crafted, c.item, c.count);
            container.clearCrafted();
            if (const int tradeXp = container.takeTradeExperience();
                tradeXp > 0) // (M24.2: orbs from trading)
                orbs.drop(player.position() + glm::dvec3(0.0, 0.5, 0.0), tradeXp, gameRng);
            if (const int grindCost = container.takeGrindCost();
                grindCost > 0) // (M23.5: orbs at the grindstone)
                orbs.drop({containerBlock.x + 0.5, containerBlock.y + 0.5, containerBlock.z + 0.5},
                          mc::grindExperience(grindCost, gameRng), gameRng);
            const bool anvilUsed = container.takeAnvilUsed();
            if (anvilUsed) playSound(mc::world::Sound::AnvilUse, blockCentre, 1.0f, 1.0f, true);
            if (anvilUsed && survival && gameRng.nextFloat() < 0.12f) {
                const auto st = world.getBlock(containerBlock);
                const auto b = reg.blockOf(st);
                const mc::world::BlockId next =
                    b == mc::world::blocks::Anvil
                        ? mc::world::BlockId(mc::world::blocks::ChippedAnvil)
                    : b == mc::world::blocks::ChippedAnvil
                        ? mc::world::BlockId(mc::world::blocks::DamagedAnvil)
                        : mc::world::BlockId(0);
                if (b == mc::world::blocks::Anvil || b == mc::world::blocks::ChippedAnvil ||
                    b == mc::world::blocks::DamagedAnvil) {
                    const auto ns =
                        next ? reg.with(reg.defaultState(next), "facing", *reg.value(st, "facing"))
                                   .value_or(reg.defaultState(next))
                             : mc::world::BlockStateId{0};
                    world.updateBlock(containerBlock, ns);
                    frameEdits.push_back(containerBlock);
                    if (!next) { // destroyed: the screen closes, its items come back
                        screenDrops.clear();
                        container.close(inventory, screenDrops);
                        for (const auto& d : screenDrops)
                            droppedItems.spawn(player.position() + glm::dvec3(0, 1, 0), d, gameRng);
                        window.setCursorCaptured(true);
                    }
                }
            }
            if (int xp = orbs.tick(world, player.box(), !dead); xp > 0) {
                // Mending (M29.2b; wiki): the orb repairs a damaged Mending item worn or held
                // first - 2 durability a point - and only what's left counts as experience.
                const auto mendable = [&](const mc::world::ItemStack& s) {
                    return !s.empty() && s.damage > 0 && mc::world::itemRegistry().item(s.item).durability > 0 &&
                           mc::world::enchantLevel(s, mc::world::Enchantment::Mending) > 0;
                };
                for (int slot = -6; slot < 0 && xp > 0; ++slot) { // armor x4, offhand, main hand
                    mc::world::ItemStack s = slot < -2 ? inventory.armor(slot + 6)
                                             : slot == -2 ? inventory.offhand()
                                                          : inventory.selectedStack();
                    if (!mendable(s)) continue;
                    const int fix = std::min(int(s.damage), xp * 2);
                    s.damage = uint16_t(s.damage - fix);
                    xp -= (fix + 1) / 2;
                    if (slot < -2) inventory.setArmor(slot + 6, s);
                    else if (slot == -2) inventory.setOffhand(s);
                    else inventory.setSlot(inventory.selected(), s);
                }
                vitals.addExperience(xp);
                playSound(mc::world::Sound::OrbPickup, player.position(), 1.0f, 1.0f, false);
            }
            mobs.tick(mobCtx);
            for (const uint64_t cartUuid : mobs.cartCommands()) // (M29.7) command block minecarts
                if (const mc::world::MobData* cart = mc::Mobs::mobByUuid(world, player.position(), cartUuid)) {
                    mc::CommandContext ctx{player,   inventory,   dayTime,  gameTime,
                                           seed,     &survival,   &vitals,  &world,
                                           &gameRng, &frameEdits, &weather, &commandBolts};
                    ctx.rules = &rules;
                    ctx.difficulty = &difficulty;
                    ctx.gameMode = &gameMode;
                    const glm::dvec3 from = cart->pos;
                    ctx.origin = &from;
                    mc::runCommand(mc::world::nameText(cart->commandId), ctx);
                }
            mobs.cartCommands().clear();
            mc::tickHoppers(world, droppedItems); // (M21.3)
            if (ridingCart !=
                0) { // the rider goes with the cart (an activator rail throws them out)
                if (mc::world::MobData* cart = findCart(); cart && cart->ridden) {
                    const bool boat = cart->type == mc::world::MobType::Boat;
                    player.setPosition(cart->pos +
                                       glm::dvec3(0.0, mc::Mobs::seatHeight(*cart), 0.0));
                    if (boat)
                        player.setRotation(player.yaw() + cart->yawVel,
                                           player.pitch()); // (turning with it)
                    player.setVelocity(glm::dvec3(0.0));
                    vitals.resetFall();
                    const mc::world::BlockPos under{int(std::floor(cart->pos.x)),
                                                    int(std::floor(cart->pos.y)),
                                                    int(std::floor(cart->pos.z))};
                    const auto us = world.getBlock(under);
                    if (reg.blockOf(us) == mc::world::blocks::ActivatorRail &&
                        reg.get(us, mc::world::properties::powered) == 0) {
                        cart->ridden = false;
                        player.setPosition(cart->pos + glm::dvec3(0.0, 1.0, 0.0));
                        ridingCart = 0;
                    }
                } else {
                    ridingCart = 0;
                }
            }
            const auto* endGen = dynamic_cast<const mc::world::EndGenerator*>(generatorPtr.get());
            if (dimension == Dimension::End && endKind == "end2" && endGen)
                dragonFight.tick(world, *endGen, mobs, player.position(), orbs, gameRng,
                                 frameEdits);
            if (dimension == Dimension::Overworld && !dead) { // (M24.4)
                if (rules.spawnWanderingTraders)
                    traderSpawner.tick(world, player.position(), gameRng);
                if (rules.spawnPatrols && difficulty > 0)
                    patrolSpawner.tick(world, player.position(), dayTime, gameRng);
                if (difficulty > 0) {
                    raid.tick(world, vitals, player.position(), gameRng);
                } else if (raid.active() || raid.pending()) { // Peaceful ends raids (wiki: Raid)
                    mc::Raid::State none = raid.state();
                    none.active = false;
                    none.pendingTicks = 0;
                    raid.restore(none);
                }
            }
            // Furnaces smelt in every loaded chunk (block entities tick, wiki).
            litChanges.clear();
            world.forEachTickingChunk([&](mc::world::Chunk& c) {
                // Beacons (every 80 ticks) and conduits (every 40) (M23.6; wiki: Beacon, Conduit).
                if (gameTime % 40 == 0)
                    for (auto& bc : c.beacons()) {
                        const mc::world::BlockPos bp{c.pos().x * 16 + bc.x, bc.y,
                                                     c.pos().z * 16 + bc.z};
                        if (bc.data.conduit) {
                            const int frame = mc::conduitFrame(world, bp);
                            if (frame != bc.data.levels) bc.data.levels = frame;
                            const glm::dvec3 d =
                                player.position() - glm::dvec3(bp.x + 0.5, bp.y + 0.5, bp.z + 0.5);
                            const bool wet =
                                player.inWater() ||
                                mc::world::rainingAt(world, weather,
                                                     {int(std::floor(player.position().x)),
                                                      int(std::floor(player.position().y)),
                                                      int(std::floor(player.position().z))});
                            if (frame >= 16 && mc::conduitWet(world, bp) && wet && !dead &&
                                glm::length(d) <= double(mc::conduitRange(frame)))
                                vitals.addEffect(mc::world::Effect::ConduitPower, 0, 260); // 13 s
                            // A full frame (42 blocks) strikes the nearest hostile mob in the
                            // water within 8 blocks for 4 every 2 s (wiki: Conduit › Attack).
                            if (frame >= 42 && mc::conduitWet(world, bp)) {
                                mc::world::MobData* target = nullptr;
                                double best = 8.0 * 8.0;
                                const glm::dvec3 at(bp.x + 0.5, bp.y + 0.5, bp.z + 0.5);
                                for (int ddz = -1; ddz <= 1; ++ddz)
                                    for (int ddx = -1; ddx <= 1; ++ddx)
                                        if (mc::world::Chunk* tc =
                                                world.chunk({c.pos().x + ddx, c.pos().z + ddz}))
                                            for (auto& mob : tc->mobs()) {
                                                if (!mc::world::mobInfo(mob.type).hostile ||
                                                    mob.health <= 0.0f)
                                                    continue;
                                                const double d2 =
                                                    glm::dot(mob.pos - at, mob.pos - at);
                                                if (d2 >= best) continue;
                                                if (!mc::fluidContact(world, mc::Mobs::box(mob))
                                                         .water)
                                                    continue;
                                                best = d2;
                                                target = &mob;
                                            }
                                if (target) {
                                    target->health -= 4.0f;
                                    target->hurtTime = 10;
                                }
                            }
                            continue;
                        }
                        if (gameTime % 80 != 0) continue;
                        const int tiers = mc::beaconTiers(world, bp);
                        const bool sky = mc::beaconSky(world, bp);
                        if (tiers != bc.data.levels || sky != bc.data.beam) c.markDirty();
                        bc.data.levels = tiers;
                        bc.data.beam = sky;
                        std::array<mc::BeaconGift, 2> gifts{};
                        if (!dead)
                            for (int g = 0,
                                     n = mc::beaconGifts(bc.data, bp, player.position(), gifts);
                                 g < n; ++g)
                                vitals.addEffect(gifts[size_t(g)].type, gifts[size_t(g)].amplifier,
                                                 gifts[size_t(g)].duration);
                    }
                // Jukeboxes play their disc's tune note by note until the song's length (M23.6).
                for (auto& jb : c.jukeboxes()) {
                    if (!jb.data.playing) continue;
                    const int disc = mc::discIndex(jb.data.record.item);
                    if (disc < 0 || jb.data.ticks >= mc::discInfo(disc).lengthTicks) {
                        jb.data.playing = false;
                        c.markDirty();
                        litChanges.push_back({c.pos().x * 16 + jb.x, jb.y,
                                              c.pos().z * 16 + jb.z}); // (power off, below)
                        continue;
                    }
                    std::array<mc::JukeboxNote, 3> notes{};
                    const double jx = c.pos().x * 16 + jb.x + 0.5, jz = c.pos().z * 16 + jb.z + 0.5;
                    for (int k = 0, n = mc::jukeboxNotes(disc, jb.data.ticks, notes); k < n; ++k)
                        world.playSound(notes[size_t(k)].sound, jx, jb.y + 1.0, jz, 1.3f,
                                        notes[size_t(k)].pitch);
                    ++jb.data.ticks;
                }
                for (auto& br : c.brewingStands()) // brewing stands brew (M19.4)
                    if (mc::tickBrewing(br.data)) c.markDirty();
                // Campfires cook each item for 600 ticks, then drop it cooked above
                // (M23.4c; wiki: Campfire).
                for (auto& cf : c.campfires()) {
                    if (reg.get(c.get(cf.x, cf.y, cf.z), mc::world::properties::lit) != 0)
                        continue; // (out: no cooking)
                    std::array<mc::world::ItemStack, 4> done{};
                    const int n = mc::tickCampfire(cf.data, done);
                    for (int i = 0; i < n; ++i)
                        droppedItems.spawn(
                            {c.pos().x * 16 + cf.x + 0.5, cf.y + 1.0, c.pos().z * 16 + cf.z + 0.5},
                            done[size_t(i)], gameRng);
                    if (n > 0) c.markDirty();
                }
                for (auto& f : c.furnaces()) {
                    const bool changedLit = mc::tickFurnace(f.data);
                    if (f.data.lit() || f.data.cookTime > 0 || changedLit) c.markDirty();
                    if (changedLit)
                        litChanges.push_back({c.pos().x * 16 + f.x, f.y, c.pos().z * 16 + f.z});
                }
            });
            // Block states change after the loop (setBlock may touch the furnace lists).
            for (const mc::world::BlockPos& p : litChanges) {
                const auto state = world.getBlock(p);
                if (reg.blockOf(state) ==
                    mc::world::blocks::Jukebox) { // a song ended: its power stops
                    blockUpdates.jukeboxChanged(p);
                    continue;
                }
                if (reg.likeOf(reg.blockOf(state)) != mc::world::blocks::Furnace) continue;
                mc::world::Chunk* fc = world.chunk(p.chunk());
                const auto* f = fc ? fc->furnace(mc::world::blockToLocal(p.x), p.y,
                                                 mc::world::blockToLocal(p.z))
                                   : nullptr;
                if (!f) continue;
                world.setBlock(p,
                               reg.with(state, "lit", f->lit() ? "true" : "false").value_or(state));
                frameEdits.push_back(p); // relit, then re-meshed
            }
            // A barrel's lid shows open while its screen is (vanilla: open=true while viewed).
            {
                const bool viewing =
                    container.isOpen() &&
                    container.type() == mc::ui::ContainerScreen::Type::Chest &&
                    reg.blockOf(world.getBlock(containerBlock)) == mc::world::blocks::Barrel;
                if (openBarrel && (!viewing || *openBarrel != containerBlock)) {
                    const auto st = world.getBlock(*openBarrel);
                    if (reg.blockOf(st) == mc::world::blocks::Barrel) {
                        world.setBlock(*openBarrel, reg.with(st, "open", "false").value_or(st));
                        frameEdits.push_back(*openBarrel);
                        playSound(mc::world::Sound::ChestClose,
                                  {openBarrel->x + 0.5, openBarrel->y + 0.5, openBarrel->z + 0.5},
                                  1.0f, 1.0f, true);
                    }
                    openBarrel.reset();
                }
                if (viewing && !openBarrel) {
                    const auto st = world.getBlock(containerBlock);
                    world.setBlock(containerBlock, reg.with(st, "open", "true").value_or(st));
                    frameEdits.push_back(containerBlock);
                    openBarrel = containerBlock;
                }
            }
            { // A trapped chest powers while its screen is up (M29.5).
                const bool viewing = container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Chest &&
                                     reg.blockOf(world.getBlock(containerBlock)) == mc::world::blocks::TrappedChest;
                if (openTrapped && (!viewing || *openTrapped != containerBlock)) {
                    blockUpdates.setChestOpen(*openTrapped, false);
                    openTrapped.reset();
                }
                if (viewing && !openTrapped) {
                    blockUpdates.setChestOpen(containerBlock, true);
                    openTrapped = containerBlock;
                }
            }
            renderer.tick();
            // Particles (M22.3): this tick's level events, blocks animating around the
            // player, rain splashes, the player's effect swirls.
            if (!dead)
                for (const auto& e : vitals.effects())
                    if (e.duration > 0)
                        particles.effectSwirl(player.position(), 0.6, 1.8,
                                              mc::world::effectInfo(e.type).colour, particleRng);
            particles.tick(world, world.levelEvents(), player.eyePosition(1.0),
                           dimension == Dimension::Overworld ? &weather : nullptr, particleRng);
            // Sounds (M22.4): level events that make a sound, then everything queued.
            for (const auto& e : world.levelEvents()) {
                using T = mc::world::LevelEvent::Type;
                using mc::world::BlockSound;
                const glm::dvec3 at(e.x, e.y, e.z), centre(e.x + 0.5, e.y + 0.5, e.z + 0.5);
                switch (e.type) {
                case T::BlockBreak:
                    playSound(
                        mc::world::blockSoundOf(mc::world::BlockStateId(e.data), BlockSound::Break),
                        centre, 1.0f, 1.0f, true);
                    break;
                case T::BlockPlace: {
                    playSound(
                        mc::world::blockSoundOf(mc::world::BlockStateId(e.data), BlockSound::Place),
                        centre, 1.0f, 1.0f, true);
                    // A carved pumpkin on a T of iron blocks makes an iron golem (M24.3).
                    if (mc::world::blockRegistry().likeOf(mc::world::blockRegistry().blockOf(mc::world::BlockStateId(e.data))) ==
                        mc::world::blocks::CarvedPumpkin) // (M29.4a: jack o'lanterns too)
                        if (mc::Mobs::buildIronGolem(
                                world,
                                {int(std::floor(e.x)), int(std::floor(e.y)), int(std::floor(e.z))},
                                gameRng))
                            announce(adv.onEvent(mc::world::AdvEvent::SummonedIronGolem));
                        else if (!mc::Mobs::buildSnowGolem( // (M29.1c: on two snow blocks)
                                     world, {int(std::floor(e.x)), int(std::floor(e.y)), int(std::floor(e.z))},
                                     gameRng)) // (M26.5b: on copper, a copper golem)
                            mc::Mobs::buildCopperGolem(
                                world,
                                {int(std::floor(e.x)), int(std::floor(e.y)), int(std::floor(e.z))},
                                gameRng);
                    // A wither skeleton skull topping a T of soul sand makes the Wither (M26.4b).
                    if (const auto pb =
                            mc::world::blockRegistry().blockOf(mc::world::BlockStateId(e.data));
                        pb == mc::world::blocks::WitherSkeletonSkull ||
                        pb == mc::world::blocks::WitherSkeletonWallSkull)
                        if (mc::Mobs::buildWither(
                                world,
                                {int(std::floor(e.x)), int(std::floor(e.y)), int(std::floor(e.z))},
                                gameRng))
                            announce(adv.onEvent(mc::world::AdvEvent::SummonedWither));
                    // A placed sign opens its editor (vanilla).
                    const auto kind = mc::world::blockRegistry().kind(
                        mc::world::blockRegistry().blockOf(mc::world::BlockStateId(e.data)));
                    const mc::world::BlockPos sp{int(e.x), int(e.y), int(e.z)};
                    if (!screenshotMode && (kind == mc::world::BlockKind::Sign ||
                                            kind == mc::world::BlockKind::HangingSign ||
                                            kind == mc::world::BlockKind::WallSign ||
                                            kind == mc::world::BlockKind::WallHangingSign))
                        if (mc::world::Chunk* sc = world.chunk(sp.chunk()))
                            if (const mc::world::SignData* sd =
                                    sc->sign(mc::world::blockToLocal(sp.x), sp.y,
                                             mc::world::blockToLocal(sp.z))) {
                                signEditor.open(sp, sd->front);
                                window.setCursorCaptured(false);
                            }
                    break;
                }
                case T::BlockHit: // (vanilla: every 4 ticks while mining)
                    if (gameTime % 4 == 0)
                        playSound(mc::world::blockSoundOf(mc::world::BlockStateId(e.data & 0xFFFF),
                                                          BlockSound::Hit),
                                  centre, 1.0f, 1.0f, true);
                    break;
                case T::Explosion:
                    playSound(mc::world::Sound::Explode, at, 1.0f, 1.0f, true);
                    break;
                case T::PotionSplash:
                    playSound(mc::world::Sound::GlassBreak, at, 1.0f, 1.0f, true);
                    break;
                case T::Crit:
                    playSound(mc::world::Sound::Crit, at, 1.0f, 1.0f, true);
                    break;
                case T::Portal:
                    playSound(mc::world::Sound::Teleport, at, 1.0f, 1.0f, true);
                    break;
                default:
                    break;
                }
            }
            // Vibrations (M27.3): blocks broken and placed (by the player), explosions.
            for (size_t ev = 0, n = world.levelEvents().size(); ev < n; ++ev) {
                const auto e = world.levelEvents()[ev];
                using T = mc::world::LevelEvent::Type;
                if (e.type == T::BlockBreak || e.type == T::BlockPlace)
                    blockUpdates.vibrate({e.x + 0.5, e.y + 0.5, e.z + 0.5}, true);
                else if (e.type == T::Explosion)
                    blockUpdates.vibrate({e.x, e.y, e.z}, false);
            }
            world.levelEvents().clear();
            for (const auto& s : world.soundEvents())
                playSound(s.sound, {s.x, s.y, s.z}, s.volume, s.pitch, true);
            world.soundEvents().clear();
            if (!dead) {
                const glm::dvec3 feetNow = player.position();
                // Footsteps: one per 1/0.6 blocks walked on the ground (vanilla moveDist
                // grows by 0.6 x the horizontal distance), the block underfoot's sound;
                // swimming splashes instead.
                if (player.onGround() && !player.flying()) {
                    stepDistance +=
                        glm::length(glm::dvec2(feetNow.x - lastFeet.x, feetNow.z - lastFeet.z)) *
                        0.6;
                    if (stepDistance > nextStep) {
                        nextStep = std::floor(stepDistance) + 1.0;
                        const mc::world::BlockPos under{int(std::floor(feetNow.x)),
                                                        int(std::floor(feetNow.y - 0.2)),
                                                        int(std::floor(feetNow.z))};
                        const auto us = world.getBlock(under);
                        if (us != 0 && !player.inWater())
                            playSound(mc::world::blockSoundOf(us, mc::world::BlockSound::Step),
                                      feetNow, 1.0f, 1.0f, false);
                        if (!player.sneaking())
                            blockUpdates.vibrate(feetNow, true); // (M27.3: sneaking is silent)
                    }
                } else if (player.inWater()) {
                    stepDistance += glm::length(feetNow - lastFeet) * 0.6;
                    if (stepDistance > nextStep) {
                        nextStep = std::floor(stepDistance) + 1.0;
                        playSound(mc::world::Sound::Swim, feetNow, 1.0f, 1.0f, false);
                    }
                }
                if (player.inWater() && !wasInWater && player.velocity().y < -0.1) // falling in
                    playSound(mc::world::Sound::Splash, feetNow, 1.0f, 1.0f, false);
                wasInWater = player.inWater();
                lastFeet = feetNow;
                if (survival && vitals.health() < lastHealth - 0.01f)
                    playSound(mc::world::Sound::PlayerHurt, feetNow, 1.0f, 1.0f, false);
                // Every 5th level a fanfare (vanilla: levels that are multiples of 5).
                if (vitals.xpLevel() > lastXpLevel && vitals.xpLevel() % 5 == 0)
                    playSound(mc::world::Sound::LevelUp, feetNow, 1.0f, 1.0f, false);
                // Eating and drinking: a bite every 4 ticks after the first 7, a burp
                // when a food is finished.
                const int eat = interaction.eatTicks();
                const auto& heldDef =
                    mc::world::itemRegistry().item(inventory.selectedStack().item);
                const bool drinkable =
                    heldDef.id == "minecraft:potion" || heldDef.id == "minecraft:milk_bucket";
                if (eat > 7 && eat % 4 == 0)
                    playSound(drinkable ? mc::world::Sound::Drink : mc::world::Sound::Eat, feetNow,
                              1.0f, 1.0f, false);
                if (eat == 0 && lastEatTicks >= 30 && !drinkable)
                    playSound(mc::world::Sound::Burp, feetNow, 1.0f, 1.0f, false);
                lastEatTicks = eat;
            }
            lastHealth = vitals.health();
            lastXpLevel = vitals.xpLevel();
            // Rain on the ground nearby (vanilla's weather sounds: a spot within 10
            // blocks that rain reaches, quieter when it's above the player).
            if (dimension == Dimension::Overworld && weather.rain > 0.2f && ++rainSoundTime >= 10) {
                rainSoundTime = 0;
                const glm::dvec3 eye = player.eyePosition(1.0);
                const int x = int(std::floor(eye.x)) + int(soundRng.nextInt(21)) - 10;
                const int z = int(std::floor(eye.z)) + int(soundRng.nextInt(21)) - 10;
                const int top = mc::world::rainHeight(world, x, z);
                if (mc::world::rainFallsOn(world, weather, {x, top, z}) &&
                    std::abs(top - eye.y) < 20.0)
                    playSound(mc::world::Sound::Rain, {x + 0.5, double(top), z + 0.5},
                              top > eye.y + 1.0 ? 0.5f : 1.0f, top > eye.y + 1.0 ? 0.5f : 1.0f,
                              true);
            }
            // A lodestone compass whose lodestone is gone loses its target (M28.2a; wiki:
            // Lodestone - the needle then spins). Checked once a second.
            if (gameTime % 20 == 0)
                for (int slot = 0; slot < mc::Inventory::kSlots; ++slot) {
                    const mc::world::ItemStack& s = inventory.slot(slot);
                    if (!s.extra || s.empty()) continue;
                    const auto t = mc::world::lodestoneTarget(s.extra);
                    if (!t || !t->hasTarget || !t->tracked || t->dimension != uint8_t(dimension) ||
                        !world.chunk(t->pos.chunk()) ||
                        mc::world::blockRegistry().blockOf(world.getBlock(t->pos)) ==
                            mc::world::blocks::Lodestone)
                        continue;
                    mc::world::LodestoneTarget lost = *t;
                    lost.hasTarget = false;
                    mc::world::ItemStack copy = s;
                    copy.extra = mc::world::addLodestoneTarget(lost);
                    inventory.setSlot(slot, copy);
                }
            { // Maps (M28.2b): new map data for maps a table zoomed out or locked, then the
              // held map draws the terrain around the player.
                static const mc::world::ItemId filledMap =
                    *mc::world::itemRegistry().find("filled_map");
                for (int slot = 0; slot <= mc::Inventory::kSlots;
                     ++slot) { // (the last: the offhand)
                    const bool off = slot == mc::Inventory::kSlots;
                    const mc::world::ItemStack& s =
                        off ? inventory.offhand() : inventory.slot(slot);
                    if (s.item != filledMap || s.state == 0 || s.empty()) continue;
                    mc::world::ItemStack done = s;
                    done.state = 0;
                    if (const mc::world::MapData* old = maps.get(s.damage)) {
                        const mc::world::MapData from = *old; // (create() may move the map store)
                        if (s.state == mc::kMapScale && from.scale < 4 && !from.locked) {
                            done.damage = uint16_t(maps.create(from.centerX, from.centerZ,
                                                               from.scale + 1, from.dimension));
                        } else if (s.state == mc::kMapLock && !from.locked) {
                            done.damage = uint16_t(maps.create(from.centerX, from.centerZ,
                                                               from.scale, from.dimension));
                            mc::world::MapData* copy = maps.get(done.damage);
                            copy->colors = from.colors;
                            copy->centerX = from.centerX; // (the same place exactly)
                            copy->centerZ = from.centerZ;
                            copy->locked = true;
                        }
                    }
                    if (off)
                        inventory.setOffhand(done);
                    else
                        inventory.setSlot(slot, done);
                }
                // (a map whose data doesn't exist - given by a command - starts where it is held)
                if (inventory.selectedStack().item == filledMap &&
                    !maps.get(inventory.selectedStack().damage)) {
                    mc::world::ItemStack s = inventory.selectedStack();
                    s.damage = uint16_t(maps.create(int(std::floor(player.position().x)),
                                                    int(std::floor(player.position().z)), 0,
                                                    uint8_t(dimension)));
                    inventory.setSlot(inventory.selected(), s);
                }
                for (const mc::world::ItemStack* held :
                     {&inventory.selectedStack(), &inventory.offhand()})
                    if (held->item == filledMap && !dead)
                        if (mc::world::MapData* md = maps.get(held->damage);
                            md && md->dimension == uint8_t(dimension))
                            mc::world::Maps::update(world, *md, player.position(), gameTime);
            }
            stats.add(mc::world::Stat::PlayTime); // (M28.1d)
            // Advancements (M28.5c): the dimension entered, effects, and once a second what
            // the player carries (vanilla's inventory_changed trigger, polled).
            if (dimension != advDimension) {
                advDimension = dimension;
                if (dimension == Dimension::Nether)
                    announce(adv.onEvent(mc::world::AdvEvent::EnterNether));
                if (dimension == Dimension::End)
                    announce(adv.onEvent(mc::world::AdvEvent::EnterEnd));
            }
            if (vitals.effectLevel(mc::world::Effect::Levitation) > 0)
                announce(adv.onEvent(mc::world::AdvEvent::Levitated));
            if (vitals.effectLevel(mc::world::Effect::HeroOfTheVillage) > 0)
                announce(adv.onEvent(mc::world::AdvEvent::HeroOfTheVillage));
            if (dragonFight.respawning()) announce(adv.onEvent(mc::world::AdvEvent::RespawnDragon));
            if (gameTime % 20 == 0) {
                auto carried = [&](const mc::world::ItemStack& s) {
                    if (s.empty()) return;
                    announce(adv.onItem(mc::world::itemRegistry().item(s.item).id));
                    if (s.trim != 0) announce(adv.onEvent(mc::world::AdvEvent::TrimmedArmor));
                    if (s.potion > uint8_t(mc::world::Potion::Water))
                        announce(adv.onEvent(mc::world::AdvEvent::BrewedPotion));
                };
                for (int s = 0; s < 36; ++s)
                    carried(inventory.slot(s));
                for (int p = 0; p < 4; ++p)
                    carried(inventory.armor(p));
                carried(inventory.offhand());
                if (ridingCart != 0)
                    if (const mc::world::MobData* steed = findCart(); steed && steed->tamed)
                        announce(adv.onEvent(
                            mc::world::AdvEvent::Tamed)); // (a horse tamed by riding it)
            }
            if (toastTicks > 0) --toastTicks;
            if (toastTicks == 0 && toastQueued > 0) { // (the next toast)
                toastAdv = toastQueue[0];
                std::copy(toastQueue.begin() + 1, toastQueue.end(), toastQueue.begin());
                --toastQueued;
                toastTicks = 100;
            } else if (toastTicks == 0) {
                toastAdv = -1;
            }
            stats.add(mc::world::Stat::TotalWorldTime);
            if (!dead) stats.add(mc::world::Stat::TimeSinceDeath);
            stats.set(mc::world::Stat::TimeSinceRest, vitals.timeSinceRest());
            stats.add(mc::world::Stat::DamageTaken, std::lround(vitals.takeDamageTaken() * 10.0f));
            for (const mc::world::MobType k : mobs.playerKills()) {
                stats.add(mc::world::Stat::MobKills);
                stats.addKilled(k);
                announce(adv.onKill(k));
                if (k == mc::world::MobType::EnderDragon)
                    announce(adv.onEvent(mc::world::AdvEvent::KillDragon));
            }
            mobs.clearPlayerKills();
            if (const int bred = mobs.takeBred(); bred > 0) {
                stats.add(mc::world::Stat::AnimalsBred, bred);
                announce(adv.onEvent(mc::world::AdvEvent::Bred));
            }
            if (mc::Mobs::takeCured() > 0)
                announce(adv.onEvent(mc::world::AdvEvent::CuredZombieVillager));
            if (interaction.takeAte()) announce(adv.onEvent(mc::world::AdvEvent::Ate));
            for (const mc::world::ItemStack& p : droppedItems.pickedUp())
                stats.addItem(mc::world::ItemStat::PickedUp, p.item, p.count);
            if (const auto b = interaction.takeBroken()) stats.addMined(b);
            if (const auto u = interaction.takeUsed()) {
                stats.addItem(mc::world::ItemStat::Used, u);
                // Planting (A Seedy Place / Planting the Past): a seed item placed its crop.
                static const std::array<mc::world::ItemId, 6> seeds{
                    *mc::world::itemRegistry().find("wheat_seeds"),
                    *mc::world::itemRegistry().find("beetroot_seeds"),
                    *mc::world::itemRegistry().find("melon_seeds"),
                    *mc::world::itemRegistry().find("pumpkin_seeds"),
                    mc::world::itemRegistry().find("torchflower_seeds").value_or(0),
                    mc::world::itemRegistry().find("pitcher_pod").value_or(0)};
                for (size_t si = 0; si < seeds.size(); ++si)
                    if (seeds[si] != 0 && u == seeds[si]) {
                        announce(adv.onEvent(mc::world::AdvEvent::Planted));
                        if (si >= 4) announce(adv.onEvent(mc::world::AdvEvent::PlantedSnifferSeed));
                    }
            }
            if (const auto t = interaction.takeBrokenTool())
                stats.addItem(mc::world::ItemStat::Broken, t);
            if (rules.advanceTime) ++dayTime; // the daylight cycle advances one tick per tick
            if (rules.advanceWeather) weather.tick(gameRng);
            if (skyFlash > 0) --skyFlash;
            for (Bolt& b : bolts)
                --b.ticks;
            std::erase_if(bolts, [](const Bolt& b) { return b.ticks <= 0; });
            ++gameTime;
            if (pearlCooldown > 0) --pearlCooldown;
            if (hornCooldown > 0) --hornCooldown;
            if (windCooldown > 0) --windCooldown;
            if (!dead && survival) vitals.addRestTime(); // (M26.4a: insomnia - phantoms)
            vitals.tickWardenTracker();                  // (M27.3)
            // Vanilla autosave: every 6000 ticks (5 minutes) of play.
            if (++sessionTicks % 6000 == 0) {
                blockUpdates.landAll(); // (blocks in flight land before saving)
                saveWorld(false);
            }
        }

        int fbWidth = 0;
        int fbHeight = 0;
        window.framebufferSize(fbWidth, fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            // Minimised: swapBuffers may not block, so sleep instead of spinning a core.
            window.waitEvents(0.05);
            continue;
        }

        if (openBlockPending && gameTime > 2) { // --open-block (screenshots of container screens)
            openBlockPending = false;
            const mc::world::BlockPos ob{opts->openBlock[0], opts->openBlock[1],
                                         opts->openBlock[2]};
            const auto obBlock = mc::world::blockRegistry().blockOf(world.getBlock(ob));
            containerBlock = ob;
            mc::world::Chunk* obc = world.chunk(ob.chunk());
            const mc::world::SignData* obSign =
                obc ? obc->sign(mc::world::blockToLocal(ob.x), ob.y, mc::world::blockToLocal(ob.z))
                    : nullptr;
            if (obSign) // (M23.3c: the sign editor)
                signEditor.open(ob, obSign->front);
            else if (obBlock == mc::world::blocks::Stonecutter)
                container.open(mc::ui::ContainerScreen::Type::Stonecutter);
            else if (obBlock == mc::world::blocks::Grindstone)
                container.open(mc::ui::ContainerScreen::Type::Grindstone);
            else if (obBlock == mc::world::blocks::SmithingTable)
                container.open(mc::ui::ContainerScreen::Type::Smithing);
            else if (obBlock == mc::world::blocks::Beacon)
                container.openBeacon(obc ? obc->beacon(mc::world::blockToLocal(ob.x), ob.y,
                                                       mc::world::blockToLocal(ob.z))
                                         : nullptr);
            else if (obBlock == mc::world::blocks::EnchantingTable)
                container.openEnchanting(mc::countBookshelves(world, ob), vitals.enchantSeed());
            else if (obBlock == mc::world::blocks::Anvil ||
                     obBlock == mc::world::blocks::ChippedAnvil ||
                     obBlock == mc::world::blocks::DamagedAnvil)
                container.openAnvil();
            else if (obBlock == mc::world::blocks::BrewingStand) {
                mc::world::Chunk* bc = world.chunk(ob.chunk());
                container.openBrewing(bc ? bc->brewing(mc::world::blockToLocal(ob.x), ob.y,
                                                       mc::world::blockToLocal(ob.z))
                                         : nullptr);
            } else if (!openStoreAt(ob)) {
                openChestAt(ob);
            }
        }
        if (bookPending && gameTime > 2) { // --book TEXT (screenshots of the book screen)
            bookPending = false;
            const mc::world::ItemStack& b = inventory.selectedStack();
            const std::string_view id = mc::world::itemRegistry().item(b.item).id;
            if (id == "minecraft:writable_book" || id == "minecraft:written_book") {
                const auto content =
                    mc::world::bookContent(b.extra).value_or(mc::world::BookContent{});
                if (id == "minecraft:writable_book")
                    bookScreen.openEdit(content, inventory.selected());
                else
                    bookScreen.openRead(content);
                bookScreen.type(opts->bookText, gui.batch().font());
                window.setCursorCaptured(false);
            }
        }
        if (tradePending && gameTime > 2) { // --trade (screenshots of the trading screen)
            mc::world::MobData* best = nullptr;
            double bestD = 8.0 * 8.0;
            world.forEachChunk([&](mc::world::Chunk& c) {
                for (auto& mob : c.mobs())
                    if ((mob.type == mc::world::MobType::Villager ||
                         mob.type == mc::world::MobType::WanderingTrader) &&
                        mob.offerCount > 0) {
                        const double d =
                            glm::dot(mob.pos - player.position(), mob.pos - player.position());
                        if (d < bestD) bestD = d, best = &mob;
                    }
            });
            if (best) {
                tradePending = false;
                traderUuid = best->uuidHi;
                container.openTrading(best);
            }
        }
        if (mountPending &&
            gameTime > 2) { // --mount (M26.2: screenshots of riding / a mount's screen)
            mc::world::MobData* best = nullptr;
            double bestD = 10.0 * 10.0;
            world.forEachChunk([&](mc::world::Chunk& c) {
                for (auto& mob : c.mobs())
                    if ((mc::world::isMount(mob.type) || (mc::world::isStickRidden(mob.type) && mob.saddled)) && !mob.isBaby()) {
                        const double d =
                            glm::dot(mob.pos - player.position(), mob.pos - player.position());
                        if (d < bestD) bestD = d, best = &mob;
                    }
            });
            if (best) {
                mountPending = false;
                best->ridden = true;
                best->tameCheck = 30000; // (a wild one stays calm for the picture)
                ridingCart = best->uuidHi;
                if (openInventoryPending) {
                    openInventoryPending = false;
                    openMountScreen(best->uuidHi);
                }
            }
        }
        if (openInventoryPending && gameTime > 0 && !mountPending) {
            openInventoryPending = false;
            if (survival)
                container.open(mc::ui::ContainerScreen::Type::Inventory);
            else
                creative.open();
        }
        if (spawnPending) { // new player: stand on solid ground once it's generated
            if (const auto safe = settleSpawn(world, spawn)) {
                player.setPosition(*safe);
                spawnPending = false;
            }
        }
        mc::gfx::Camera camera;
        camera.fovDegrees = spyglassUp ? shared.options.fov * 0.1f : shared.options.fov;
        camera.position = player.eyePosition(clock.alpha);
        camera.yaw = player.yaw();
        audio.setListener(camera.position, camera.yaw);
        camera.pitch = player.pitch();
        const float animAlpha = float(clock.alpha);
        // View bobbing (M30.1; vanilla GameRenderer.bobView): each step sways the view a
        // little sideways and down, and nods it - first person, on the ground.
        glm::vec2 handBob(0.0f);
        if (shared.options.bobView && perspective == 0 && !player.flying()) {
            const float walk = playerAnim.walkPhase(animAlpha) * 3.14159265f, bob = playerAnim.bobAmount(animAlpha);
            handBob = {std::sin(walk) * bob * 0.5f, -std::abs(std::cos(walk) * bob)};
            const glm::vec3 fwd = mc::world::lookVector(camera.yaw, camera.pitch);
            const glm::vec3 rightAxis = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
            const glm::vec3 upAxis = glm::cross(rightAxis, fwd);
            camera.position += glm::dvec3(rightAxis * handBob.x + upAxis * handBob.y);
            camera.pitch += std::abs(std::cos(walk - 0.2f) * bob) * 5.0f;
        }
        // Third person (M30.1, F5): the camera backs away behind the player, or in front
        // looking back, stopping short of blocks (vanilla Camera.getMaxZoom).
        if (perspective != 0 && gameMode != 3) {
            const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
            const glm::dvec3 back = perspective == 1 ? -look : look;
            camera.position += back * mc::thirdPersonDistance(world, camera.position, back);
            if (perspective == 2) {
                camera.yaw += 180.0f;
                camera.pitch = -camera.pitch;
            }
        }
        // Nausea (M29.2a; wiki: Nausea): the view sways and the field of view breathes
        // (ours: a sway in place of vanilla's warping screen).
        if (const int nauseaTicks = vitals.effectTicks(mc::world::Effect::Nausea); nauseaTicks > 0) {
            const float k = std::min(1.0f, float(nauseaTicks) / 40.0f);
            const double t = double(gameTime) + clock.alpha;
            camera.yaw += float(std::sin(t * 0.11) * 4.0 * k);
            camera.pitch += float(std::sin(t * 0.17) * 3.0 * k);
            camera.fovDegrees += float(std::sin(t * 0.13) * 9.0 * k);
        }
        { // Blindness (M29.2a): eased in and out over its last second
            const int blindTicks = vitals.effectTicks(mc::world::Effect::Blindness);
            renderer.setBlindness(blindTicks > 0 ? std::min(1.0f, float(blindTicks) / 20.0f) : 0.0f);
        }
        if (dimension == Dimension::Nether) { // fog of the Nether biome at the camera, eased in
            const mc::world::BlockPos cam{int(std::floor(camera.position.x)),
                                          int(std::floor(camera.position.y)),
                                          int(std::floor(camera.position.z))};
            if (const mc::world::Chunk* c = world.chunk(cam.chunk()); c && c->biomes()) {
                const uint32_t fog =
                    mc::world::biomeInfo(c->biomes()->at(mc::world::blockToLocal(cam.x), cam.y,
                                                         mc::world::blockToLocal(cam.z),
                                                         world.height()))
                        .fog;
                if (fog) {
                    const glm::vec3 target(float(fog >> 16) / 255.0f,
                                           float((fog >> 8) & 255) / 255.0f,
                                           float(fog & 255) / 255.0f);
                    // (3% per 1/60 s, whatever the frame rate)
                    netherFog = glm::mix(netherFog, target,
                                         1.0f - std::pow(0.97f, float(frameSeconds * 60.0)));
                }
            }
            renderer.setNetherFog(netherFog);
        }
        { // Darkness (M27.3): the dark pulses while it lasts.
            const int darkTicks = vitals.effectTicks(mc::world::Effect::Darkness);
            const float pulse =
                0.65f + 0.3f * float(std::sin((double(gameTime) + clock.alpha) * 0.08));
            renderer.setDarkness(darkTicks > 0 ? pulse * std::min(1.0f, darkTicks / 20.0f) : 0.0f);
        }
        renderer.setNightVision(vitals.effectLevel(mc::world::Effect::NightVision) > 0 ||
                                vitals.effectLevel(mc::world::Effect::ConduitPower) >
                                    0); // (conduits: vision too)
        if (dimension == Dimension::Overworld) {
            // Biome sky and fog colours, blended over 5x5 biome cells (4 blocks each)
            // around the camera, so crossing a border fades the sky (vanilla samples
            // its biome grid with a smooth kernel).
            glm::vec3 sky(0.0f), fog(0.0f);
            int n = 0;
            const int bx = int(std::floor(camera.position.x)),
                      bz = int(std::floor(camera.position.z));
            const int by = std::clamp(int(std::floor(camera.position.y)), world.height().minY,
                                      world.height().maxY());
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = -2; dx <= 2; ++dx) {
                    const mc::world::BlockPos p{bx + dx * 4, by, bz + dz * 4};
                    const mc::world::Chunk* c = world.chunk(p.chunk());
                    if (!c || !c->biomes()) continue;
                    const auto& info = mc::world::biomeInfo(
                        c->biomes()->at(mc::world::blockToLocal(p.x), p.y,
                                        mc::world::blockToLocal(p.z), world.height()));
                    auto rgb = [](uint32_t v) {
                        return glm::vec3(float((v >> 16) & 255), float((v >> 8) & 255),
                                         float(v & 255)) /
                               255.0f;
                    };
                    sky += rgb(info.sky ? info.sky : mc::world::skyColorFor(info.temperature));
                    fog += rgb(info.fog ? info.fog
                                        : mc::world::kOverworldFog); // (M27.1: the pale garden's)
                    ++n;
                }
            if (n > 0) renderer.setBiomeSky(sky / float(n), fog / float(n));
        }
        {
            // Under water: the biome's water fog (M25.1). Vanilla's dark fog colours are
            // lifted toward the biome's water colour so daylight water reads blue.
            const mc::world::BlockPos eye{int(std::floor(camera.position.x)),
                                          int(std::floor(camera.position.y)),
                                          int(std::floor(camera.position.z))};
            // (the water's surface height counts, and waterlogged cells: review fix)
            const bool under = mc::pointInFluid(world, camera.position, mc::world::blocks::Water);
            glm::vec3 waterFog(0.0f);
            if (under)
                if (const mc::world::Chunk* c = world.chunk(eye.chunk()); c && c->biomes()) {
                    const mc::world::Biome b =
                        c->biomes()->at(mc::world::blockToLocal(eye.x), eye.y,
                                        mc::world::blockToLocal(eye.z), world.height());
                    auto rgb = [](uint32_t v) {
                        return glm::vec3(float((v >> 16) & 255), float((v >> 8) & 255),
                                         float(v & 255)) /
                               255.0f;
                    };
                    waterFog = glm::mix(rgb(mc::world::waterFogColor(b)),
                                        rgb(mc::world::biomeInfo(b).water) * 0.55f, 0.6f);
                }
            renderer.setUnderwater(under, waterFog);
        }
        {
            const bool overworld = dimension == Dimension::Overworld;
            const float a = static_cast<float>(clock.alpha);
            renderer.setCloudTime(double(gameTime) + double(a));
            renderer.setDayTime(dayTime, a, overworld ? weather.rainAt(a) : 0.0f,
                                overworld ? weather.thunderAt(a) : 0.0f);
            if (overworld && skyFlash > 0) renderer.setSkyFlash(); // (a bolt lights everything up)
        }
        if (loader) {
            const mc::world::ChunkPos center{
                mc::world::blockToChunk(static_cast<int32_t>(std::floor(camera.position.x))),
                mc::world::blockToChunk(static_cast<int32_t>(std::floor(camera.position.z)))};
            loader->setGameTime(gameTime);
            loader->update(center, loadedChunks, unloadedChunks);
        }
        // Lighting follows loading and edits; meshing follows lighting.
        lighting.update(loadedChunks, unloadedChunks, frameEdits, frameSettling, litChunks,
                        relitSections, editsReady);
        frameSettling.clear();
        // Edited blocks are re-meshed once their light is current (no stale-light flash).
        renderer.onBlocksChanged(editsReady);
        renderer.onBlocksChanged(frameRemesh);
        frameRemesh.clear();
        renderer.onChunksUnloaded(unloadedChunks);
        renderer.onChunksLit(world, litChunks);
        renderer.onLightChanged(relitSections);
        loadedChunks.clear();
        unloadedChunks.clear();
        frameEdits.clear();
        renderer.update(world, camera.position);
        renderer.drawFrame(camera, fbWidth, fbHeight);

        // Targeted block: from the eye along the look direction (reach by game mode).
        static const mc::world::ItemId lightItem = mc::world::itemRegistry().find("light").value_or(0);
        const auto hit = mc::world::raycastBlocks( // (from the eye, whatever the view - M30.1)
            world, player.eyePosition(clock.alpha), glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())),
            survival ? mc::world::kSurvivalReach : mc::world::kCreativeReach, mc::world::RayFluids::Skip,
            lightItem != 0 && inventory.selectedStack().item == lightItem);
        // Dropped items and the breaking crack.
        // Light colours for all 16x16 sky/block levels this frame (night changes them).
        std::array<glm::vec3, 256> lightTable;
        for (int sky = 0; sky < 16; ++sky)
            for (int blk = 0; blk < 16; ++blk)
                lightTable[size_t(sky * 16 + blk)] = mc::gfx::lightColor(
                    sky, blk, renderer.skyDarken(),
                    mc::world::dimensionInfo(dimension).ambientLight, dimension == Dimension::End);
        for (const auto& e : droppedItems.items()) {
            const glm::dvec3 p = glm::mix(e.prevPos, e.pos, clock.alpha);
            const float t = float(e.age) + float(clock.alpha);
            entities.addItem(e.stack, p, t / 20.0f + e.spinOffset,
                             std::sin(t / 10.0f + e.spinOffset) * 0.1f + 0.1f,
                             lightTable[size_t(e.skyLight * 16 + e.blockLight)], camera.position);
        }
        if (fishing.active()) { // the bobber (red over white) and the line to the rod (M25.2)
            const glm::dvec3 b = glm::mix(fishing.prevBobber(), fishing.bobber(), clock.alpha);
            const glm::dvec3 hand =
                camera.position +
                glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())) * 0.6 +
                glm::dvec3(0.0, -0.35, 0.0);
            entities.addBeam(hand, b + glm::dvec3(0.0, 0.18, 0.0), camera.position,
                             {0.1f, 0.1f, 0.1f}, 0.008f);
            entities.addBeam(b, b + glm::dvec3(0.0, 0.09, 0.0), camera.position, {0.9f, 0.9f, 0.9f},
                             0.06f);
            entities.addBeam(b + glm::dvec3(0.0, 0.09, 0.0), b + glm::dvec3(0.0, 0.18, 0.0),
                             camera.position, {0.85f, 0.15f, 0.1f}, 0.06f);
        }
        for (const auto& pr : projectiles.items()) {
            const glm::dvec3 p = glm::mix(pr.prevPos, pr.pos, clock.alpha);
            const glm::vec3 light = lightTable[size_t(pr.skyLight * 16 + pr.blockLight)];
            static const mc::world::ItemId eggItem = *mc::world::itemRegistry().find("egg");
            static const mc::world::ItemId eyeItem = *mc::world::itemRegistry().find("ender_eye");
            static const mc::world::ItemId splashItem =
                *mc::world::itemRegistry().find("splash_potion");
            static const mc::world::ItemId fireItem =
                *mc::world::itemRegistry().find("fire_charge");
            static const mc::world::ItemId pearlItem =
                *mc::world::itemRegistry().find("ender_pearl");
            static const mc::world::ItemId shellItem =
                *mc::world::itemRegistry().find("shulker_shell");
            // (M26.2-M26.5: spit, wither skulls, wind charges, snowballs)
            static const mc::world::ItemId snowItem = *mc::world::itemRegistry().find("snowball");
            static const mc::world::ItemId windItem =
                *mc::world::itemRegistry().find("wind_charge");
            static const mc::world::ItemId xpBottleItem = *mc::world::itemRegistry().find("experience_bottle");
            static const mc::world::ItemId rocketItem =
                *mc::world::itemRegistry().find("firework_rocket");
            static const mc::world::ItemId skullItem =
                *mc::world::itemRegistry().find("wither_skeleton_skull");
            // (dragon fireballs too)
            if (pr.kind == mc::ProjectileKind::Arrow)
                entities.addArrow(p, pr.facing, light, camera.position);
            else if (pr.kind == mc::ProjectileKind::Trident) // (its item, like a dropped one)
                entities.addItem(pr.stack, p - glm::dvec3(0, 0.1, 0), 0.0f, 0.0f, light,
                                 camera.position);
            else {
                mc::world::ItemStack look{pr.kind == mc::ProjectileKind::EyeOfEnder ? eyeItem
                                          : pr.kind == mc::ProjectileKind::SplashPotion ||
                                                  pr.kind == mc::ProjectileKind::LingeringPotion
                                              ? splashItem
                                          : pr.kind == mc::ProjectileKind::Egg           ? eggItem
                                          : pr.kind == mc::ProjectileKind::EnderPearl    ? pearlItem
                                          : pr.kind == mc::ProjectileKind::ShulkerBullet ? shellItem
                                          : pr.kind == mc::ProjectileKind::Snowball ||
                                                  pr.kind == mc::ProjectileKind::LlamaSpit
                                              ? snowItem
                                          : pr.kind == mc::ProjectileKind::WindCharge  ? windItem
                                          : pr.kind == mc::ProjectileKind::ExperienceBottle ? xpBottleItem
                                          : pr.kind == mc::ProjectileKind::Firework    ? rocketItem
                                          : pr.kind == mc::ProjectileKind::WitherSkull ? skullItem
                                                                                       : fireItem,
                                          1};
                look.potion = pr.potion;
                entities.addItem(look, p - glm::dvec3(0, 0.1, 0), 0.0f, 0.0f, light,
                                 camera.position);
            }
        }
        if (dimension == Dimension::Overworld) {
            // Rain and snow (M22.1; vanilla's weather layer): columns within 10 blocks
            // (Fancy), from the higher of the ground and 10 below the eye to 10 above,
            // fading toward the edge; each column has its own speed and phase.
            const float a = static_cast<float>(clock.alpha);
            const float rain = weather.rainAt(a);
            if (rain > 0.0f) {
                constexpr int r = kRainRadius;
                const int cx = int(std::floor(camera.position.x)),
                          cz = int(std::floor(camera.position.z)),
                          cy = int(std::floor(camera.position.y));
                if (rainColumnsTick != gameTime || cx != rainCx || cz != rainCz || cy != rainCy) {
                    rainColumnsTick = gameTime;
                    rainCx = cx;
                    rainCy = cy;
                    rainCz = cz;
                    for (int dz = -r; dz <= r; ++dz)
                        for (int dx = -r; dx <= r; ++dx) {
                            RainColumn& col = rainColumns[size_t((dz + r) * (2 * r + 1) + dx + r)];
                            const int x = cx + dx, z = cz + dz;
                            col.ground = mc::world::rainHeight(world, x, z);
                            const mc::world::BlockPos at{x, std::max(col.ground, cy), z};
                            col.kind = mc::world::precipitationAt(world, at);
                            int sky = 15, blk = 0;
                            const mc::world::Chunk* c = world.chunk(at.chunk());
                            if (c && c->lit() && world.isInHeight(at.y)) {
                                sky = c->skyLight(mc::world::blockToLocal(x), at.y,
                                                  mc::world::blockToLocal(z));
                                blk = c->blockLight(mc::world::blockToLocal(x), at.y,
                                                    mc::world::blockToLocal(z));
                            }
                            col.light = uint8_t(sky * 16 + blk);
                        }
                }
                const float t = float(gameTime) + a;
                for (int dz = -r; dz <= r; ++dz)
                    for (int dx = -r; dx <= r; ++dx) {
                        const int d2 = dx * dx + dz * dz;
                        if (d2 > r * r) continue;
                        const int x = cx + dx, z = cz + dz;
                        const RainColumn& col =
                            rainColumns[size_t((dz + r) * (2 * r + 1) + dx + r)];
                        const int ground = col.ground;
                        const int y0 = std::max(ground, cy - r), y1 = std::max(ground, cy + r);
                        if (y0 >= y1) continue;
                        const auto kind = col.kind;
                        if (kind == mc::world::Precipitation::None) continue;
                        const uint32_t ux = uint32_t(x), uz = uint32_t(z);
                        const uint32_t h =
                            ux * 3121u + ux * ux * 45238971u + uz * uz * 418711u + uz * 13761u;
                        const float phase = float(h & 31) / 32.0f,
                                    speed = 1.0f + float((h >> 5) & 7) / 16.0f;
                        const bool snow = kind == mc::world::Precipitation::Snow;
                        // Rain: about a block a tick; snow drifts down slowly, swaying.
                        const float scroll =
                            snow ? t * 0.05f * speed + phase : t * speed + phase; // (grows: falls)
                        const float drift =
                            snow ? 0.15f * std::sin(t * 0.03f + phase * 6.28f) : 0.0f;
                        const float alpha =
                            ((1.0f - float(d2) / float(r * r)) * 0.5f + 0.5f) * rain;
                        entities.addPrecipitation(x, z, y0, y1, snow, scroll, drift, alpha,
                                                  lightTable[col.light], camera.position);
                    }
            }
            for (const Bolt& b : bolts)
                entities.addLightning(b.pos, b.seed, camera.position);
        }
        {
            // Particles: camera-facing squares (vanilla turns them with the camera).
            const glm::vec3 look = mc::world::lookVector(camera.yaw, camera.pitch);
            const glm::vec3 right = glm::normalize(glm::cross(look, glm::vec3(0, 1, 0)));
            const glm::vec3 up = glm::cross(right, look);
            for (const mc::Particle& p : particles.all()) {
                const glm::vec3 light = p.fullBright
                                            ? glm::vec3(1.0f)
                                            : lightTable[size_t(p.skyLight * 16 + p.blockLight)];
                entities.addParticle(glm::mix(p.prevPos, p.pos, clock.alpha), p.size, p.frame(),
                                     p.state, p.u, p.v, p.color * light, camera.position, right,
                                     up);
            }
        }
        {
            // Sign text (M23.3c): the front lines of signs within 3 chunks, on the board's
            // face - 1/96 block per font pixel, lines 10 pixels apart (vanilla's sign
            // text scale), in the sign's dye colour.
            static constexpr uint32_t kTextColours[16] = {
                0xFFFFFF, 0xFF681F, 0xFF00FF, 0x9AC0CD, 0xFFFF00, 0xBFFF00, 0xFF69B4, 0x808080,
                0xD3D3D3, 0x00FFFF, 0xA020F0, 0x0000FF, 0x8B4513, 0x00FF00, 0xFF0000, 0x000000};
            const auto& reg = mc::world::blockRegistry();
            const mc::world::ChunkPos cc{
                mc::world::blockToChunk(int32_t(std::floor(camera.position.x))),
                mc::world::blockToChunk(int32_t(std::floor(camera.position.z)))};
            // Only chunks in view (M23 perf review: no glyphs behind the camera).
            const mc::gfx::Frustum textFrustum = mc::gfx::Frustum::fromMatrix(
                camera.viewProjectionAtOrigin(float(fbWidth) / float(fbHeight)));
            for (int dz = -3; dz <= 3; ++dz)
                for (int dx = -3; dx <= 3; ++dx) {
                    const mc::world::Chunk* ch = world.chunk({cc.x + dx, cc.z + dz});
                    if (!ch) continue;
                    const glm::vec3 cmin(
                        glm::dvec3(ch->pos().x * 16.0, ch->height().minY, ch->pos().z * 16.0) -
                        camera.position);
                    if (!textFrustum.intersectsBox(
                            cmin, cmin + glm::vec3(16.0f, float(ch->height().height), 16.0f)))
                        continue;
                    for (const auto& cf : ch->campfires()) // food cooking on campfires (M23.4c)
                        for (int i = 0; i < 4; ++i)
                            if (!cf.data.items[size_t(i)].empty()) {
                                static constexpr double kSlot[4][2] = {
                                    {0.3, 0.3}, {0.7, 0.3}, {0.7, 0.7}, {0.3, 0.7}};
                                const glm::dvec3 at(ch->pos().x * 16 + cf.x + kSlot[i][0],
                                                    cf.y + 0.45,
                                                    ch->pos().z * 16 + cf.z + kSlot[i][1]);
                                entities.addItem(cf.data.items[size_t(i)], at, float(i) * 1.5707963f, // (radians)
                                                 0.0f, lightTable[15 * 16 + 15], camera.position);
                            }
                    for (const auto& sh : ch->chests()) { // (M29.6) a shelf's three stacks on its front
                        if (!sh.data.shelf) continue;
                        const auto st = ch->get(sh.x, sh.y, sh.z);
                        const auto& sreg = mc::world::blockRegistry();
                        if (sreg.likeOf(sreg.blockOf(st)) != mc::world::blocks::Shelf) continue;
                        const int f = sreg.get(st, mc::world::properties::facing); // north, south, west, east
                        static constexpr int kOut[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
                        static constexpr int kRight[4][2] = {{-1, 0}, {1, 0}, {0, 1}, {0, -1}}; // the viewer's right
                        static constexpr float kYaw[4] = {3.14159265f, 0.0f, 1.5707963f, 4.712389f}; // (radians: flat items face out)
                        const glm::dvec3 centre(ch->pos().x * 16 + sh.x + 0.5, sh.y + 0.3, ch->pos().z * 16 + sh.z + 0.5);
                        for (int j = 0; j < 3; ++j) {
                            const mc::world::ItemStack& it = sh.data.items[size_t(j)];
                            if (it.empty()) continue;
                            const double side = (j - 1) * 0.3125;
                            const glm::dvec3 pos = centre + glm::dvec3(kOut[f][0] * 0.56 + kRight[f][0] * side, 0.0,
                                                                       kOut[f][1] * 0.56 + kRight[f][1] * side);
                            entities.addItem(it, pos, kYaw[f], 0.0f, lightTable[15 * 16 + 15], camera.position);
                        }
                    }
                    for (const auto& bn : ch->banners()) { // (M28.3d) the cloth and its layers
                        const auto st = ch->get(bn.x, bn.y, bn.z);
                        const auto& breg = mc::world::blockRegistry();
                        const mc::world::BlockKind k = breg.kind(breg.blockOf(st));
                        const bool wallBanner = k == mc::world::BlockKind::WallBanner;
                        if (!wallBanner && k != mc::world::BlockKind::Banner) continue;
                        int bs = 15, bb = 0;
                        if (ch->lit()) {
                            bs = ch->skyLight(bn.x, bn.y, bn.z);
                            bb = ch->blockLight(bn.x, bn.y, bn.z);
                        }
                        entities.addBanner(
                            glm::dvec3(ch->pos().x * 16 + bn.x, bn.y, ch->pos().z * 16 + bn.z),
                            wallBanner,
                            wallBanner ? breg.get(st, mc::world::properties::facing) + 2
                                       : breg.get(st, mc::world::properties::rotation16),
                            mc::world::bannerColour(breg.block(breg.blockOf(st)).id), bn.data,
                            lightTable[size_t(bs * 16 + bb)], camera.position);
                    }
                    for (const auto& sg : ch->signs()) {
                        bool written = false; // (blank signs: nothing to draw)
                        for (const auto& line : sg.data.front.lines)
                            written = written || line[0] != '\0';
                        if (!written) continue;
                        const auto st = ch->get(sg.x, sg.y, sg.z);
                        const mc::world::BlockKind k = reg.kind(reg.blockOf(st));
                        // The board's facing as quarter turns (south 0, west 1, north 2, east 3)
                        // and the centre of its face, as the models build them.
                        int quarter = 0;
                        if (k == mc::world::BlockKind::WallSign ||
                            k == mc::world::BlockKind::WallHangingSign) {
                            const int f = reg.get(st, mc::world::properties::facing);
                            quarter = f == 0 ? 2 : f == 1 ? 0 : f == 2 ? 1 : 3;
                        } else if (k == mc::world::BlockKind::Sign ||
                                   k == mc::world::BlockKind::HangingSign) {
                            quarter =
                                ((reg.get(st, mc::world::properties::rotation16) + 2) / 4) & 3;
                        } else {
                            continue;
                        }
                        static constexpr glm::vec3 kOut[4] = {
                            {0, 0, 1}, {-1, 0, 0}, {0, 0, -1}, {1, 0, 0}};
                        const glm::vec3 out = kOut[quarter];
                        const glm::vec3 right(out.z, 0.0f, -out.x); // (as seen looking at the face)
                        glm::dvec3 centre(ch->pos().x * 16 + sg.x + 0.5, sg.y,
                                          ch->pos().z * 16 + sg.z + 0.5);
                        double faceOffset = 1.0 / 16.0 + 0.002; // half the board's thickness
                        switch (k) {
                        case mc::world::BlockKind::Sign:
                            centre.y += 12.0 / 16.0;
                            break;
                        case mc::world::BlockKind::WallSign:
                            centre.y += 8.0 / 16.0;
                            faceOffset = 2.0 / 16.0 - 0.5 + 0.002; // board against the wall behind
                            break;
                        default:
                            centre.y += 5.0 / 16.0;
                            break; // hanging boards
                        }
                        centre += glm::dvec3(out) * faceOffset;
                        const float px = 1.0f / 96.0f;
                        for (int i = 0; i < mc::world::SignData::kLines; ++i) {
                            const glm::dvec3 lineCentre =
                                centre + glm::dvec3(0.0, (1.5 - i) * 10.0 * px, 0.0);
                            entities.addText(sg.data.front.lines[size_t(i)].data(), lineCentre,
                                             right, glm::vec3(0, 1, 0), px,
                                             kTextColours[sg.data.front.colour & 15],
                                             camera.position);
                        }
                    }
                }
        }
        for (const auto& c :
             projectiles.clouds()) { // (M20.2) dragon's breath; (M28.4b) lingering potions
            const uint32_t pc =
                c.potion ? mc::world::potionColour(static_cast<mc::world::Potion>(c.potion))
                         : 0xBF4DF2u;
            entities.addCloud(
                c.pos, c.radius, float(gameTime) + float(clock.alpha), camera.position,
                glm::vec3(float(pc >> 16 & 255), float(pc >> 8 & 255), float(pc & 255)) / 255.0f);
        }
        for (const auto& o : orbs.orbs())
            entities.addOrb(glm::mix(o.prevPos, o.pos, clock.alpha), o.value,
                            float(o.age) + float(clock.alpha), camera.position);
        for (const auto& mv : blockUpdates.moving()) { // blocks in flight (M21.5): 2 ticks a move
            const double progress =
                std::min(1.0, (double(blockUpdates.now() - mv.start) + clock.alpha) / 2.0);
            const glm::dvec3 to(mv.to.x + 0.5, mv.to.y, mv.to.z + 0.5);
            entities.addBlock(mv.state,
                              to - glm::dvec3(mc::world::normal(mv.dir)) * (1.0 - progress),
                              glm::vec3(1.0f), camera.position);
        }
        for (const auto& t : primedTnt.items()) { // flashing white every 5 ticks (wiki)
            static const mc::world::BlockStateId tntState =
                mc::world::blockRegistry().defaultState(mc::world::blocks::Tnt);
            const glm::vec3 light = (t.fuse / 5) % 2 == 0 ? glm::vec3(2.0f) : glm::vec3(1.0f);
            entities.addBlock(tntState, glm::mix(t.prevPos, t.pos, clock.alpha), light,
                              camera.position);
        }
        for (const auto& f : fallingBlocks.blocks())
            entities.addBlock(f.state, glm::mix(f.prevPos, f.pos, clock.alpha),
                              lightTable[size_t(f.skyLight * 16 + f.blockLight)], camera.position);
        // Mobs: only chunks in view, and mobs within vanilla's entity render distance
        // (64 blocks x the hitbox's average edge; wiki: Options › Entity Distance).
        const mc::gfx::Frustum mobFrustum = mc::gfx::Frustum::fromMatrix(
            camera.viewProjectionAtOrigin(float(fbWidth) / float(fbHeight)));
        world.forEachTickingChunk([&](mc::world::Chunk& c) {
            // Beacon beams up to the sky from lit beacons (M23.6; wiki: Beacon).
            for (const auto& bc : c.beacons())
                if (!bc.data.conduit && bc.data.beam && bc.data.levels > 0) {
                    const glm::dvec3 base(c.pos().x * 16 + bc.x + 0.5, bc.y + 0.75,
                                          c.pos().z * 16 + bc.z + 0.5);
                    entities.addBeam(base,
                                     glm::dvec3(base.x, double(c.height().maxY() + 1), base.z),
                                     camera.position, {0.85f, 0.95f, 1.0f}, 0.2f);
                }
            if (c.mobs().empty()) return;
            const glm::vec3 cmin(glm::dvec3(c.pos().x * 16.0, c.height().minY, c.pos().z * 16.0) -
                                 camera.position);
            if (!mobFrustum.intersectsBox(cmin,
                                          cmin + glm::vec3(16.0f, float(c.height().height), 16.0f)))
                return;
            for (const auto& m : c.mobs()) {
                const glm::dvec3 p = glm::mix(m.prevPos, m.pos, clock.alpha);
                const auto& info = mc::world::mobInfo(m.type);
                const double maxDist = mc::world::isTechnical(m.type) ? 64.0 // (M29.7e: displays, no hitbox)
                                                                      : 64.0 * (info.width * 2.0 + info.height) / 3.0;
                const glm::dvec3 rel = p - camera.position;
                if (glm::dot(rel, rel) > maxDist * maxDist) continue;
                if (m.leash != 0 &&
                    m.health > 0.0f) { // (M28.3c) the lead, from its neck to the holder
                    const glm::dvec3 neck = p + glm::dvec3(0.0, info.height * 0.75, 0.0);
                    const glm::dvec3 to =
                        m.leash == 1
                            ? player.renderPosition(clock.alpha) + glm::dvec3(0.0, 1.1, 0.0)
                            : glm::dvec3(m.leashPos) + glm::dvec3(0.5, 0.5, 0.5);
                    entities.addBeam(neck, to, camera.position, {0.42f, 0.30f, 0.16f}, 0.03f);
                }
                if (m.type == mc::world::MobType::LeashKnot) {
                    entities.addKnot(p, camera.position);
                    continue;
                }
                if (mc::world::isTechnical(m.type)) { // (M29.7e) only displays show anything
                    const glm::vec3 full = lightTable[15 * 16 + 15];
                    if (m.type == mc::world::MobType::BlockDisplay && m.commandId != 0)
                        entities.addBlock(mc::world::BlockStateId(m.commandId), p, full, camera.position);
                    else if (m.type == mc::world::MobType::ItemDisplay && m.commandId != 0)
                        entities.addItem({mc::world::ItemId(m.commandId), 1}, p, 0.0f, 0.0f, full, camera.position);
                    else if (m.type == mc::world::MobType::TextDisplay && m.commandId != 0) {
                        const float yawRad = glm::radians(camera.yaw); // (vanilla's default: it faces the camera)
                        entities.addText(mc::world::nameText(m.commandId), p, glm::vec3(-std::cos(yawRad), 0.0f, -std::sin(yawRad)),
                                         glm::vec3(0.0f, 1.0f, 0.0f), 0.025f, 0xFFFFFF, camera.position);
                    }
                    continue;
                }
                if (mc::world::isHanging(
                        m.type)) { // (M28.3a) item frames and paintings, from the atlas
                    const mc::Aabb hb = mc::Mobs::hangingBox(m);
                    if (!mobFrustum.intersectsBox(glm::vec3(hb.min - camera.position),
                                                  glm::vec3(hb.max - camera.position)))
                        continue;
                    const mc::world::BlockPos hp{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)),
                                                 int(std::floor(m.pos.z))};
                    int hs = 15, hbl = 0;
                    if (const auto* lc = world.chunk(hp.chunk()); lc && lc->lit()) {
                        hs = lc->skyLight(mc::world::blockToLocal(hp.x), hp.y,
                                          mc::world::blockToLocal(hp.z));
                        hbl = lc->blockLight(mc::world::blockToLocal(hp.x), hp.y,
                                             mc::world::blockToLocal(hp.z));
                    }
                    const glm::vec3 hl = lightTable[size_t(hs * 16 + hbl)];
                    if (m.type == mc::world::MobType::Painting)
                        entities.addPainting(m.woolColour, m.pos, m.phase, hl, camera.position);
                    else
                        entities.addItemFrame(
                            m.pos, m.phase, m.type == mc::world::MobType::GlowItemFrame,
                            mc::Mobs::frameItem(world, m), m.node, hl, camera.position);
                    continue;
                }
                const glm::vec3 bmin(rel - glm::dvec3(info.width * 0.5, 0.0, info.width * 0.5));
                const float reachOut =
                    m.type == mc::world::MobType::EnderDragon ? 8.0f : 0.0f; // (its tail and wings)
                if (!mobFrustum.intersectsBox(
                        bmin - glm::vec3(reachOut),
                        bmin + glm::vec3(float(info.width), float(info.height), float(info.width)) +
                            glm::vec3(reachOut)))
                    continue;
                const mc::world::BlockPos b{int(std::floor(p.x)), int(std::floor(p.y + 0.5)),
                                            int(std::floor(p.z))};
                int sky = 15, blk = 0;
                if (const auto* lc = world.chunk(b.chunk()); lc && lc->lit()) {
                    sky = lc->skyLight(mc::world::blockToLocal(b.x), b.y,
                                       mc::world::blockToLocal(b.z));
                    blk = lc->blockLight(mc::world::blockToLocal(b.x), b.y,
                                         mc::world::blockToLocal(b.z));
                }
                const float a = float(clock.alpha);
                entities.addMob(m, p, m.prevYaw + (m.yaw - m.prevYaw) * a,
                                m.prevHeadYaw + (m.headYaw - m.prevHeadYaw) * a,
                                m.prevPitch + (m.pitch - m.prevPitch) * a,
                                lightTable[size_t(sky * 16 + blk)], camera.position);
                // (M29.3e) the block a minecart kind carries, three quarters size, in the cart
                if (m.type == mc::world::MobType::Minecart && m.decor != 0) {
                    static const mc::world::BlockStateId cartBlocks[7] = {
                        0,
                        mc::world::blockRegistry().defaultState(mc::world::blocks::Chest),
                        mc::world::blockRegistry().defaultState(mc::world::blocks::Furnace),
                        mc::world::blockRegistry().defaultState(mc::world::blocks::Hopper),
                        mc::world::blockRegistry().defaultState(mc::world::blocks::Tnt),
                        mc::world::blockRegistry().findBlock("minecraft:command_block")
                            ? mc::world::blockRegistry().defaultState(*mc::world::blockRegistry().findBlock("minecraft:command_block"))
                            : mc::world::blockRegistry().defaultState(mc::world::blocks::Furnace),
                        mc::world::blockRegistry().defaultState(mc::world::blocks::Spawner)};
                    entities.addBlock(cartBlocks[m.decor % 7], p + glm::dvec3(0.0, 0.2, 0.0),
                                      lightTable[size_t(sky * 16 + blk)], camera.position, 0.75f);
                }
                // (M29.3b) a named mob's name floats over it, facing the camera, within 64.
                if (m.nameId != 0 && glm::length(p - camera.position) < 64.0) {
                    const float yawRad = glm::radians(camera.yaw);
                    const glm::vec3 right(-std::cos(yawRad), 0.0f, -std::sin(yawRad));
                    entities.addText(mc::world::nameText(m.nameId),
                                     p + glm::dvec3(0.0, mc::world::mobInfo(m.type).height + 0.5, 0.0), right,
                                     glm::vec3(0.0f, 1.0f, 0.0f), 0.025f, 0xFFFFFF, camera.position);
                }
                if (m.type == mc::world::MobType::EnderDragon && m.hasBeam && m.deathTime == 0)
                    entities.addBeam(m.beam, p + glm::dvec3(0.0, 0.75, 0.0), camera.position);
                if ((m.type == mc::world::MobType::Guardian ||
                     m.type == mc::world::MobType::ElderGuardian) &&
                    m.hasBeam &&
                    m.deathTime == 0) { // the laser, purple turning yellow as it charges (M25.5)
                    const float f = std::min(
                        1.0f, float(m.chargeTicks) /
                                  (m.type == mc::world::MobType::Guardian ? 80.0f : 60.0f));
                    entities.addBeam(
                        p + glm::dvec3(0.0, mc::world::mobInfo(m.type).height * 0.5, 0.0), m.beam,
                        camera.position, {0.5f + 0.5f * f, 0.3f + 0.6f * f, 0.9f - 0.7f * f},
                        0.04f);
                }
            }
        });
        if (survival && interaction.breakingBlock())
            entities.setCrack(*interaction.breakingBlock(),
                              static_cast<int>(interaction.breakProgress() * 10.0f));
        else
            entities.clearCrack();
        // The player (M30.1): its model in third person (a mannequin wearing what it wears),
        // else the first-person hand, drawn over the world after the outline.
        const glm::dvec3 playerFeet = player.renderPosition(clock.alpha);
        int playerSky = 15, playerBlk = 0;
        {
            const mc::world::BlockPos b{int(std::floor(playerFeet.x)), int(std::floor(playerFeet.y + 0.5)), int(std::floor(playerFeet.z))};
            if (const auto* lc = world.chunk(b.chunk()); lc && lc->lit() && world.isInHeight(b.y)) {
                playerSky = lc->skyLight(mc::world::blockToLocal(b.x), b.y, mc::world::blockToLocal(b.z));
                playerBlk = lc->blockLight(mc::world::blockToLocal(b.x), b.y, mc::world::blockToLocal(b.z));
            }
        }
        const glm::vec3 playerLight = lightTable[size_t(playerSky * 16 + playerBlk)];
        if (perspective != 0 && gameMode != 3 && !dead) {
            static mc::world::MobData body; // (reused: no per-frame construction of the large struct)
            body.type = mc::world::MobType::Mannequin;
            body.limbSwing = playerAnim.limbSwing(animAlpha);
            body.limbSwingAmount = playerAnim.limbAmount(animAlpha);
            body.heldItem = inventory.selectedStack().item;
            body.crouching = player.sneaking() && !player.flying();
            body.swingProgress = playerAnim.swing(animAlpha);
            body.vehicle = ridingCart != 0 ? 1 : 0;
            body.sleeping = sleepTicks > 0;
            for (int i = 0; i < 4; ++i)
                body.worn[size_t(i)] = inventory.armor(i).empty()
                                           ? 0
                                           : mc::world::armorMaterial(mc::world::itemRegistry().item(inventory.armor(i).item).id);
            entities.addMob(body, playerFeet, playerAnim.bodyYaw(animAlpha), player.yaw(), player.pitch(), playerLight,
                            camera.position);
        }
        entities.draw(camera, float(fbWidth) / float(fbHeight));
        lastHit = hit;
        {
            // The outline hugs the block's shape bounds (slabs, stairs, doors...).
            glm::vec3 lo(0.0f), hi(1.0f);
            if (hit) {
                const auto st = world.getBlock(hit->block);
                const mc::world::BlockShape& sh = mc::world::collisionShape(st);
                if (mc::world::blockRegistry().collides(st) && sh.count > 0) {
                    lo = glm::vec3(1.0f);
                    hi = glm::vec3(0.0f);
                    for (int i = 0; i < sh.count; ++i)
                        for (int a = 0; a < 3; ++a) {
                            lo[a] = std::min(lo[a], sh.boxes[size_t(i)].from[a] / 16.0f);
                            hi[a] = std::max(hi[a],
                                             std::min<int>(sh.boxes[size_t(i)].to[a], 16) / 16.0f);
                        }
                }
            }
            overlay.draw(camera, fbWidth, fbHeight, // (no outline for spectators)
                         hit && gameMode != 3 ? std::optional<mc::world::BlockPos>(hit->block)
                                              : std::nullopt,
                         lo, hi);
        }
        // The first-person hand (M30.1), then the crosshair over it (first person only).
        if (perspective == 0 && gameMode != 3 && !dead && sleepTicks == 0) {
            const glm::vec3 fwd = mc::world::lookVector(camera.yaw, camera.pitch);
            const glm::vec3 rightAxis = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
            entities.addHand(inventory.selectedStack(), rightAxis, glm::cross(rightAxis, fwd), fwd,
                             playerAnim.swing(animAlpha), playerAnim.equip(animAlpha), handBob, playerLight);
            entities.drawHand(camera, float(fbWidth) / float(fbHeight));
        }
        if (perspective == 0) overlay.drawCrosshair(fbWidth, fbHeight);

        // HUD: inventory, chat, F3 - one batched GUI draw.
        {
            const int scale = mc::gfx::GuiRenderer::guiScale(fbWidth, fbHeight);
            const int guiW = fbWidth / scale, guiH = fbHeight / scale;
            auto& batch = gui.batch();
            { // (M28.2a) compasses and clocks turn with the player and the sky
                mc::gfx::ItemIcons::Dials dials;
                dials.player = player.renderPosition(clock.alpha);
                dials.yaw = player.yaw();
                dials.dimension = uint8_t(dimension);
                dials.spawn = {worldSpawn[0], worldSpawn[1], worldSpawn[2]};
                dials.hasDeath = lastDeath.has_value();
                if (lastDeath) {
                    dials.death = {lastDeath->first.x, lastDeath->first.y, lastDeath->first.z};
                    dials.deathDimension = uint8_t(lastDeath->second);
                }
                dials.celestial = mc::world::celestialAngle(dayTime, clock.alpha);
                dials.seconds = mc::timeSeconds();
                itemIcons.setDials(dials);
            }
            mc::ui::drawFrostOverlay(batch, vitals.frozen(), guiW, guiH); // (M29.4c: powder snow)
            if (gameMode != 3)
                mc::ui::drawHotbar(batch, inventory, itemIcons, renderer.models(), guiW, guiH);
            { // The held map (M28.2b): ours is a panel at the bottom right (vanilla holds it
              // in first person), the player a white marker.
                static const mc::world::ItemId filledMap =
                    *mc::world::itemRegistry().find("filled_map");
                const mc::world::ItemStack& held = inventory.selectedStack().item == filledMap
                                                       ? inventory.selectedStack()
                                                       : inventory.offhand();
                const mc::world::MapData* md = held.item == filledMap && !dead && gameMode != 3
                                                   ? maps.get(held.damage)
                                                   : nullptr;
                if (md) {
                    if (int(held.damage) != shownMap || md->version != shownMapVersion) {
                        for (size_t i = 0; i < md->colors.size(); ++i) {
                            const uint32_t c = mc::world::mapColorRgb(md->colors[i]);
                            mapRgba[i * 4 + 0] = uint8_t(c >> 16);
                            mapRgba[i * 4 + 1] = uint8_t(c >> 8);
                            mapRgba[i * 4 + 2] = uint8_t(c);
                            mapRgba[i * 4 + 3] =
                                md->colors[i] >> 2 ? 255 : 0; // (unexplored: the paper shows)
                        }
                        gui.uploadMap(mapRgba.data());
                        shownMap = held.damage;
                        shownMapVersion = md->version;
                    }
                    const float size = std::min(128.0f, float(guiH) - 60.0f);
                    const float mx = float(guiW) - size - 10.0f, my = float(guiH) - size - 34.0f;
                    batch.fill(mx - 5.0f, my - 5.0f, size + 10.0f, size + 10.0f,
                               mc::gfx::rgba(120, 96, 60));
                    batch.fill(mx - 4.0f, my - 4.0f, size + 8.0f, size + 8.0f,
                               mc::gfx::rgba(222, 206, 162));
                    batch.sprite(mc::gfx::GuiTexture::Map, mx, my, size, size, 0.0f, 0.0f, 128.0f,
                                 128.0f);
                    if (md->dimension == uint8_t(dimension)) {
                        const glm::dvec2 p =
                            mc::world::Maps::pixelOf(*md, player.position().x, player.position().z);
                        const float k = size / 128.0f;
                        const float px = mx + std::clamp(float(p.x), 0.0f, 128.0f) * k,
                                    py = my + std::clamp(float(p.y), 0.0f, 128.0f) * k;
                        batch.fill(px - 2.0f, py - 2.0f, 4.0f, 4.0f, mc::gfx::rgba(40, 40, 40));
                        batch.fill(px - 1.0f, py - 1.0f, 2.0f, 2.0f, mc::gfx::rgba(255, 255, 255));
                    }
                }
            }
            if (survival)
                mc::ui::drawVitals(batch, vitals.health(), vitals.food(), guiW, guiH, vitals.air(),
                                   inventory.armorPoints(), vitals.maxHealth(), vitals.absorption());
            const mc::world::MobData* steed =
                ridingCart ? findCart() : nullptr; // (M26.2: the jump bar)
            if (steed && steed->saddled &&
                (mc::world::isHorseKind(steed->type) || mc::world::isCamel(steed->type) ||
                 mc::world::isNautilus(steed->type)))
                mc::ui::drawJumpBar(batch, float(mountJumpTicks) / 10.0f, guiW, guiH);
            else if (survival)
                mc::ui::drawExperience(batch, vitals.xpLevel(), vitals.xpProgress(), guiW, guiH);
            if (mobs.bossHealth() >= 0.0f) // (M20.2)
                mc::ui::drawBossBar(
                    batch,
                    mobs.bossType() == mc::world::MobType::Wither ? "Wither" : "Ender Dragon",
                    mobs.bossHealth() / mc::world::mobInfo(mobs.bossType()).maxHealth,
                    mobs.bossType() == mc::world::MobType::Wither ? mc::gfx::rgba(150, 60, 220)
                                                                  : mc::gfx::rgba(236, 72, 200),
                    guiW);
            if (raid.active() && raid.loaded() &&
                dimension == Dimension::Overworld && // (M24.5: vanilla's red raid bar)
                glm::length(glm::dvec3(raid.centre()) - player.position()) < 96.0)
                mc::ui::drawBossBar(batch, "Raid", raid.progress(), mc::gfx::rgba(220, 40, 40),
                                    guiW);
            if (toastAdv >= 0) { // the advancement toast (vanilla: 160x32, top right, 5 s)
                const auto& a = mc::world::advancements()[size_t(toastAdv)];
                const float tx = float(guiW) - 164.0f, ty = 4.0f;
                batch.fill(tx, ty, 160.0f, 32.0f, mc::gfx::rgba(33, 33, 33, 230));
                batch.fill(tx, ty, 160.0f, 1.0f, mc::gfx::rgba(85, 85, 85));
                batch.fill(tx, ty + 31.0f, 160.0f, 1.0f, mc::gfx::rgba(85, 85, 85));
                const bool challenge = a.frame == mc::world::AdvFrame::Challenge;
                batch.text(challenge                              ? "Challenge Complete!"
                           : a.frame == mc::world::AdvFrame::Goal ? "Goal Reached!"
                                                                  : "Advancement Made!",
                           tx + 8.0f, ty + 7.0f,
                           challenge ? mc::gfx::argb(0xFFFF55FF) : mc::gfx::argb(0xFFFFFF55));
                char title[64];
                std::snprintf(title, sizeof title, "%.*s", int(a.title.size()), a.title.data());
                batch.text(title, tx + 8.0f, ty + 18.0f, mc::gfx::argb(0xFFFFFFFF));
            }
            if (dead) mc::ui::drawDeathScreen(batch, guiW, guiH);
            if (sleepTicks > 0) // falling asleep: the screen darkens (vanilla)
                batch.fill(
                    0, 0, float(guiW), float(guiH),
                    mc::gfx::rgba(0, 0, 0, uint8_t(std::min(1.0f, sleepTicks / 100.0f) * 230.0f)));
            chat.draw(batch, guiW, guiH, gameTime);
            ++fpsFrames;
            if (now - fpsStart >= 1.0) {
                fps = fpsFrames;
                fpsFrames = 0;
                fpsStart = now;
            }
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Furnace) {
                mc::world::Chunk* fc = world.chunk(containerBlock.chunk());
                container.setFurnace(fc ? fc->furnace(mc::world::blockToLocal(containerBlock.x),
                                                      containerBlock.y,
                                                      mc::world::blockToLocal(containerBlock.z))
                                        : nullptr);
            }
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Trading)
                pointTrader();
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Mount)
                pointMount();
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Beacon) {
                mc::world::Chunk* bc = world.chunk(containerBlock.chunk());
                container.setBeacon(bc ? bc->beacon(mc::world::blockToLocal(containerBlock.x),
                                                    containerBlock.y,
                                                    mc::world::blockToLocal(containerBlock.z))
                                       : nullptr);
            }
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Brewing) {
                mc::world::Chunk* bc = world.chunk(containerBlock.chunk());
                container.setBrewing(bc ? bc->brewing(mc::world::blockToLocal(containerBlock.x),
                                                      containerBlock.y,
                                                      mc::world::blockToLocal(containerBlock.z))
                                        : nullptr);
            }
            // Chests may have moved in memory or gone during this frame's ticks: re-point.
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Chest)
                pointChests();
            if (container.isOpen() &&
                (container.type() == mc::ui::ContainerScreen::Type::Hopper ||
                 container.type() == mc::ui::ContainerScreen::Type::Dispenser))
                pointStore();
            if (container.isOpen())
                container.draw(
                    batch, itemIcons, renderer.models(), inventory, guiW, guiH,
                    [&] {
                        double x = 0, y = 0;
                        window.cursorPos(x, y);
                        return x / scale;
                    }(),
                    [&] {
                        double x = 0, y = 0;
                        window.cursorPos(x, y);
                        return y / scale;
                    }());
            if (creative.isOpen()) {
                double mx = 0, my = 0;
                window.cursorPos(mx, my);
                if (screenshotMode) { // a fixed hover for screenshots: the 3rd grid item
                    mx = (guiW - mc::ui::CreativeInventory::kWidth) / 2 + 9 + 2 * 18 + 8;
                    my = (guiH - mc::ui::CreativeInventory::kHeight) / 2 + 18 + 8;
                } else {
                    mx /= scale;
                    my /= scale;
                }
                creative.draw(batch, itemIcons, renderer.models(), inventory, guiW, guiH, mx, my);
            }
            if (showDebug) {
                mc::ui::DebugInfo d;
                d.version = mc::buildString();
                d.fps = fps;
                d.feet = player.position();
                d.yaw = player.yaw();
                d.pitch = player.pitch();
                if (hit) {
                    d.hasTarget = true;
                    d.target = {hit->block.x, hit->block.y, hit->block.z};
                    // Name re-built only when the targeted state changes (not per frame).
                    // State names are built once per state, then reused (no per-frame
                    // allocation).
                    const auto state = world.getBlock(hit->block);
                    auto& name = stateNames[state];
                    if (name.empty()) name = mc::world::blockRegistry().toString(state);
                    d.targetName = name.c_str();
                }
                const mc::world::BlockPos feet{static_cast<int32_t>(std::floor(d.feet.x)),
                                               static_cast<int32_t>(std::floor(d.feet.y)),
                                               static_cast<int32_t>(std::floor(d.feet.z))};
                if (const auto* c = world.chunk(feet.chunk()))
                    d.biome = mc::world::biomeInfo(
                                  c->biomes()->at(mc::world::blockToLocal(feet.x), feet.y,
                                                  mc::world::blockToLocal(feet.z), c->height()))
                                  .id.data();
                if (const auto* c = world.chunk(feet.chunk()); c && c->lit()) {
                    d.skyLight = c->skyLight(mc::world::blockToLocal(feet.x), feet.y,
                                             mc::world::blockToLocal(feet.z));
                    d.blockLight = c->blockLight(mc::world::blockToLocal(feet.x), feet.y,
                                                 mc::world::blockToLocal(feet.z));
                }
                d.dayTime = dayTime;
                d.gameTime = gameTime;
                d.renderDistance = loader ? opts->renderDistance : 8;
                d.sectionsDrawn = renderer.stats().sectionsDrawn;
                d.sectionsTotal = renderer.stats().sections;
                d.pendingChunks = loader ? loader->pending() : 0;
                d.pendingLight = lighting.pending();
                d.pendingMeshes = renderer.pendingMeshes();
                d.hostileMobs = mobs.hostileCount();
                d.width = fbWidth;
                d.height = fbHeight;
                d.gpuMs = renderer.averageGpuMs();
                debugScreen.draw(batch, d, guiW);
            }
            if (signEditor.isOpen()) { // (M23.3c)
                double mx = 0, my = 0;
                window.cursorPos(mx, my);
                if (signEditor.draw(batch, guiW, guiH, mx / scale, my / scale, signClick,
                                    int64_t(mc::timeSeconds() * 1000.0)))
                    signDone = true;
                signClick = false;
            }
            if (bookScreen.isOpen()) { // (M28.2c)
                double mx = 0, my = 0;
                window.cursorPos(mx, my);
                batch.fill(0, 0, float(guiW), float(guiH), mc::gfx::rgba(16, 16, 16, 160));
                const auto a = bookScreen.draw(batch, guiW, guiH, mx / scale, my / scale, bookClick,
                                               int64_t(mc::timeSeconds() * 1000.0));
                if (a != mc::ui::BookScreen::Action::None) bookAction = a;
                bookClick = false;
            }
            gui.draw(fbWidth, fbHeight);
        }

        if (paused) { // the Game Menu over the world
            const auto action = drawMenuFrame(shared, fbWidth, fbHeight, false);
            if (action == mc::ui::MenuAction::Resume) {
                shared.menuState.screen = mc::ui::MenuScreen::None;
                window.setCursorCaptured(true);
                attackArmed = false;      // (the click on the button mustn't break a block)
                last = mc::timeSeconds(); // (no catching up on the paused time)
            } else if (action == mc::ui::MenuAction::CommandBlockDone) { // (M29.7) applied in the next tick
                const auto& ms = shared.menuState;
                pendingCommandEdit = PendingCommandEdit{commandEditing, commandEditingCart, ms.command, ms.commandMode,
                                                        ms.commandConditional, ms.commandAlways};
                commandEditingCart = 0;
                shared.menuState.screen = mc::ui::MenuScreen::None;
                window.setCursorCaptured(true);
                attackArmed = false;
                last = mc::timeSeconds();
            } else if (action == mc::ui::MenuAction::SaveAndQuit) {
                sessionEnd = SessionEnd::ToTitle;
                break;
            } else if (action == mc::ui::MenuAction::OptionsChanged ||
                       action == mc::ui::MenuAction::OptionsClosed) {
                applySessionOptions();
            }
        }
        if (!meshed && renderer.pendingMeshes() == 0 && lighting.pending() == 0 &&
            (!loader || loader->pending() == 0) && renderer.stats().sections > 0) {
            meshed = true;
            renderer.resetGpuStats(); // steady-state GPU numbers, like the CPU stats
            if (opts->demoEdit) runDemoEdit(world, player, inventory, frameEdits);
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
    // Quitting with a screen open returns its grid and carried stack first (vanilla).
    screenDrops.clear();
    if (container.isOpen()) container.close(inventory, screenDrops);
    for (const auto& d :
         screenDrops) // didn't fit: drop at the player (saved later... lost: see deviations)
        droppedItems.spawn(player.position(), d, gameRng);
    blockUpdates.landAll();
    stats.add(mc::world::Stat::LeaveGame);
    saveWorld(true);
    shared.menuState.stats = nullptr; // (the session's counters go away with it)
    shared.menuState.adv = nullptr;
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

// The menus between worlds (M22.5): draws the current screen each frame until the
// player picks a world, creates one or quits.
mc::ui::MenuAction runMenus(Shared& shared) {
    mc::Window& window = shared.window;
    mc::ui::MenuState& st = shared.menuState;
    window.setCursorCaptured(false);
    double last = mc::timeSeconds();
    while (!window.shouldClose()) {
        window.pollEvents();
        int fbWidth = 0, fbHeight = 0;
        window.framebufferSize(fbWidth, fbHeight);
        if (fbWidth == 0 || fbHeight == 0) { // minimised
            window.waitEvents(0.1);
            continue;
        }
        const auto action = drawMenuFrame(shared, fbWidth, fbHeight, true);
        window.swapBuffers();
        const double now = mc::timeSeconds();
        if (now - last < 1.0 / 120.0)
            std::this_thread::sleep_for(std::chrono::milliseconds(2)); // (menus: no need to spin)
        last = now;
        if (action == mc::ui::MenuAction::PlayWorld && st.selected >= 0 &&
            st.selected < int(st.worlds.size()))
            return action;
        if (action == mc::ui::MenuAction::CreateWorld || action == mc::ui::MenuAction::Quit)
            return action;
        if (action == mc::ui::MenuAction::DeleteWorld && st.selected >= 0 &&
            st.selected < int(st.worlds.size())) {
            // (vanilla deletes the whole folder; the confirmation screen came first)
            std::error_code ec;
            const auto dir =
                std::filesystem::path(MC_SAVES_DIR) / st.worlds[size_t(st.selected)].folder;
            {
                mc::FileLock lock; // (open in another instance: leave it alone, as vanilla)
                if (!lock.acquire(dir / "session.lock")) {
                    MC_LOG_WARN("World \"%s\" is open elsewhere; not deleted",
                                st.worlds[size_t(st.selected)].name.c_str());
                    st.screen = mc::ui::MenuScreen::WorldList;
                    continue;
                }
            }
            std::filesystem::remove_all(dir, ec);
            st.worlds = mc::world::listWorlds(MC_SAVES_DIR);
            st.selected = st.worlds.empty() ? -1 : 0;
            st.screen = mc::ui::MenuScreen::WorldList;
        }
    }
    return mc::ui::MenuAction::Quit;
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
    if (opts->printVersion) {
        std::printf("MinecraftClone %s (%s)\n", mc::version(), mc::buildString());
        return 0;
    }
    MC_LOG_INFO("MinecraftClone %s (%s)", mc::version(), mc::buildString());
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
    mc::gfx::GuiRenderer gui;
    if (!gui.init(renderer.packs(), renderer.atlas())) return 1;

    // Sound (M22.4, ADR 0008), options (M22.5) and the menus: shared by every session.
    // Sound (M22.4, ADR 0008): every sound event's files, loaded through the pack stack
    // (a pack may replace any WAV). Screenshot and hidden runs stay silent.
    mc::audio::SoundEngine audio;
    std::vector<std::vector<int>> soundHandles(static_cast<size_t>(mc::world::kSoundCount));
    if (!opts->mute && (opts->sound || (!opts->hidden && opts->screenshotPath.empty())) &&
        audio.init()) {
        int loaded = 0;
        for (int s = 0; s < mc::world::kSoundCount; ++s)
            for (const std::string& f :
                 mc::world::soundInfo(static_cast<mc::world::Sound>(s)).files) {
                const auto bytes = renderer.packs().read("assets/minecraft/sounds/" + f + ".wav");
                const int h = bytes ? audio.load(*bytes) : -1;
                if (h >= 0) {
                    soundHandles[size_t(s)].push_back(h);
                    ++loaded;
                } else {
                    MC_LOG_WARN("Sound: missing or unusable %s.wav", f.c_str());
                }
            }
        MC_LOG_INFO("Sound: %d files loaded", loaded);
    }

    mc::LaunchOptions launch = *opts; // (sessions change world, seed, mode...)
    mc::GameOptions options;
    const std::filesystem::path optionsFile =
        std::filesystem::path(MC_SAVES_DIR).parent_path() / "options.txt";
    const bool interactive = !screenshotMode && !opts->hidden;
    if (interactive)
        options.load(optionsFile); // (scripted runs: defaults and the command line only)
    if (interactive && !opts->renderDistanceSet) launch.renderDistance = options.renderDistance;
    audio.setMasterVolume(options.masterVolume);
    mc::ui::Menu menu;
    mc::ui::MenuState menuState;
    menuState.splash = mc::ui::splashText(uint32_t(mc::timeSeconds() * 1000.0));
    Shared shared{window,
                  renderer,
                  overlay,
                  gui,
                  audio,
                  soundHandles,
                  options,
                  optionsFile,
                  menu,
                  menuState,
                  static_cast<uint16_t>(renderer.atlas().spriteIndex("dirt"))};
    shared.optionsRenderDistance = options.renderDistance;
    if (!interactive || opts->renderDistanceSet) shared.cliRenderDistance = launch.renderDistance;
    shared.cliNoVsync = !opts->vsync;
    // --menu: a screen by itself (screenshots of the menus; "pause" opens over a world).
    if (!opts->menu.empty() && opts->menu != "pause" && opts->menu != "statistics" &&
        opts->menu != "advancements" && opts->menu != "commandblock") { // (in-world screens below)
        menuState.worlds = mc::world::listWorlds(MC_SAVES_DIR);
        menuState.selected = menuState.worlds.empty() ? -1 : 0;
        menuState.screen = opts->menu == "title"    ? mc::ui::MenuScreen::Title
                           : opts->menu == "worlds" ? mc::ui::MenuScreen::WorldList
                           : opts->menu == "create" ? mc::ui::MenuScreen::CreateWorld
                                                    : mc::ui::MenuScreen::Options;
        for (int f = 0; !window.shouldClose(); ++f) {
            window.pollEvents();
            int fbWidth = 0, fbHeight = 0;
            window.framebufferSize(fbWidth, fbHeight);
            drawMenuFrame(shared, fbWidth, fbHeight, true);
            if (screenshotMode && f + 1 >= opts->screenshotFrames)
                return mc::gfx::saveScreenshot(opts->screenshotPath.c_str(), fbWidth, fbHeight) ? 0
                                                                                                : 1;
            window.swapBuffers();
        }
        return 0;
    }
    // Settings of one run that must not follow the player into other worlds.
    auto clearOneShots = [&] {
        launch.commands.clear(); // (--command lines run in the first world only)
        launch.hasPos = launch.hasLook = false;
        launch.dimension.clear();
        launch.book = false;
        launch.use = 0;
        launch.autoFly = launch.demoEdit = launch.inventory = launch.hasOpenBlock = launch.trade =
            false;
    };
    // With --world, --no-save, screenshots or hidden runs: straight into the world;
    // interactive ones come back to the title on "Save and Quit to Title".
    if (!interactive || !opts->world.empty() || opts->noSave) {
        menuState.screen = opts->menu == "pause"          ? mc::ui::MenuScreen::Pause
                           : opts->menu == "statistics"   ? mc::ui::MenuScreen::Statistics
                           : opts->menu == "advancements" ? mc::ui::MenuScreen::Advancements
                           : opts->menu == "commandblock" ? mc::ui::MenuScreen::CommandBlock // (M29.7)
                                                          : mc::ui::MenuScreen::None;
        if (opts->menu == "commandblock") { // (a sample for screenshots)
            menuState.command = "/setblock ~ ~1 ~ minecraft:gold_block";
            menuState.commandOutput = "Changed the block at 4, 65, 4";
            menuState.commandMode = 2;
            menuState.commandAlways = true;
        }
        SessionEnd end;
        const int code = runSession(shared, &launch, end);
        if (!interactive || end != SessionEnd::ToTitle || window.shouldClose()) return code;
        clearOneShots();
        launch.noSave = false;
        menuState.screen = mc::ui::MenuScreen::Title;
    }
    const std::filesystem::path savesDir(MC_SAVES_DIR);
    std::error_code ec;
    std::filesystem::create_directories(savesDir, ec);
    menuState.worlds = mc::world::listWorlds(savesDir);
    while (!window.shouldClose()) {
        // The title screen and world selection (vanilla), until a world is chosen.
        const auto choice = runMenus(shared);
        if (choice == mc::ui::MenuAction::Quit || window.shouldClose()) break;
        const bool create = choice == mc::ui::MenuAction::CreateWorld;
        if (create) {
            launch.world = mc::world::folderForWorld(menuState.newName, savesDir);
            launch.worldTitle = menuState.newName.empty() ? launch.world : menuState.newName;
            launch.seed = mc::world::seedFromText(
                menuState.newSeed,
                uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()) *
                    0x9E3779B97F4A7C15ull);
            launch.flat = menuState.newFlat;
            launch.survival = menuState.newSurvival;
            launch.difficulty = menuState.newDifficulty;
        } else {
            launch.world = menuState.worlds[size_t(menuState.selected)].folder;
            launch.worldTitle.clear();
        }
        menuState.screen = mc::ui::MenuScreen::None;
        SessionEnd end;
        const int code = runSession(shared, &launch, end);
        clearOneShots();
        if (window.shouldClose()) return code;
        menuState.worlds = mc::world::listWorlds(savesDir);
        // A world that couldn't open (locked, unreadable, unknown generator: logged)
        // leaves the player in the world list rather than quitting the game.
        menuState.screen = code != 0 ? mc::ui::MenuScreen::WorldList : mc::ui::MenuScreen::Title;
        if (code != 0)
            MC_LOG_WARN("World \"%s\" could not be opened (see above)", launch.world.c_str());
        menuState.selected = menuState.worlds.empty() ? -1 : 0;
    }
    return 0;
}
