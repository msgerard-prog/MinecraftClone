#include "core/CommandLine.h"
#include "core/FrameStats.h"
#include "core/GameClock.h"
#include "core/Log.h"
#include "core/Window.h"
#include "gameplay/Beds.h"
#include "gameplay/BlockInteraction.h"
#include "gameplay/FallingBlocks.h"
#include "gameplay/Inventory.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Player.h"
#include "rendering/Camera.h"
#include "rendering/GlContext.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/Screenshot.h"
#include "rendering/WorldRenderer.h"
#include "world/Blocks.h"
#include "world/ChunkLoader.h"
#include "world/FlatGenerator.h"
#include "world/LightManager.h"
#include "world/ChunkStorage.h"
#include "world/LevelData.h"
#include "core/FileLock.h"
#include "gameplay/Commands.h"
#include "gameplay/DragonFight.h"
#include "gameplay/Enchanting.h"
#include "gameplay/ExperienceOrbs.h"
#include "gameplay/Brewing.h"
#include "gameplay/Explosion.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "gameplay/Vitals.h"
#include "rendering/EntityRenderer.h"
#include "rendering/Frustum.h"
#include "core/Version.h"
#include "world/DayTime.h"
#include "world/BlockUpdates.h"
#include "world/Enchantments.h"
#include "world/NetherGenerator.h"
#include "world/Potions.h"
#include "gameplay/Buckets.h"
#include "gameplay/FluidContact.h"
#include "gameplay/Portals.h"
#include "rendering/GuiRenderer.h"
#include "ui/Chat.h"
#include "ui/ContainerScreen.h"
#include "ui/CreativeInventory.h"
#include "gameplay/Furnace.h"
#include "ui/Hud.h"
#include "world/Raycast.h"
#include "world/Rotation.h"
#include "world/OverworldGenerator.h"
#include "world/TerrainGenerator.h"
#include "world/World.h"

#include <cstdio>
#include <array>
#include <cmath>
#include <filesystem>
#include <optional>
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
    const int sx = static_cast<int>(std::floor(spawn.x)), sz = static_cast<int>(std::floor(spawn.z));
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
                    if (s == 0 || !r.collides(s)) continue; // air, plants, snow layers
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
    mc::ui::Chat chat;
    mc::ui::DebugScreen debugScreen;
    mc::gfx::ItemIcons itemIcons;
    itemIcons.build(renderer.atlas());
    mc::ui::CreativeInventory creative;
    mc::ui::ContainerScreen container; // survival inventory, crafting table, furnace
    mc::world::BlockPos containerBlock{};
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
    std::array<char, 64> typed{};
    mc::world::World world;
    glm::dvec3 spawn(0.5, -60.0, -6.0); // flat world: feet on the grass

    // Saved world (saves/<name>, vanilla Anvil layout, ADR 0007). Interactive runs
    // save to "New World" by default; scripted runs (screenshots, hidden) only with
    // --world. An existing world's level.dat decides seed, generator, time and player.
    std::string worldName = opts->world;
    if (worldName.empty() && !screenshotMode && !opts->hidden) worldName = "New World";
    if (opts->noSave) worldName.clear();
    const std::filesystem::path worldDir =
        worldName.empty() ? std::filesystem::path() : std::filesystem::path(MC_SAVES_DIR) / worldName;
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
    if (level) MC_LOG_INFO("Loading world \"%s\" (seed %lld)", worldName.c_str(), static_cast<long long>(seed));
    else if (!worldName.empty()) MC_LOG_INFO("Creating world \"%s\"", worldName.c_str());
    // The player's dimension (M12): saved in level.dat; --dimension starts elsewhere.
    using mc::world::Dimension;
    Dimension dimension = Dimension::Overworld;
    if (level)
        if (const auto d = mc::world::findDimension(level->dimension)) dimension = *d;
    if (!opts->dimension.empty() && !flatWorld) dimension = *mc::world::findDimension(opts->dimension);
    // Each dimension saves in its own folder (vanilla: DIM-1 Nether, DIM1 End).
    auto dimensionDir = [&](Dimension d) { return worldDir / std::string(mc::world::dimensionInfo(d).folder); };
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
        RetiringStorage(std::filesystem::path d, std::thread t) : dir(std::move(d)), thread(std::move(t)) {}
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
    const std::string netherKind = level ? level->netherGenerator : std::string("nether2");
    const std::string endKind = level ? level->endGenerator : std::string("end2"); // (M20)
    if (endKind != "end" && endKind != "end2") {
        MC_LOG_ERROR("World \"%s\" uses End generator \"%s\", which this build doesn't have", worldName.c_str(),
                     endKind.c_str());
        return 1;
    }
    if (netherKind != "nether" && netherKind != "nether2") {
        MC_LOG_ERROR("World \"%s\" uses Nether generator \"%s\", which this build doesn't have", worldName.c_str(),
                     netherKind.c_str());
        return 1;
    }
    if (generatorKind != "terrain" && generatorKind != "overworld" && generatorKind != "overworld2") {
        // A world from a newer/other build: generating here would leave seams.
        MC_LOG_ERROR("World \"%s\" uses generator \"%s\", which this build doesn't have",
                     worldName.c_str(), generatorKind.c_str());
        return 1;
    }
    auto makeGenerator = [&](Dimension d) -> std::unique_ptr<mc::world::ChunkGenerator> {
        if (d == Dimension::Nether)
            return std::make_unique<mc::world::NetherGenerator>(seed, netherKind == "nether" ? 1 : 2);
        if (d == Dimension::End) return std::make_unique<mc::world::EndGenerator>(seed, endKind == "end" ? 1 : 2);
        if (generatorKind == "terrain") return std::make_unique<mc::world::TerrainGenerator>(seed);
        return std::make_unique<mc::world::OverworldGenerator>(seed, generatorKind == "overworld" ? 1 : 2);
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
            if (!c.furnaces().empty() || !c.mobs().empty() || !c.blockTicks().empty() || !c.spawners().empty() ||
                !c.brewingStands().empty())
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
        loader = std::make_unique<mc::world::ChunkLoader>(world, *generatorPtr, genThreads, storage.get());
        loader->setRenderDistance(opts->renderDistance);
        renderer.setRenderDistance(opts->renderDistance);
        spawn = generatorPtr->findSpawn();
    }
    // Lighting on worker threads (a quarter of the cores).
    mc::world::LightManager lighting(
        world, std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 4));
    std::vector<mc::world::ChunkPos> litChunks;
    std::vector<mc::world::SectionPos> relitSections;
    std::vector<mc::world::BlockPos> frameEdits; // all block edits this frame (for lighting)
    std::vector<mc::world::BlockPos> frameRemesh; // edits that don't change light: re-mesh at once
    std::vector<mc::world::BlockPos> frameSettling; // flowing fluids: re-mesh now, relight in the background
    frameSettling.reserve(4096);
    frameRemesh.reserve(4096);
    litChunks.reserve(256);
    relitSections.reserve(256);
    frameEdits.reserve(4096);
    std::vector<mc::world::BlockPos> editsReady;
    editsReady.reserve(4096); // (settling fluids come straight back through it)

    int64_t dayTime = level ? level->dayTime : opts->time; // world day time (world/DayTime.h)
    int64_t gameTime = level ? level->gameTime : 0;        // ticks since the world began

    mc::Player player;
    // The flight benchmark starts high above spawn so it never hits terrain.
    // (the flight benchmark flies at Y 220+, above any mountain, so it keeps streaming)
    player.setPosition(opts->hasPos ? opts->pos
                       : opts->autoFly ? glm::dvec3(spawn.x, std::max(spawn.y + 60.0, 220.0), spawn.z)
                                       : spawn);
    // Scripted views (--pos) and the flight benchmark start in the air: fly.
    player.setFlying(opts->hasPos || opts->autoFly);
    player.setRotation(opts->hasLook ? opts->yaw : 0.0f, opts->hasLook ? opts->pitch : 25.0f);

    if (opts->autoFly) player.setFlySpeedMultiplier(4.0);
    mc::Inventory inventory;
    // Survival (M9): game mode, health/hunger, dropped items.
    bool survival = level && level->survival;
    mc::Vitals vitals;
    vitals.setVoidY(mc::world::dimensionInfo(dimension).voidY);
    if (level) {
        vitals.setState(level->health, level->food, level->saturation, level->exhaustion);
        vitals.setFoodTimer(level->foodTimer);
        vitals.setAir(level->air);
        vitals.setFireTicks(level->fire);
        vitals.setExperience(level->xpLevel, level->xpProgress, level->xpTotal);
        vitals.setEnchantSeed(uint32_t(level->xpSeed));
        for (const auto& e : level->effects) // (kinds we don't have are dropped)
            if (const auto kind = mc::world::findEffect(e.id)) vitals.addEffect(*kind, e.amplifier, e.duration);
    }
    mc::DragonFight dragonFight; // (M20.2; end2 worlds)
    if (level) {
        dragonFight.killed = level->dragonKilled;
        dragonFight.previouslyKilled = level->dragonPreviouslyKilled;
        dragonFight.uuidHi = level->dragonUuidHi;
        dragonFight.uuidLo = level->dragonUuidLo;
        dragonFight.gateways = level->gateways;
        dragonFight.gatewaysReady = level->hasGateways;
    }
    mc::ItemEntities droppedItems;
    mc::FallingBlocks fallingBlocks; // sand and gravel in the air (M16)
    mc::Projectiles projectiles;     // arrows and eggs (M16.4)
    mc::ExperienceOrbs orbs;         // experience orbs (M17.5)
    mc::Explosion fireballBlast;     // ghast fireballs (M19.2)
    int bowTicks = 0;                // how long the bow has been drawn
    double airPeakY = 0.0;           // highest feet height since leaving the ground (trampling)
    int shieldTicks = 0;             // how long right-click has held a shield up
    // Beds (M17.4): the respawn point, sleeping (ticks asleep, the bed's head).
    std::optional<mc::world::BlockPos> bedSpawn;
    if (level && level->hasRespawn)
        bedSpawn = mc::world::BlockPos{level->respawn[0], level->respawn[1], level->respawn[2]};
    int sleepTicks = 0;
    mc::world::BlockPos sleepBed{};
    bool bedRespawnPending = false; // respawned at the bed: check it once its chunks load
    mc::Explosion bedExplosion;     // beds in the Nether and the End
    std::optional<mc::world::BlockPos> pendingBedUse;
    mc::Mobs mobs;
    mc::world::Xoroshiro gameRng(seed ^ 0x5EEDull);
    std::vector<mc::BlockInteraction::Drop> drops;
    drops.reserve(64); // (a full chest: 27 stacks and the chest)
    bool dead = false;
    mc::gfx::EntityRenderer entities;
    if (!entities.init(renderer.atlas(), renderer.models(), itemIcons, renderer.packs())) return 1;
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
            if (const auto p = mc::world::findPotion(it.potion)) s.potion = static_cast<uint8_t>(*p);
            // Our armor slots: 100 feet .. 103 head (vanilla's old numbers); 150 offhand.
            if (it.slot >= 100 && it.slot <= 103) inventory.setArmor(103 - it.slot, s);
            else if (it.slot == 150) inventory.setOffhand(s);
            else if (it.slot >= 0 && it.slot < mc::Inventory::kSlots) inventory.setSlot(it.slot, s);
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
    else if (dimension != Dimension::Overworld) { // a new world started elsewhere: spawn is the Overworld's
        const glm::dvec3 s = makeGenerator(Dimension::Overworld)->findSpawn();
        worldSpawn[0] = int32_t(std::floor(s.x)), worldSpawn[1] = int32_t(std::floor(s.y)), worldSpawn[2] = int32_t(std::floor(s.z));
    }
    // Portals players lit or came through (level.dat, our tag), travel state (M12).
    std::vector<mc::portals::Known> knownPortals;
    if (level)
        for (const auto& p : level->portals)
            if (const auto d = mc::world::findDimension(p.dimension)) knownPortals.push_back({*d, {p.x, p.y, p.z}});
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
        arrival = Travel{Dimension::End, Travel::Via::EndPortal, {100, 49, 0}, Dimension::End, {100.5, 49.0, 0.5}};
    // Just arrived (or loaded, maybe standing in one): step out of the portal first.
    bool portalCooldown = level.has_value();
    int pearlCooldown = 0;
    int64_t sessionTicks = 0;
    // (Overworld only: elsewhere the highest ground is a roof; findSpawn is exact.)
    bool spawnPending = !level && !opts->hasPos && !opts->autoFly && !flatWorld && dimension == Dimension::Overworld;
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
        l.name = worldName;
        l.seed = seed;
        l.flat = flatWorld;
        l.generator = generatorKind;
        l.netherGenerator = netherKind;
        l.endGenerator = endKind;
        l.dragonKilled = dragonFight.killed;
        l.dragonPreviouslyKilled = dragonFight.previouslyKilled;
        l.dragonUuidHi = dragonFight.uuidHi;
        l.dragonUuidLo = dragonFight.uuidLo;
        l.gateways = dragonFight.gateways;
        l.hasGateways = dragonFight.gatewaysReady;
        l.cloneFormat = cloneFormat;
        // Mid-travel the player is still where they left from (a reload re-enters).
        l.dimension = std::string(mc::world::dimensionInfo(arrival ? arrival->fromDimension : dimension).id);
        for (const auto& k : knownPortals)
            l.portals.push_back({std::string(mc::world::dimensionInfo(k.dimension).id), k.pos.x, k.pos.y, k.pos.z});
        for (int i = 0; i < 3; ++i)
            l.spawn[i] = worldSpawn[i];
        l.dayTime = dayTime;
        l.gameTime = gameTime;
        const glm::dvec3 p = arrival ? arrival->fromPos : player.position();
        l.pos[0] = p.x;
        l.pos[1] = p.y;
        l.pos[2] = p.z;
        l.yaw = player.yaw();
        l.pitch = player.pitch();
        l.flying = player.flying();
        l.survival = survival;
        l.health = vitals.health();
        l.food = vitals.food();
        l.saturation = vitals.saturation();
        l.exhaustion = vitals.exhaustion();
        l.foodTimer = vitals.foodTimer();
        l.air = vitals.air();
        l.xpLevel = vitals.xpLevel();
        l.xpProgress = vitals.xpProgress();
        l.xpTotal = vitals.xpTotal();
        l.xpSeed = int32_t(uint32_t(vitals.enchantSeed()));
        for (const auto& e : vitals.effects())
            if (e.duration > 0)
                l.effects.push_back({std::string(mc::world::effectInfo(e.type).id), e.amplifier, e.duration});
        l.hasRespawn = bedSpawn.has_value();
        if (bedSpawn) l.respawn[0] = bedSpawn->x, l.respawn[1] = bedSpawn->y, l.respawn[2] = bedSpawn->z;
        l.fire = vitals.fireTicks();
        auto saveSlot = [&](int slot, const mc::world::ItemStack& s) {
            if (s.empty()) return;
            mc::world::LevelData::SavedItem it{slot, mc::world::itemRegistry().item(s.item).id,
                                               s.state ? mc::world::blockRegistry().toString(s.state) : std::string(),
                                               s.count, s.damage};
            for (const uint16_t v : s.enchantments)
                if (v) it.enchantments.emplace_back(
                    std::string(mc::world::enchantmentInfo(mc::world::Enchantment(v >> 8)).id), int(v & 0xFF));
            it.repairCost = s.repairCost;
            if (s.potion) it.potion = std::string(mc::world::potionInfo(static_cast<mc::world::Potion>(s.potion)).id);
            it.storedEnchantments = it.id == "minecraft:enchanted_book";
            l.inventory.push_back(std::move(it));
        };
        for (int i = 0; i < mc::Inventory::kSlots; ++i)
            saveSlot(i, inventory.slot(i));
        for (int piece = 0; piece < 4; ++piece) // head = 103 .. feet = 100
            saveSlot(103 - piece, inventory.armor(piece));
        saveSlot(150, inventory.offhand());
        l.selectedSlot = inventory.selected();
        if (!l.save(worldDir)) MC_LOG_ERROR("Failed to write level.dat");
        if (wait) storage->flush();
        MC_LOG_INFO("Saved world \"%s\" (%d changed chunks)", worldName.c_str(), chunks);
    };
    if (storage && !level) saveWorld(false); // a new world gets its level.dat at once
    mc::BlockInteraction interaction;
    mc::world::BlockUpdates blockUpdates(world); // block updates, scheduled ticks, redstone (M11)
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
            mc::CommandContext ctx{player, inventory, dayTime, gameTime, opts->seed, &survival, &vitals, &world, &gameRng,
                                  &frameEdits};
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
    auto pointChests = [&] {
        auto chestAt = [&](const mc::world::BlockPos& p) -> mc::world::ChestData* {
            mc::world::Chunk* c = world.chunk(p.chunk());
            return c ? c->chest(mc::world::blockToLocal(p.x), p.y, mc::world::blockToLocal(p.z)) : nullptr;
        };
        mc::world::ChestData* first = chestAt(containerBlock);
        mc::world::ChestData* second = chestSecond ? chestAt(*chestSecond) : nullptr;
        if (!first || (chestSecond && !second)) {
            screenDrops.clear();
            container.close(inventory, screenDrops);
            pendingThrows.insert(pendingThrows.end(), screenDrops.begin(), screenDrops.end()); // (spawned in the tick)
            if (!screenshotMode) window.setCursorCaptured(true);
            return;
        }
        container.setChests(first, second);
    };
    // Opens a chest's screen unless a solid block sits on it (wiki: Chest); a double
    // chest shows its "left" half first.
    auto openChestAt = [&](const mc::world::BlockPos& p) {
        const auto& creg = mc::world::blockRegistry();
        if (creg.blockOf(world.getBlock(p)) != mc::world::blocks::Chest) return false;
        const auto partner = mc::world::BlockUpdates::chestPartner(world, p);
        const bool blocked = creg.opaqueCube(world.getBlock({p.x, p.y + 1, p.z})) ||
                             (partner && creg.opaqueCube(world.getBlock({partner->x, partner->y + 1, partner->z})));
        if (blocked) return false;
        const bool leftFirst = creg.value(world.getBlock(p), "type") != "right";
        containerBlock = partner && !leftFirst ? *partner : p;
        chestSecond = partner ? std::optional(leftFirst ? *partner : p) : std::nullopt;
        container.openChest(nullptr, nullptr);
        pointChests();
        return true;
    };
    bool openBlockPending = opts->hasOpenBlock;
    while (!window.shouldClose()) {
        window.pollEvents();
        // An open chest screen follows its block(s) (closed if broken).
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Chest) pointChests();
        // An open brewing stand screen follows its block (closed if it was broken).
        if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Brewing) {
            mc::world::Chunk* c = world.chunk(containerBlock.chunk());
            mc::world::BrewingData* b = c ? c->brewing(mc::world::blockToLocal(containerBlock.x), containerBlock.y,
                                                       mc::world::blockToLocal(containerBlock.z))
                                          : nullptr;
            container.setBrewing(b);
            if (!b) {
                screenDrops.clear();
                container.close(inventory, screenDrops);
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
            if (f) container.setFurnace(f);
            else {
                screenDrops.clear();
                container.close(inventory, screenDrops);
                if (!screenshotMode) window.setCursorCaptured(true);
            }
        }
        // Text typed this frame (only the chat consumes it).
        const int typedCount = window.takeText(typed.data(), static_cast<int>(typed.size()));
        if (chat.isOpen()) {
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
            for (auto p : {mc::Press::Chat, mc::Press::Command, mc::Press::F3,
                           mc::Press::Inventory, mc::Press::LeftMouse, mc::Press::RightMouse})
                window.takePresses(p); // typing, not game keys
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
            screenDrops.clear();
            for (int n = window.takePresses(mc::Press::LeftMouse); n > 0; --n)
                container.click(mx, my, mc::ui::ContainerScreen::Button::Left, shift, fw / scale, fh / scale,
                                inventory, screenDrops);
            for (int n = window.takePresses(mc::Press::RightMouse); n > 0; --n)
                container.click(mx, my, mc::ui::ContainerScreen::Button::Right, shift, fw / scale, fh / scale,
                                inventory, screenDrops);
            // Contents edited through the screen: the block entity's chunk needs saving.
            if (container.type() == mc::ui::ContainerScreen::Type::Chest ||
                container.type() == mc::ui::ContainerScreen::Type::Furnace ||
                container.type() == mc::ui::ContainerScreen::Type::Brewing) {
                if (mc::world::Chunk* c = world.chunk(containerBlock.chunk())) c->markDirty();
                if (chestSecond)
                    if (mc::world::Chunk* c = world.chunk(chestSecond->chunk())) c->markDirty();
            }
            if (window.takePresses(mc::Press::Escape) > 0 || window.takePresses(mc::Press::Inventory) > 0) {
                container.close(inventory, screenDrops);
                if (!screenshotMode) window.setCursorCaptured(true);
                attackArmed = false;
            }
            pendingThrows.insert(pendingThrows.end(), screenDrops.begin(), screenDrops.end()); // next tick
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
                const bool down = window.keyDown(static_cast<mc::Key>(static_cast<int>(mc::Key::Num1) + i));
                if (down && !numberWasDown[i]) creative.numberKey(i, mx, my, fw / scale, fh / scale, inventory);
            }
            if (window.takePresses(mc::Press::Escape) > 0 || window.takePresses(mc::Press::Inventory) > 0) {
                creative.close();
                if (!screenshotMode) window.setCursorCaptured(true);
                attackArmed = false;
            }
            for (auto p : {mc::Press::Chat, mc::Press::Command, mc::Press::F3, mc::Press::RightMouse,
                           mc::Press::Backspace, mc::Press::Up, mc::Press::Down, mc::Press::Enter,
                           mc::Press::Drop})
                window.takePresses(p);
        } else {
            if (dead && window.takePresses(mc::Press::Enter) > 0) { // respawn at the bed or world spawn
                dead = false;
                vitals.reset();
                if (bedSpawn && dimension == Dimension::Overworld) {
                    player.setPosition({bedSpawn->x + 0.5, bedSpawn->y + 1.0, bedSpawn->z + 0.5});
                    player.setVelocity(glm::dvec3(0.0));
                    bedRespawnPending = true; // checked once its chunks are loaded
                } else if (dimension != Dimension::Overworld) {
                    pendingTravel = Travel{Dimension::Overworld, Travel::Via::Respawn, {}}; // spawn is in the Overworld
                } else {
                    player.setPosition(glm::dvec3(worldSpawn[0] + 0.5, worldSpawn[1], worldSpawn[2] + 0.5));
                    spawnPending = !flatWorld; // settle on the ground there again
                    if (spawnPending) spawn = glm::dvec3(worldSpawn[0] + 0.5, worldSpawn[1], worldSpawn[2] + 0.5);
                }
            }
            for (auto p : {mc::Press::Backspace, mc::Press::Up, mc::Press::Down, mc::Press::Enter})
                window.takePresses(p);
            if (window.takePresses(mc::Press::F3) > 0) showDebug = !showDebug;
            if (window.cursorCaptured()) {
                // T opens the chat, / opens it with the slash typed (vanilla).
                if (window.takePresses(mc::Press::Command) > 0) {
                    chat.open("/");
                    window.setCursorCaptured(false);
                } else if (window.takePresses(mc::Press::Chat) > 0) {
                    chat.open();
                    window.setCursorCaptured(false);
                } else if (window.takePresses(mc::Press::Inventory) > 0) {
                    if (survival) container.open(mc::ui::ContainerScreen::Type::Inventory);
                    else creative.open();
                    window.setCursorCaptured(false);
                } else if (lastHit && !window.keyDown(mc::Key::LeftShift) && window.takePresses(mc::Press::RightMouse) > 0) {
                    // Using a workstation opens its screen (sneaking places against it).
                    const auto& reg = mc::world::blockRegistry();
                    const auto block = reg.blockOf(world.getBlock(lastHit->block));
                    if (block == mc::world::blocks::CraftingTable) {
                        container.open(mc::ui::ContainerScreen::Type::Crafting);
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::Furnace) {
                        containerBlock = lastHit->block;
                        container.open(mc::ui::ContainerScreen::Type::Furnace);
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::BrewingStand) {
                        containerBlock = lastHit->block;
                        mc::world::Chunk* bc = world.chunk(containerBlock.chunk());
                        container.openBrewing(bc ? bc->brewing(mc::world::blockToLocal(containerBlock.x), containerBlock.y,
                                                               mc::world::blockToLocal(containerBlock.z))
                                                 : nullptr);
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::RedBed) {
                        pendingBedUse = lastHit->block; // used in the next tick (simulation stays in ticks)
                    } else if (block == mc::world::blocks::EnchantingTable) {
                        containerBlock = lastHit->block;
                        container.openEnchanting(mc::countBookshelves(world, lastHit->block), vitals.enchantSeed());
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::Anvil || block == mc::world::blocks::ChippedAnvil ||
                               block == mc::world::blocks::DamagedAnvil) {
                        containerBlock = lastHit->block;
                        container.openAnvil();
                        window.setCursorCaptured(false);
                    } else if (block == mc::world::blocks::Chest) {
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
                if (window.takePresses(mc::Press::Escape) > 0 && window.cursorCaptured()) {
                    window.setCursorCaptured(false);
                    saveWorld(false); // vanilla saves when the game pauses
                }
            }
        }
        // Key edges for the creative's number keys: tracked every frame, so a key
        // held while the screen opens doesn't count as a fresh press.
        for (int i = 0; i < mc::Inventory::kHotbar; ++i)
            numberWasDown[i] = window.keyDown(static_cast<mc::Key>(static_cast<int>(mc::Key::Num1) + i));
        // Game presses made while the mouse isn't captured (typing, menus) must not
        // act later (spaces typed in chat would toggle flight).
        if (!window.cursorCaptured()) {
            window.takePresses(mc::Press::Jump);
            window.takePresses(mc::Press::RightMouse);
            window.takePresses(mc::Press::Drop);
        }
        player.turn(window.mouseDx(), window.mouseDy());
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
        clock.advance(now - last);
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
                MC_LOG_INFO("Travelling to %s", std::string(mc::world::dimensionInfo(t.to).id).c_str());
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
                orbs.clear();
                const Dimension from = dimension;
                dimension = t.to;
                world.setHasSkyLight(mc::world::dimensionInfo(dimension).hasSkyLight);
                world.setHeight(mc::world::dimensionInfo(dimension).height); // (no chunks are loaded now)
                world.setUltrawarm(dimension == Dimension::Nether);
                renderer.setDimension(dimension);
                vitals.setVoidY(mc::world::dimensionInfo(dimension).voidY);
                // The old storage finishes its writes on a thread of its own (flushing
                // hundreds of chunks would freeze the frame); a folder is reopened only
                // after its previous storage has finished.
                if (storage) {
                    retiringStorage.push_back({dimensionDir(from), std::thread([old = std::move(storage)]() mutable { old.reset(); })});
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
                loader = std::make_unique<mc::world::ChunkLoader>(world, *generatorPtr, genThreads, storage.get());
                loader->setRenderDistance(opts->renderDistance);
                arrival = t;
                if (t.via == Travel::Via::NetherPortal) arrival->from = mc::portals::destination(from, dimension, t.from);
                else if (t.via == Travel::Via::EndPortal) arrival->from = {100, 49, 0};
                else arrival->from = {worldSpawn[0], worldSpawn[1], worldSpawn[2]};
                // Wait at the destination; unloaded chunks hold the player up meanwhile.
                player.setPosition({arrival->from.x + 0.5, double(arrival->from.y), arrival->from.z + 0.5});
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
                        if (++arrivalWait < 400) ready = false;
                        else found.reset();
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
                            while (mc::world::blockRegistry().blockOf(world.getBlock({in.x, in.y - 1, in.z})) ==
                                   mc::world::blocks::NetherPortal)
                                --in.y;
                        } else {
                            const bool nether = dimension == Dimension::Nether;
                            in = mc::portals::build(world, a.from, nether ? 32 : world.height().minY + 8,
                                                    // vanilla: up to 10 below the logical top
                                                    mc::world::dimensionInfo(dimension).logicalHeight +
                                                        world.height().minY - 10,
                                                    frameEdits);
                            knownPortals.push_back({dimension, in});
                        }
                        player.setPosition({in.x + 0.5, double(in.y), in.z + 0.5});
                    } else if (a.via == Travel::Via::EndPortal) {
                        player.setPosition(mc::portals::endPlatform(world, frameEdits));
                        player.setRotation(90.0f, 0.0f); // facing west, toward the island (wiki: End Platform)
                    } else if (bedSpawn) { // back from the Nether/End to the bed (wiki: Bed)
                        player.setPosition({bedSpawn->x + 0.5, bedSpawn->y + 1.0, bedSpawn->z + 0.5});
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
                    if (const auto spot = mc::bedStandSpot(world, *bedSpawn)) {
                        player.setPosition(*spot);
                    } else { // gone or blocked: the world spawn (wiki: Bed)
                        chat.addMessage("You have no home bed or charged respawn anchor, or it was obstructed",
                                        0xFFFFFFFFu, gameTime, gui.batch());
                        bedSpawn.reset();
                        spawn = glm::dvec3(worldSpawn[0] + 0.5, worldSpawn[1], worldSpawn[2] + 0.5);
                        player.setPosition(spawn);
                        spawnPending = !flatWorld;
                    }
                    vitals.resetFall();
                } else {
                    player.setVelocity(glm::dvec3(0.0));
                }
            }
            blockUpdates.setTime(gameTime);
            blockUpdates.setCreative(!survival);
            // A bed used this frame (M17.4): sleep, set the respawn point, or explode.
            if (pendingBedUse && !dead) {
                const mc::world::BlockPos bedPos = *pendingBedUse;
                pendingBedUse.reset();
                const glm::dvec3 feetNow = player.position();
                switch (mc::useBed(world, bedPos, dayTime, dimension, !survival, &feetNow)) {
                        case mc::BedUse::Sleep:
                            bedSpawn = *mc::bedHead(world, bedPos);
                            sleepBed = *bedSpawn;
                            sleepTicks = 1;
                            chat.addMessage("Respawn point set", 0xFFFFFFFFu, gameTime, gui.batch());
                            break;
                        case mc::BedUse::NotNight:
                            bedSpawn = *mc::bedHead(world, bedPos);
                            chat.addMessage("Respawn point set", 0xFFFFFFFFu, gameTime, gui.batch());
                            chat.addMessage("You can sleep only at night", 0xFFFFFFFFu, gameTime, gui.batch());
                            break;
                        case mc::BedUse::Monsters:
                            chat.addMessage("You may not rest now; there are monsters nearby", 0xFFFFFFFFu, gameTime,
                                            gui.batch());
                            break;
                        case mc::BedUse::Occupied:
                            chat.addMessage("This bed is occupied", 0xFFFFFFFFu, gameTime, gui.batch());
                            break;
                        case mc::BedUse::Explodes: { // wiki: Bed - power 5 outside the Overworld
                            const mc::world::BlockPos head = *mc::bedHead(world, bedPos);
                            world.updateBlock(head, 0); // (the foot follows)
                            frameEdits.push_back(head);
                            mc::ExplosionTargets t;
                            if (survival) {
                                t.player = &player;
                                t.vitals = &vitals;
                            }
                            bedExplosion.explode(world, {head.x + 0.5, head.y + 0.5, head.z + 0.5}, 5.0f, gameRng,
                                                 droppedItems, frameEdits, t);
                            break;
                        }
                        case mc::BedUse::TooFar:
                            chat.addMessage("You can't rest now; the bed is too far away", 0xFFFFFFFFu, gameTime,
                                            gui.batch());
                            break;
                        case mc::BedUse::Obstructed:
                            chat.addMessage("This bed is obstructed", 0xFFFFFFFFu, gameTime, gui.batch());
                            break;
                        case mc::BedUse::NotABed: break;
                        }
            }
            pendingBedUse.reset();

            // Commands wait until the player's chunk is there (--command scripts run
            // before the world has streamed in otherwise).
            if (!pendingChat.empty() && world.chunk(mc::world::ChunkPos{
                                            mc::world::blockToChunk(int(std::floor(player.position().x))),
                                            mc::world::blockToChunk(int(std::floor(player.position().z)))}) &&
                !spawnPending && !arrival) {
                for (const auto& line : pendingChat)
                    runChatLine(line);
                pendingChat.clear();
            }
            for (const auto& t : pendingThrows)
                droppedItems.throwFrom(player.eyePosition(1.0),
                                       glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())), t, gameRng);
            pendingThrows.clear();
            mc::PlayerInput input = readInput(window);
            // Presses since the last tick (only the first tick of a frame sees them).
            input.jumpPresses = window.cursorCaptured() ? window.takePresses(mc::Press::Jump) : 0;
            if (opts->autoFly) { // benchmark: constant sprint-flight forward
                input.forward = 1.0f;
                input.sprint = true;
            }
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
                    if (sleepTicks >= 100) dayTime = mc::morningAfter(dayTime); // the night passes
                    if (stillBed)
                        if (const auto spot = mc::bedStandSpot(world, sleepBed)) player.setPosition(*spot);
                    sleepTicks = 0;
                }
            }
            input.canSprint = !survival || vitals.canSprint(); // hunger ends a sprint too
            const glm::dvec3 before = player.position();
            const bool wasOnGround = player.onGround();
            {
                using E = mc::world::Effect;
                vitals.tickEffects(); // (M19.4: in any game mode)
                player.setEffects(vitals.effectLevel(E::Speed), vitals.effectLevel(E::Slowness),
                                  vitals.effectLevel(E::JumpBoost), vitals.effectLevel(E::SlowFalling) > 0);
            }
            if (!arrival) player.tick(world, input); // waiting for a destination: held in place
            const auto& reg = mc::world::blockRegistry();
            const glm::dvec3 feet = player.position();
            const mc::world::BlockPos feetBlock{int(std::floor(feet.x)), int(std::floor(feet.y)), int(std::floor(feet.z))};
            const bool inWater = reg.blockOf(world.getBlock(feetBlock)) == mc::world::blocks::Water;
            // Landing on farmland may trample it: chance fall distance - 0.5 (wiki: Farmland).
            if (player.onGround() || player.flying() || inWater) {
                if (player.onGround() && !wasOnGround && !player.flying()) {
                    const mc::world::BlockPos below{feetBlock.x, int(std::floor(feet.y - 0.01)), feetBlock.z};
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
                    vitals.exhaust(0.1f * float(glm::length(glm::dvec2(feet.x - before.x, feet.z - before.z))));
                if (wasOnGround && !player.onGround() && player.velocity().y > 0.0)
                    vitals.exhaust(player.sprinting() ? 0.2f : 0.05f);
                if (!arrival) {
                    vitals.tick(feet.y, player.onGround(), inWater || player.inWater(), player.flying());
                    // Drowning, lava and burning (M14; wiki: Drowning, Lava, Fire).
                    const int respiration = mc::world::enchantLevel(inventory.armor(0), mc::world::Enchantment::Respiration);
                    vitals.breathe(mc::pointInFluid(world, player.eyePosition(1.0), mc::world::blocks::Water),
                                   respiration > 0 && gameRng.nextInt(uint32_t(respiration + 1)) > 0);
                    if (player.inLava()) {
                        vitals.attacked(4.0f, nullptr, mc::Vitals::Hit::Fire); // lava: armor reduces it (wiki: Armor)
                        vitals.setOnFire(300); // 15 s
                    }
                    vitals.touchFire(mc::portals::touching(world, player.box(), mc::world::blocks::Fire));
                    // Touching a cactus (beside or on top) hurts 1 (wiki: Cactus); our
                    // cactus collides as a full cube, so the box reaches out a hair.
                    if (mc::portals::touching(world, player.box().inflated(0.001), mc::world::blocks::Cactus))
                        vitals.attacked(1.0f);
                    vitals.tickFire(player.inWater());
                }
            }
            if (!dead && vitals.dead()) { // drop everything where we died (keepInventory off)
                {
                    // Open screens close first: their grid/carried items drop too.
                    screenDrops.clear();
                    if (container.isOpen()) container.close(inventory, screenDrops);
                    if (creative.isOpen()) creative.close();
                    for (const auto& d : screenDrops)
                        droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), d, gameRng);
                    dead = true;
                    for (int s = 0; s < mc::Inventory::kSlots; ++s) {
                        droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), inventory.slot(s), gameRng, 40);
                        inventory.setSlot(s, {});
                    }
                    for (int piece = 0; piece < 4; ++piece) { // worn armor and the offhand too
                        droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), inventory.armor(piece), gameRng, 40);
                        inventory.setArmor(piece, {});
                    }
                    droppedItems.spawn(feet + glm::dvec3(0, 0.5, 0), inventory.offhand(), gameRng, 40);
                    inventory.setOffhand({});
                    orbs.drop(feet + glm::dvec3(0, 0.5, 0), vitals.deathExperience(), gameRng); // (the rest is lost)
                    chat.addMessage("Player died", 0xFFFFFFFFu, gameTime, gui.batch());
                }
            }
            // Q drops one of the held item (wiki: Controls).
            if (window.cursorCaptured() && window.takePresses(mc::Press::Drop) > 0 && !inventory.selectedStack().empty()) {
                mc::world::ItemStack one = inventory.selectedStack();
                one.count = 1;
                droppedItems.throwFrom(player.eyePosition(1.0),
                                       glm::dvec3(mc::world::lookVector(player.yaw(), player.pitch())), one,
                                       gameRng);
                inventory.consumeSelected(1);
            }
            mc::InteractionInput clicks;
            clicks.attack = window.cursorCaptured() && attackArmed && window.leftMousePressed();
            clicks.use = window.cursorCaptured() && window.rightMousePressed();
            clicks.attackClick =
                window.cursorCaptured() && window.takePresses(mc::Press::LeftMouse) > 0;
            clicks.useClick =
                window.cursorCaptured() && window.takePresses(mc::Press::RightMouse) > 0;
            // Flint and steel lights portals; eyes of ender go into end portal frames (M12).
            if (!dead && clicks.useClick && lastHit) { // (sneaking only skips block actions)
                const mc::world::ItemStack held = inventory.selectedStack();
                const size_t editsBefore = frameEdits.size();
                if (!held.empty() &&
                    mc::portals::useItem(world, dimension, held.item, lastHit->block, lastHit->face, frameEdits)) {
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
                static const mc::world::ItemId shieldItem = *mc::world::itemRegistry().find("shield");
                const bool shieldInHand = inventory.selectedStack().item == shieldItem ||
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
                if (!dead && clicks.useClick && inventory.equipSelected()) { // armor in hand: put it on
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Bows and eggs (M16.4; wiki: Bow, Egg): hold right-click to draw a bow (it
            // needs an arrow in survival), release to shoot; right-click throws an egg.
            {
                const std::string_view heldId = mc::world::itemRegistry().item(inventory.selectedStack().item).id;
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (!dead && heldId == "minecraft:bow" && clicks.use && mc::canDrawBow(inventory, survival)) {
                    ++bowTicks;
                    clicks.useClick = false;
                } else if (bowTicks > 0) {
                    if (!dead && heldId == "minecraft:bow")
                        mc::releaseBow(inventory, bowTicks, survival, eye, look, projectiles, gameRng);
                    bowTicks = 0;
                }
                // Eyes of ender (M18.5) fly toward the nearest stronghold - in the
                // Overworld, unless aimed at an end portal frame (that fills it).
                if (!dead && heldId == "minecraft:ender_eye" && clicks.useClick && dimension == Dimension::Overworld &&
                    !(lastHit && mc::world::blockRegistry().blockOf(world.getBlock(lastHit->block)) ==
                                     mc::world::blocks::EndPortalFrame))
                    if (const auto s = generatorPtr->nearestStronghold(eye.x, eye.z)) {
                        mc::throwEye(inventory, survival, eye, *s, projectiles);
                        clicks.useClick = false;
                    }
                if (!dead && (clicks.useClick || clicks.attackClick) && lastHit &&
                    reg.blockOf(world.getBlock(lastHit->block)) == mc::world::blocks::DragonEgg) {
                    // The egg teleports away when clicked (wiki: Dragon Egg).
                    mc::DragonFight::teleportEgg(world, lastHit->block, gameRng, frameEdits);
                    clicks.useClick = clicks.attackClick = clicks.attack = false;
                }
                if (!dead && heldId == "minecraft:end_crystal" && clicks.useClick && lastHit &&
                    lastHit->face == mc::world::Direction::Up) { // (M20.1)
                    if (mc::Mobs::placeEndCrystal(world, lastHit->block, gameRng) && survival)
                        inventory.consumeSelected(1);
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:ender_pearl" && clicks.useClick) { // (M20.3)
                    if (pearlCooldown == 0) {
                        mc::throwPearl(inventory, survival, eye, player.yaw(), player.pitch(), projectiles, gameRng);
                        pearlCooldown = 20; // (wiki: 1 s between throws)
                    }
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:splash_potion" && clicks.useClick) { // (M19.4)
                    mc::throwSplashPotion(inventory, survival, eye, player.yaw(), player.pitch(), projectiles, gameRng);
                    clicks.useClick = false;
                }
                if (!dead && heldId == "minecraft:egg" && clicks.useClick) {
                    mc::throwEgg(inventory, survival, eye, look, projectiles, gameRng);
                    clicks.useClick = false;
                }
            }
            // Hoes and bone meal (M17.1; wiki: Hoe, Bone Meal) on the targeted block.
            if (!dead && clicks.useClick && lastHit && !inventory.selectedStack().empty()) {
                const mc::world::ItemStack held = inventory.selectedStack();
                const auto& def = mc::world::itemRegistry().item(held.item);
                static const mc::world::ItemId boneMealItem = *mc::world::itemRegistry().find("bone_meal");
                if (def.tool == mc::world::ToolType::Hoe &&
                    mc::world::BlockUpdates::till(world, lastHit->block, lastHit->face)) {
                    frameEdits.push_back(lastHit->block);
                    if (survival) inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng)); // 1 per block
                    clicks.useClick = false;
                    clicks.use = false;
                } else if (held.item == boneMealItem && blockUpdates.boneMeal(lastHit->block)) {
                    if (survival) inventory.consumeSelected(1);
                    clicks.useClick = false;
                    clicks.use = false;
                }
            }
            // Feeding and shearing animals (M16.3): right-click the mob in front.
            if (!dead && clicks.useClick && !inventory.selectedStack().empty()) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                if (const auto mh = mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0);
                    mh && (!lastHit || mh->distance < lastHit->distance)) {
                    auto& mob = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    const mc::world::ItemStack held = inventory.selectedStack();
                    const auto use = mc::Mobs::interact(mob, held.item, gameRng, droppedItems);
                    if (use != mc::Mobs::Use::None) {
                        world.chunk(mh->chunk)->markDirty();
                        if (survival && use == mc::Mobs::Use::Fed) inventory.consumeSelected(1);
                        if (survival && use == mc::Mobs::Use::Sheared) // shears wear 1 per sheep
                            inventory.setSlot(inventory.selected(), mc::wearItem(held, 1, gameRng));
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
                const bool blockUse = lastHit && !player.sneaking() &&
                                      mc::world::BlockUpdates::usable(world.getBlock(lastHit->block));
                // A glass bottle fills from water (wiki: Glass Bottle): a water bottle.
                if (heldId == "minecraft:glass_bottle" && clicks.useClick && !blockUse) {
                    const glm::dvec3 eye = player.eyePosition(1.0);
                    const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                    const double reach = survival ? mc::world::kSurvivalReach : mc::world::kCreativeReach;
                    for (double t = 0.0; t <= reach; t += 0.1) {
                        const glm::dvec3 q = eye + look * t;
                        const mc::world::BlockPos b{int(std::floor(q.x)), int(std::floor(q.y)), int(std::floor(q.z))};
                        const mc::world::BlockStateId s = world.getBlock(b);
                        if (mc::world::blockRegistry().blockOf(s) == mc::world::blocks::Water) {
                            static const mc::world::ItemId potionItem = *mc::world::itemRegistry().find("potion");
                            mc::world::ItemStack water{potionItem, 1};
                            water.potion = static_cast<uint8_t>(mc::world::Potion::Water);
                            if (survival) inventory.consumeSelected(1);
                            if (inventory.add(water) > 0) droppedItems.spawn(player.position(), water, gameRng);
                            clicks.useClick = false;
                            break;
                        }
                        if (mc::world::blockRegistry().collides(s)) break;
                    }
                }
                if (heldId.ends_with("bucket") && heldId != "minecraft:milk_bucket" && !blockUse) {
                    const glm::dvec3 eye = player.eyePosition(1.0);
                    const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                    const double reach = survival ? mc::world::kSurvivalReach : mc::world::kCreativeReach;
                    std::optional<mc::BucketResult> result;
                    if (heldId == "minecraft:bucket")
                        if (const auto mh = mc::Mobs::raycast(world, eye, look, survival ? 3.0 : 5.0);
                            mh && (!lastHit || mh->distance < lastHit->distance) && // not through walls
                            world.chunk(mh->chunk)->mobs()[size_t(mh->index)].type == mc::world::MobType::Cow)
                            result = mc::BucketResult{*mc::world::itemRegistry().find("milk_bucket")};
                    if (!result) result = mc::useBucket(world, held.item, eye, look, reach, frameEdits);
                    if (result) {
                        if (!result->washed.empty() && survival) {
                            const mc::world::BlockPos w = frameEdits.back();
                            droppedItems.spawn({w.x + 0.5, w.y + 0.25, w.z + 0.5}, result->washed, gameRng);
                        }
                        const mc::world::ItemStack extra = mc::applyBucket(inventory, result->filled, survival);
                        if (!extra.empty())
                            droppedItems.spawn(player.position() + glm::dvec3(0, 1, 0), extra, gameRng);
                        clicks.useClick = false;
                        clicks.use = false;
                    }
                }
            }
            // Portals (wiki: Nether portal - 4 s inside in survival, at once in creative;
            // End portal - at once). Arriving players step out before they can go back.
            if (!dead && !flatWorld && !pendingTravel && !arrival) {
                const mc::Aabb box = player.box();
                const mc::world::BlockPos portalFeet{int(std::floor(feet.x)), int(std::floor(feet.y)), int(std::floor(feet.z))};
                if (mc::portals::touching(world, box, mc::world::blocks::EndPortal)) {
                    if (!portalCooldown)
                        pendingTravel = Travel{dimension == Dimension::End ? Dimension::Overworld : Dimension::End,
                                               dimension == Dimension::End ? Travel::Via::Respawn : Travel::Via::EndPortal,
                                               portalFeet};
                } else if (dimension != Dimension::End && mc::portals::touching(world, box, mc::world::blocks::NetherPortal)) {
                    if (!portalCooldown && ++portalTicks >= (survival ? 80 : 1))
                        pendingTravel = Travel{dimension == Dimension::Nether ? Dimension::Overworld : Dimension::Nether,
                                               Travel::Via::NetherPortal, portalFeet};
                } else {
                    portalTicks = 0;
                    portalCooldown = false;
                }
            }
            // Attacking a mob in front of the targeted block (wiki: Melee attack):
            // the held item's attack damage (hand: 1), knockback away from us.
            if (!dead && clicks.attackClick) {
                const glm::dvec3 eye = player.eyePosition(1.0);
                const glm::dvec3 look(mc::world::lookVector(player.yaw(), player.pitch()));
                const double reach = survival ? 3.0 : 5.0; // wiki: entity interaction range
                if (const auto mh = mc::Mobs::raycast(world, eye, look, reach);
                    mh && (!lastHit || mh->distance < lastHit->distance)) {
                    auto& m = world.chunk(mh->chunk)->mobs()[size_t(mh->index)];
                    const auto& stack = inventory.selectedStack();
                    const auto& held = mc::world::itemRegistry().item(stack.item);
                    // Weapon enchantments (wiki): Sharpness +0.5 per level +0.5, Smite and
                    // Bane of Arthropods +2.5 per level against their mobs.
                    using E = mc::world::Enchantment;
                    float dmg = stack.empty() ? 1.0f : held.attackDamage;
                    if (const int s = mc::world::enchantLevel(stack, E::Sharpness)) dmg += 0.5f * float(s) + 0.5f;
                    // Strength +3 and weakness -4 per level (wiki: Strength, Weakness).
                    dmg += 3.0f * float(vitals.effectLevel(mc::world::Effect::Strength));
                    dmg = std::max(0.0f, dmg - 4.0f * float(vitals.effectLevel(mc::world::Effect::Weakness)));
                    if (m.type == mc::world::MobType::Zombie || m.type == mc::world::MobType::Skeleton)
                        dmg += 2.5f * float(mc::world::enchantLevel(stack, E::Smite));
                    if (m.type == mc::world::MobType::Spider)
                        dmg += 2.5f * float(mc::world::enchantLevel(stack, E::BaneOfArthropods));
                    if (m.type == mc::world::MobType::EnderDragon) // (the head takes it all)
                        dmg = mc::Mobs::dragonDamage(m, dmg, eye + look * mh->distance);
                    const bool hit = m.hurtTime == 0 && m.deathTime == 0;
                    m.looting = static_cast<uint8_t>(mc::world::enchantLevel(stack, E::Looting));
                    mc::Mobs::attack(m, dmg, player.position());
                    if (hit) {
                        // Knockback: farther per level; Fire Aspect: alight 4 s per level.
                        if (const int kb = mc::world::enchantLevel(stack, E::Knockback)) {
                            glm::dvec2 d(m.pos.x - player.position().x, m.pos.z - player.position().z);
                            if (glm::length(d) > 1e-6) d = glm::normalize(d) * (0.5 * kb);
                            m.vel += glm::dvec3(d.x, 0.1, d.y);
                        }
                        if (const int fa = mc::world::enchantLevel(stack, E::FireAspect))
                            m.fireTicks = std::max<int16_t>(m.fireTicks, int16_t(80 * fa));
                        if (survival && held.tool != mc::world::ToolType::None) // swords wear 1 per hit, tools 2 (wiki)
                            inventory.setSlot(inventory.selected(),
                                              mc::wearItem(stack, held.tool == mc::world::ToolType::Sword ? 1 : 2, gameRng));
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
                const mc::world::BlockPos eyeBlock{int(std::floor(feet.x)), int(std::floor(feet.y + player.eyeHeight())),
                                                   int(std::floor(feet.z))};
                const bool eyesInWater = reg.blockOf(world.getBlock(eyeBlock)) == mc::world::blocks::Water;
                // Aqua Affinity: no slower mining under water (wiki).
                interaction.tickSurvival(world, player, lastHit, inventory, vitals, clicks,
                                         eyesInWater && !mc::world::enchantLevel(inventory.armor(0),
                                                                                 mc::world::Enchantment::AquaAffinity),
                                         gameRng, changedBlocks, drops);
                for (const auto& d : drops)
                    droppedItems.spawn(d.pos, d.stack, gameRng);
                if (interaction.takeChorusTeleport())
                    if (const auto to = mc::chorusTeleport(world, player.position(), gameRng)) {
                        player.setPosition(*to);
                        vitals.resetFall(); // (vanilla: the fall is reset by the teleport)
                    }
            } else {
                interaction.tickDrinking(inventory, vitals, clicks.use || clicks.useClick, false);
                interaction.tick(world, player, lastHit, inventory.placeState(), clicks, changedBlocks, &drops,
                                 mc::world::itemRegistry().item(inventory.selectedStack().item).tool ==
                                     mc::world::ToolType::Sword);
                for (const auto& d : drops)
                    droppedItems.spawn(d.pos, d.stack, gameRng);
            }
            frameEdits.insert(frameEdits.end(), changedBlocks.begin(), changedBlocks.end());
            droppedItems.tick(world, player.box(), !dead, inventory);
            // Game rules read the tick's own time, not the renderer's interpolated value.
            mc::Mobs::Context mobCtx{world, player, vitals, survival, dead, dayTime,
                                     float(mc::world::skyDarken(mc::world::celestialAngle(dayTime))), gameRng,
                                     droppedItems, dimension != Dimension::End,
                                     inventory.selectedStack().item, &frameEdits, &projectiles, &orbs};
            for (int piece = 0; piece < 4; ++piece) // piglins: any golden armor piece (wiki: Piglin)
                if (!inventory.armor(piece).empty() &&
                    mc::world::itemRegistry().item(inventory.armor(piece).item).id.starts_with("minecraft:golden_"))
                    mobCtx.wearsGold = true;
            // Scheduled block ticks, random ticks within the simulation distance, block
            // events (vanilla: before entities).
            {
                const mc::world::BlockPos at{int(std::floor(feet.x)), 0, int(std::floor(feet.z))};
                // Ticking chunks: within the simulation distance, never past what's loaded.
                blockUpdates.setRandomTicks(at.chunk(), std::min(mobs.simulationDistance(), opts->renderDistance),
                                            mc::world::BlockUpdates::kDefaultRandomTickSpeed);
                blockUpdates.setPlayer(feet);
                blockUpdates.setSkyDarken(dimension == Dimension::Overworld
                                              ? int(mc::world::skyDarken(mc::world::celestialAngle(dayTime)))
                                              : 0);
            }
            blockUpdates.tick();
            frameEdits.insert(frameEdits.end(), blockUpdates.changed().begin(), blockUpdates.changed().end());
            blockUpdates.changed().clear();
            frameRemesh.insert(frameRemesh.end(), blockUpdates.remeshOnly().begin(), blockUpdates.remeshOnly().end());
            frameSettling.insert(frameSettling.end(), blockUpdates.settling().begin(), blockUpdates.settling().end());
            blockUpdates.settling().clear();
            blockUpdates.remeshOnly().clear();
            for (const auto& d : blockUpdates.drops()) {
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
            // Sand/gravel that lost its support falls as an entity (M16).
            for (const auto& f : blockUpdates.fallingStarts())
                fallingBlocks.spawn(f.pos, f.state);
            blockUpdates.fallingStarts().clear();
            fallingBlocks.tick(world, droppedItems, gameRng, frameEdits);
            // Wear from this tick's hits (armor pieces; the shield that blocked).
            if (const int wear = vitals.takeArmorWear(); wear > 0 && survival) inventory.wearArmor(wear, gameRng);
            if (const int wear = vitals.takeShieldWear(); wear > 0 && survival) {
                static const mc::world::ItemId shieldItem = *mc::world::itemRegistry().find("shield");
                auto wearShield = [&](mc::world::ItemStack s) { return mc::wearItem(s, wear, gameRng); };
                if (inventory.selectedStack().item == shieldItem)
                    inventory.setSlot(inventory.selected(), wearShield(inventory.selectedStack()));
                else if (inventory.offhand().item == shieldItem)
                    inventory.setOffhand(wearShield(inventory.offhand()));
            }
            projectiles.tick(world, player, !dead ? &vitals : nullptr, inventory, survival, gameRng);
            // Ghast fireballs explode (wiki: Fireball - power 1, incendiary: fire on a
            // third of the open spots around it); blaze fireballs lit blocks.
            for (const glm::dvec3& at : projectiles.explosions()) {
                fireballBlast.explode(world, at, 1.0f, gameRng, droppedItems, frameEdits,
                                      {survival && !dead ? &player : nullptr, &vitals, true});
                const mc::world::BlockPos c{int(std::floor(at.x)), int(std::floor(at.y)), int(std::floor(at.z))};
                for (int dx = -2; dx <= 2; ++dx)
                    for (int dy = -2; dy <= 2; ++dy)
                        for (int dz = -2; dz <= 2; ++dz) {
                            const mc::world::BlockPos q{c.x + dx, c.y + dy, c.z + dz};
                            if (!world.isInHeight(q.y) || world.getBlock(q) != 0 || gameRng.nextInt(3) != 0 ||
                                !mc::world::BlockUpdates::fireCanStay(world, q))
                                continue;
                            world.updateBlock(q, mc::world::BlockUpdates::fireState(0));
                            frameEdits.push_back(q);
                        }
            }
            frameEdits.insert(frameEdits.end(), projectiles.edits().begin(), projectiles.edits().end());
            projectiles.edits().clear();
            // Ender pearls (M20.3): the player goes where one lands (5 damage), or
            // through the end gateway it went into.
            auto gatewayTravel = [&](const mc::world::BlockPos& g) {
                const auto* eg = dynamic_cast<const mc::world::EndGenerator*>(generatorPtr.get());
                if (!eg) return;
                if (const auto to = dragonFight.gatewayTarget(*eg, g)) {
                    player.setPosition(*to);
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
                    if (survival) vitals.attacked(5.0f);
                }
            }
            if (!dead && dimension == Dimension::End && mc::portals::touching(world, player.box(), mc::world::blocks::EndGateway)) {
                // Walking (or flying) into a gateway works too (vanilla: any entity).
                const glm::dvec3 f = player.position();
                for (int dy = 0; dy <= 2; ++dy)
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx) {
                            const mc::world::BlockPos b{int(std::floor(f.x)) + dx, int(std::floor(f.y)) + dy,
                                                        int(std::floor(f.z)) + dz};
                            if (reg.blockOf(world.getBlock(b)) == mc::world::blocks::EndGateway) {
                                gatewayTravel(b);
                                dy = dz = dx = 3; // (once)
                            }
                        }
            }
            for (const glm::dvec3& at : projectiles.eyeDrops()) {
                static const mc::world::ItemId eyeItem = *mc::world::itemRegistry().find("ender_eye");
                droppedItems.spawn(at, {eyeItem, 1}, gameRng);
            }
            // Experience: ores just mined, furnace output taken, orbs collected (M17.5).
            if (const int xp = interaction.takeExperience(); xp > 0) {
                const mc::world::BlockPos b = interaction.experienceAt();
                orbs.drop({b.x + 0.5, b.y + 0.5, b.z + 0.5}, xp, gameRng);
            }
            // Furnace output taken: its stored recipe uses become orbs at the player (vanilla).
            if (const int xp = mc::recipesExperience(container.takeRecipes(), gameRng); xp > 0)
                orbs.drop(player.position() + glm::dvec3(0.0, 0.5, 0.0), xp, gameRng);
            // Enchanting and anvils (M17.5): levels spent, a new seed after enchanting,
            // and the anvil's wear: 12% a use - anvil, chipped, damaged, gone (wiki).
            if (const int spent = container.takeLevelsSpent(); spent > 0) vitals.spendLevels(spent);
            if (container.takeEnchanted()) vitals.setEnchantSeed(gameRng.nextLong() & 0xFFFFFFFFull);
            if (container.takeAnvilUsed() && survival && gameRng.nextFloat() < 0.12f) {
                const auto st = world.getBlock(containerBlock);
                const auto b = reg.blockOf(st);
                const mc::world::BlockId next = b == mc::world::blocks::Anvil          ? mc::world::BlockId(mc::world::blocks::ChippedAnvil)
                                                : b == mc::world::blocks::ChippedAnvil ? mc::world::BlockId(mc::world::blocks::DamagedAnvil)
                                                                                       : mc::world::BlockId(0);
                if (b == mc::world::blocks::Anvil || b == mc::world::blocks::ChippedAnvil || b == mc::world::blocks::DamagedAnvil) {
                    const auto ns = next ? reg.with(reg.defaultState(next), "facing", *reg.value(st, "facing"))
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
            vitals.addExperience(orbs.tick(world, player.box(), !dead));
            mobs.tick(mobCtx);
            const auto* endGen = dynamic_cast<const mc::world::EndGenerator*>(generatorPtr.get());
            if (dimension == Dimension::End && endKind == "end2" && endGen)
                dragonFight.tick(world, *endGen, mobs, player.position(), orbs, gameRng, frameEdits);
            // Furnaces smelt in every loaded chunk (block entities tick, wiki).
            litChanges.clear();
            world.forEachTickingChunk([&](mc::world::Chunk& c) {
                for (auto& br : c.brewingStands()) // brewing stands brew (M19.4)
                    if (mc::tickBrewing(br.data)) c.markDirty();
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
                if (reg.blockOf(state) != mc::world::blocks::Furnace) continue;
                mc::world::Chunk* fc = world.chunk(p.chunk());
                const auto* f = fc ? fc->furnace(mc::world::blockToLocal(p.x), p.y, mc::world::blockToLocal(p.z))
                                   : nullptr;
                if (!f) continue;
                world.setBlock(p, reg.with(state, "lit", f->lit() ? "true" : "false").value_or(state));
                frameEdits.push_back(p); // relit, then re-meshed
            }
            renderer.tick();
            ++dayTime; // the daylight cycle advances one tick per tick
            ++gameTime;
            if (pearlCooldown > 0) --pearlCooldown;
            // Vanilla autosave: every 6000 ticks (5 minutes) of play.
            if (++sessionTicks % 6000 == 0) saveWorld(false);
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
            const mc::world::BlockPos ob{opts->openBlock[0], opts->openBlock[1], opts->openBlock[2]};
            const auto obBlock = mc::world::blockRegistry().blockOf(world.getBlock(ob));
            containerBlock = ob;
            if (obBlock == mc::world::blocks::EnchantingTable)
                container.openEnchanting(mc::countBookshelves(world, ob), vitals.enchantSeed());
            else if (obBlock == mc::world::blocks::Anvil || obBlock == mc::world::blocks::ChippedAnvil ||
                     obBlock == mc::world::blocks::DamagedAnvil)
                container.openAnvil();
            else if (obBlock == mc::world::blocks::BrewingStand) {
                mc::world::Chunk* bc = world.chunk(ob.chunk());
                container.openBrewing(
                    bc ? bc->brewing(mc::world::blockToLocal(ob.x), ob.y, mc::world::blockToLocal(ob.z)) : nullptr);
            } else
                openChestAt(ob);
        }
        if (openInventoryPending && gameTime > 0) {
            openInventoryPending = false;
            if (survival) container.open(mc::ui::ContainerScreen::Type::Inventory);
            else creative.open();
        }
        if (spawnPending) { // new player: stand on solid ground once it's generated
            if (const auto safe = settleSpawn(world, spawn)) {
                player.setPosition(*safe);
                spawnPending = false;
            }
        }
        mc::gfx::Camera camera;
        camera.position = player.eyePosition(clock.alpha);
        camera.yaw = player.yaw();
        camera.pitch = player.pitch();
        if (dimension == Dimension::Nether) { // fog of the Nether biome at the camera, eased in
            static glm::vec3 netherFog(0x33 / 255.0f, 0x08 / 255.0f, 0x08 / 255.0f);
            const mc::world::BlockPos cam{int(std::floor(camera.position.x)), int(std::floor(camera.position.y)),
                                          int(std::floor(camera.position.z))};
            if (const mc::world::Chunk* c = world.chunk(cam.chunk()); c && c->biomes()) {
                const uint32_t fog = mc::world::biomeInfo(c->biomes()->at(mc::world::blockToLocal(cam.x), cam.y,
                                                                          mc::world::blockToLocal(cam.z),
                                                                          world.height()))
                                         .fog;
                if (fog) {
                    const glm::vec3 target(float(fog >> 16) / 255.0f, float((fog >> 8) & 255) / 255.0f, float(fog & 255) / 255.0f);
                    // (3% per 1/60 s, whatever the frame rate)
                    netherFog = glm::mix(netherFog, target, 1.0f - std::pow(0.97f, float(frameSeconds * 60.0)));
                }
            }
            renderer.setNetherFog(netherFog);
        }
        renderer.setNightVision(vitals.effectLevel(mc::world::Effect::NightVision) > 0);
        renderer.setDayTime(dayTime, static_cast<float>(clock.alpha));
        if (loader) {
            const mc::world::ChunkPos center{
                mc::world::blockToChunk(static_cast<int32_t>(std::floor(camera.position.x))),
                mc::world::blockToChunk(static_cast<int32_t>(std::floor(camera.position.z)))};
            loader->setGameTime(gameTime);
            loader->update(center, loadedChunks, unloadedChunks);
        }
        // Lighting follows loading and edits; meshing follows lighting.
        lighting.update(loadedChunks, unloadedChunks, frameEdits, frameSettling, litChunks, relitSections,
                        editsReady);
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
        const auto hit = mc::world::raycastBlocks(
            world, camera.position, glm::dvec3(mc::world::lookVector(camera.yaw, camera.pitch)),
            survival ? mc::world::kSurvivalReach : mc::world::kCreativeReach);
        // Dropped items and the breaking crack.
        // Light colours for all 16x16 sky/block levels this frame (night changes them).
        std::array<glm::vec3, 256> lightTable;
        for (int sky = 0; sky < 16; ++sky)
            for (int blk = 0; blk < 16; ++blk)
                lightTable[size_t(sky * 16 + blk)] = mc::gfx::lightColor(sky, blk, renderer.skyDarken(),
                                                                         mc::world::dimensionInfo(dimension).ambientLight,
                                                                         dimension == Dimension::End);
        for (const auto& e : droppedItems.items()) {
            const glm::dvec3 p = glm::mix(e.prevPos, e.pos, clock.alpha);
            const float t = float(e.age) + float(clock.alpha);
            entities.addItem(e.stack, p, t / 20.0f + e.spinOffset, std::sin(t / 10.0f + e.spinOffset) * 0.1f + 0.1f,
                             lightTable[size_t(e.skyLight * 16 + e.blockLight)], camera.position);
        }
        for (const auto& pr : projectiles.items()) {
            const glm::dvec3 p = glm::mix(pr.prevPos, pr.pos, clock.alpha);
            const glm::vec3 light = lightTable[size_t(pr.skyLight * 16 + pr.blockLight)];
            static const mc::world::ItemId eggItem = *mc::world::itemRegistry().find("egg");
            static const mc::world::ItemId eyeItem = *mc::world::itemRegistry().find("ender_eye");
            static const mc::world::ItemId splashItem = *mc::world::itemRegistry().find("splash_potion");
            static const mc::world::ItemId fireItem = *mc::world::itemRegistry().find("fire_charge");
            static const mc::world::ItemId pearlItem = *mc::world::itemRegistry().find("ender_pearl");
            // (dragon fireballs too)
            if (pr.kind == mc::ProjectileKind::Arrow)
                entities.addArrow(p, pr.facing, light, camera.position);
            else {
                mc::world::ItemStack look{pr.kind == mc::ProjectileKind::EyeOfEnder     ? eyeItem
                                          : pr.kind == mc::ProjectileKind::SplashPotion ? splashItem
                                          : pr.kind == mc::ProjectileKind::Egg          ? eggItem
                                          : pr.kind == mc::ProjectileKind::EnderPearl   ? pearlItem
                                                                                        : fireItem,
                                          1};
                look.potion = pr.potion;
                entities.addItem(look, p - glm::dvec3(0, 0.1, 0), 0.0f, 0.0f, light, camera.position);
            }
        }
        for (const auto& c : projectiles.clouds()) // (M20.2) dragon's breath
            entities.addCloud(c.pos, c.radius, float(gameTime) + float(clock.alpha), camera.position);
        for (const auto& o : orbs.orbs())
            entities.addOrb(glm::mix(o.prevPos, o.pos, clock.alpha), o.value, float(o.age) + float(clock.alpha),
                            camera.position);
        for (const auto& f : fallingBlocks.blocks())
            entities.addBlock(f.state, glm::mix(f.prevPos, f.pos, clock.alpha),
                              lightTable[size_t(f.skyLight * 16 + f.blockLight)], camera.position);
        // Mobs: only chunks in view, and mobs within vanilla's entity render distance
        // (64 blocks x the hitbox's average edge; wiki: Options › Entity Distance).
        const mc::gfx::Frustum mobFrustum =
            mc::gfx::Frustum::fromMatrix(camera.viewProjectionAtOrigin(float(fbWidth) / float(fbHeight)));
        world.forEachTickingChunk([&](mc::world::Chunk& c) {
            if (c.mobs().empty()) return;
            const glm::vec3 cmin(glm::dvec3(c.pos().x * 16.0, c.height().minY, c.pos().z * 16.0) - camera.position);
            if (!mobFrustum.intersectsBox(cmin, cmin + glm::vec3(16.0f, float(c.height().height), 16.0f)))
                return;
            for (const auto& m : c.mobs()) {
                const glm::dvec3 p = glm::mix(m.prevPos, m.pos, clock.alpha);
                const auto& info = mc::world::mobInfo(m.type);
                const double maxDist = 64.0 * (info.width * 2.0 + info.height) / 3.0;
                const glm::dvec3 rel = p - camera.position;
                if (glm::dot(rel, rel) > maxDist * maxDist) continue;
                const glm::vec3 bmin(rel - glm::dvec3(info.width * 0.5, 0.0, info.width * 0.5));
                const float reachOut = m.type == mc::world::MobType::EnderDragon ? 8.0f : 0.0f; // (its tail and wings)
                if (!mobFrustum.intersectsBox(bmin - glm::vec3(reachOut),
                                              bmin + glm::vec3(float(info.width), float(info.height), float(info.width)) +
                                                  glm::vec3(reachOut)))
                    continue;
                const mc::world::BlockPos b{int(std::floor(p.x)), int(std::floor(p.y + 0.5)), int(std::floor(p.z))};
                int sky = 15, blk = 0;
                if (const auto* lc = world.chunk(b.chunk()); lc && lc->lit()) {
                    sky = lc->skyLight(mc::world::blockToLocal(b.x), b.y, mc::world::blockToLocal(b.z));
                    blk = lc->blockLight(mc::world::blockToLocal(b.x), b.y, mc::world::blockToLocal(b.z));
                }
                const float a = float(clock.alpha);
                entities.addMob(m, p, m.prevYaw + (m.yaw - m.prevYaw) * a, m.prevHeadYaw + (m.headYaw - m.prevHeadYaw) * a,
                                m.prevPitch + (m.pitch - m.prevPitch) * a, lightTable[size_t(sky * 16 + blk)],
                                camera.position);
                if (m.type == mc::world::MobType::EnderDragon && m.hasBeam && m.deathTime == 0)
                    entities.addBeam(m.beam, p + glm::dvec3(0.0, 0.75, 0.0), camera.position);
            }
        });
        if (survival && interaction.breakingBlock())
            entities.setCrack(*interaction.breakingBlock(), static_cast<int>(interaction.breakProgress() * 10.0f));
        else
            entities.clearCrack();
        entities.draw(camera, float(fbWidth) / float(fbHeight));
        lastHit = hit;
        overlay.draw(camera, fbWidth, fbHeight,
                     hit ? std::optional<mc::world::BlockPos>(hit->block) : std::nullopt);

        // HUD: inventory, chat, F3 - one batched GUI draw.
        {
            const int scale = mc::gfx::GuiRenderer::guiScale(fbWidth, fbHeight);
            const int guiW = fbWidth / scale, guiH = fbHeight / scale;
            auto& batch = gui.batch();
            mc::ui::drawHotbar(batch, inventory, itemIcons, renderer.models(), guiW, guiH);
            if (survival)
                mc::ui::drawVitals(batch, vitals.health(), vitals.food(), guiW, guiH, vitals.air(), inventory.armorPoints());
            if (survival) mc::ui::drawExperience(batch, vitals.xpLevel(), vitals.xpProgress(), guiW, guiH);
            if (mobs.bossHealth() >= 0.0f) // (M20.2)
                mc::ui::drawBossBar(batch, "Ender Dragon", mobs.bossHealth() / 200.0f, mc::gfx::rgba(236, 72, 200), guiW);
            if (dead) mc::ui::drawDeathScreen(batch, guiW, guiH);
            if (sleepTicks > 0) // falling asleep: the screen darkens (vanilla)
                batch.fill(0, 0, float(guiW), float(guiH),
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
                container.setFurnace(fc ? fc->furnace(mc::world::blockToLocal(containerBlock.x), containerBlock.y,
                                                      mc::world::blockToLocal(containerBlock.z))
                                        : nullptr);
            }
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Brewing) {
                mc::world::Chunk* bc = world.chunk(containerBlock.chunk());
                container.setBrewing(bc ? bc->brewing(mc::world::blockToLocal(containerBlock.x), containerBlock.y,
                                                      mc::world::blockToLocal(containerBlock.z))
                                        : nullptr);
            }
            // Chests may have moved in memory or gone during this frame's ticks: re-point.
            if (container.isOpen() && container.type() == mc::ui::ContainerScreen::Type::Chest) pointChests();
            if (container.isOpen())
                container.draw(batch, itemIcons, renderer.models(), inventory, guiW, guiH,
                               [&] { double x = 0, y = 0; window.cursorPos(x, y); return x / scale; }(),
                               [&] { double x = 0, y = 0; window.cursorPos(x, y); return y / scale; }());
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
                    d.biome = mc::world::biomeInfo(c->biomes()->at(mc::world::blockToLocal(feet.x), feet.y,
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
            gui.draw(fbWidth, fbHeight);
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
    for (const auto& d : screenDrops) // didn't fit: drop at the player (saved later... lost: see deviations)
        droppedItems.spawn(player.position(), d, gameRng);
    saveWorld(true);
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
