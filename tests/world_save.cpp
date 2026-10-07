// Saves: Anvil region files and the 1.21 chunk NBT (wiki: Region file format,
// Chunk format).
#include "gameplay/Furnace.h"
#include "gameplay/Inventory.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/ChunkStorage.h"
#include "world/FlatGenerator.h"
#include "world/RegionFile.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace mc::world;
namespace fs = std::filesystem;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

// A fresh, empty directory under the system temp dir (removed on destruction).
struct TempDir {
    fs::path path;
    explicit TempDir(const char* name) : path(fs::temp_directory_path() / name) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void fillTestChunk(Chunk& c) {
    FlatGenerator::fromPreset(FlatGenerator::kClassicFlat)->generate(c);
    const auto& r = blockRegistry();
    c.set(1, 10, 2, S(blocks::Stone));
    c.set(3, -64, 4, S(blocks::Glowstone));
    c.set(15, 319, 15, *r.with(S(blocks::OakLog), "axis", "z"));
    for (int i = 0; i < 40; ++i) // many states in one section: wide palette
        c.set(i % 16, 100, i / 16, r.defaultState(BlockId(1 + i % (r.blockCount() - 1))));
}

bool sameBlocks(const Chunk& a, const Chunk& b) {
    if (!(a.height() == b.height())) return false;
    for (int s = 0; s < a.sectionCount(); ++s)
        for (int i = 0; i < Section::kVolume; ++i)
            if (a.section(s).getIndex(i) != b.section(s).getIndex(i)) return false;
    return true;
}

} // namespace

TEST_CASE("region file: write, rewrite larger, read back, survive reopening") {
    TempDir dir("mc_test_region");
    const auto path = dir.path / "r.0.0.mca";
    std::vector<uint8_t> small(100, 7), big(20000);
    for (size_t i = 0; i < big.size(); ++i)
        big[i] = static_cast<uint8_t>(i * 2654435761u >> 24); // poorly compressible
    {
        RegionFile r;
        REQUIRE(r.open(path));
        CHECK_FALSE(r.has(5));
        REQUIRE(r.write(5, small, 1));
        REQUIRE(r.write(6, small, 1));
        REQUIRE(r.write(5, big, 2)); // grows: moves past chunk 6
        CHECK(r.read(5) == big);
        CHECK(r.read(6) == small);
    }
    CHECK(fs::file_size(path) % RegionFile::kSector == 0); // whole sectors (vanilla)
    RegionFile again;
    REQUIRE(again.open(path));
    CHECK(again.read(5) == big);
    CHECK(again.read(6) == small);
    CHECK_FALSE(again.read(7).has_value());
    CHECK(RegionFile::index(-1, -1) == 31 * 32 + 31);
}

TEST_CASE("chunk NBT round-trips every block state, at negative positions too") {
    Chunk c({-3, 7});
    fillTestChunk(c);
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    CHECK(nbt.integer("DataVersion") == kDataVersion);
    CHECK(nbt.integer("yPos") == -4);
    REQUIRE(nbt.string("Status")); // 1.21.11 (lower case only from 26.4)
    CHECK(*nbt.string("Status") == "minecraft:full");
    CHECK_FALSE(nbt.find("status"));
    REQUIRE(nbt.list("sections"));
    CHECK(nbt.list("sections")->items.size() == 24);
    // Through bytes, as on disk.
    const auto back = mc::nbt::read(mc::nbt::write(nbt));
    REQUIRE(back.has_value());
    Chunk d({-3, 7});
    int unknown = 0;
    REQUIRE(chunkFromNbt(*back, d, &unknown));
    CHECK(unknown == 0);
    CHECK(sameBlocks(c, d));
    Chunk wrongPlace({0, 0});
    CHECK_FALSE(chunkFromNbt(*back, wrongPlace));
}

TEST_CASE("chunk NBT uses vanilla palette entries and packing") {
    Chunk c({0, 0});
    c.set(0, -64, 0, *blockRegistry().with(S(blocks::OakLog), "axis", "x"));
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    const auto* sec = nbt.list("sections")->items[0].get<mc::nbt::Compound>();
    REQUIRE(sec);
    CHECK(sec->integer("Y") == -4);
    const auto* bs = sec->compound("block_states");
    const auto& pal = bs->list("palette")->items;
    REQUIRE(pal.size() == 2); // log first (index 0 is the first block), then air
    const auto* log = pal[0].get<mc::nbt::Compound>();
    CHECK(*log->string("Name") == "minecraft:oak_log");
    CHECK(*log->compound("Properties")->string("axis") == "x");
    CHECK(*pal[1].get<mc::nbt::Compound>()->string("Name") == "minecraft:air");
    // 2 entries -> 4 bits, 16 per long, 256 longs; cell 0 = palette index 0.
    REQUIRE(bs->longArray("data"));
    CHECK(bs->longArray("data")->size() == 256);
    CHECK(((*bs->longArray("data"))[0] & 0xF) == 0);
    CHECK((((*bs->longArray("data"))[0] >> 4) & 0xF) == 1);
    // A uniform section has a one-entry palette and no data.
    const auto* top = nbt.list("sections")->items[23].get<mc::nbt::Compound>();
    CHECK(top->compound("block_states")->list("palette")->items.size() == 1);
    CHECK(top->compound("block_states")->longArray("data") == nullptr);
}

TEST_CASE("chunk storage: saves on its thread, loads back, serves queued saves") {
    TempDir dir("mc_test_storage");
    Chunk c({40, -70}); // region (1, -3)
    fillTestChunk(c);
    {
        ChunkStorage storage(dir.path);
        Chunk missing({0, 0});
        CHECK_FALSE(storage.load(missing));
        storage.save(ChunkSnapshot::of(c));
        Chunk early({40, -70}); // may still be queued: served from the snapshot
        REQUIRE(storage.load(early));
        CHECK(sameBlocks(c, early));
        CHECK_FALSE(early.dirty());
        storage.flush();
        CHECK(storage.queued() == 0);
    }
    CHECK(fs::exists(dir.path / "region" / "r.1.-3.mca"));
    ChunkStorage reopened(dir.path);
    Chunk d({40, -70});
    REQUIRE(reopened.load(d));
    CHECK(sameBlocks(c, d));
}

TEST_CASE("chunks are dirty after edits, not after a clean load") {
    Chunk c({0, 0});
    c.clearDirty();
    CHECK_FALSE(c.dirty());
    c.set(1, 1, 1, S(blocks::Stone));
    CHECK(c.dirty());
    c.reset({1, 1}, kOverworldHeight);
    CHECK_FALSE(c.dirty());
}

#include "world/Enchantments.h"
#include "world/LevelData.h"
#include "gameplay/Inventory.h"

TEST_CASE("level.dat round-trips the world settings, time, spawn and player") {
    TempDir dir("mc_test_level");
    LevelData l;
    l.name = "Test";
    l.seed = 0xFEEDFACECAFEBEEFull;
    l.flat = true;
    l.dayTime = 3 * 24000 + 13000;
    l.gameTime = 99999;
    l.pos[0] = -12.5;
    l.pos[1] = 70.25;
    l.pos[2] = 8;
    l.yaw = 135;
    l.pitch = -20;
    l.flying = true;
    l.inventory.push_back({0, "minecraft:stone", "", 12, 0});
    l.inventory.push_back({4, "minecraft:oak_log", "minecraft:oak_log[axis=x]", 1, 0});
    l.inventory.push_back({20, "minecraft:iron_pickaxe", "", 1, 37});
    l.selectedSlot = 4;
    l.spawn[0] = -7;
    l.spawn[1] = 70;
    l.spawn[2] = 12;
    REQUIRE(l.save(dir.path));
    REQUIRE(l.save(dir.path)); // second save keeps a level.dat_old backup
    CHECK(fs::exists(dir.path / "level.dat_old"));
    const auto back = LevelData::load(dir.path);
    REQUIRE(back.has_value());
    CHECK(back->name == "Test");
    CHECK(back->seed == l.seed);
    CHECK(back->flat);
    CHECK(back->dayTime == l.dayTime);
    CHECK(back->gameTime == 99999);
    CHECK(back->pos[0] == -12.5);
    CHECK(back->pos[1] == 70.25);
    CHECK(back->yaw == 135.0f);
    CHECK(back->pitch == -20.0f);
    CHECK(back->flying);
    REQUIRE(back->inventory.size() == 3);
    CHECK(back->inventory[0].id == "minecraft:stone");
    CHECK(back->inventory[0].count == 12);
    CHECK(back->inventory[1].slot == 4);
    CHECK(back->inventory[1].state == "minecraft:oak_log[axis=x]");
    CHECK(back->inventory[2].slot == 20);
    CHECK(back->inventory[2].damage == 37);
    CHECK(back->selectedSlot == 4);
    CHECK(back->spawn[0] == -7);
    CHECK(back->spawn[2] == 12);
    CHECK_FALSE(LevelData::load(dir.path / "nope").has_value());
}

#include "world/ChunkLoader.h"
#include "world/TerrainGenerator.h"

#include <chrono>
#include <thread>

TEST_CASE("chunk loader: an edited chunk saves when it unloads and comes back edited") {
    TempDir dir("mc_test_loader_save");
    World world;
    const TerrainGenerator gen(42);
    ChunkStorage storage(dir.path);
    ChunkLoader loader(world, gen, 1, &storage);
    loader.setRenderDistance(2);
    std::vector<ChunkPos> loaded, unloaded;
    auto settle = [&](ChunkPos centre) {
        for (int i = 0; i < 5000; ++i) {
            loader.update(centre, loaded, unloaded);
            if (loader.pending() == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    settle({0, 0});
    REQUIRE(world.chunk({0, 0}));
    CHECK(world.chunk({0, 0})->dirty()); // freshly generated chunks are saved too (vanilla)
    world.setBlock({3, 200, 3}, S(blocks::Glowstone));
    settle({100, 100}); // far away: (0,0) unloads and is saved
    REQUIRE_FALSE(world.chunk({0, 0}));
    settle({0, 0}); // back: loaded from storage (or its queued snapshot)
    REQUIRE(world.chunk({0, 0}));
    CHECK(world.getBlock({3, 200, 3}) == S(blocks::Glowstone));
    CHECK_FALSE(world.chunk({0, 0})->dirty());
}


#include "core/Compression.h"
#include "core/FileLock.h"

#include <fstream>
#include <iterator>

namespace {

mc::nbt::Compound readLevelRoot(const fs::path& file) {
    std::ifstream f(file, std::ios::binary);
    const std::vector<uint8_t> gz((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return *mc::nbt::read(*mc::gzipDecompress(gz));
}

} // namespace

TEST_CASE("level.dat: hotbar states use the block_state item component; types as vanilla") {
    TempDir dir("mc_test_level_types");
    LevelData l;
    l.inventory.push_back({2, "minecraft:oak_log", "minecraft:oak_log[axis=x]", 1, 0});
    REQUIRE(l.save(dir.path));
    const auto root = readLevelRoot(dir.path / "level.dat");
    const auto* data = root.compound("Data");
    REQUIRE(data);
    CHECK(data->find("version")->type() == mc::nbt::TagType::Int);
    CHECK(data->integer("version") == 19133);
    CHECK(data->find("DayTime")->type() == mc::nbt::TagType::Long);
    const auto& item = *data->compound("Player")->list("Inventory")->items[0].get<mc::nbt::Compound>();
    CHECK(item.find("Slot")->type() == mc::nbt::TagType::Byte);
    CHECK(item.find("count")->type() == mc::nbt::TagType::Int);
    CHECK(*item.string("id") == "minecraft:oak_log");
    CHECK(*item.compound("components")->compound("minecraft:block_state")->string("axis") == "x");
}

TEST_CASE("level.dat: a missing level.dat falls back to level.dat_old") {
    // Regression: a lost level.dat made the world look new (wrong seed/generator).
    TempDir dir("mc_test_level_fallback");
    LevelData l;
    l.seed = 123;
    REQUIRE(l.save(dir.path));
    REQUIRE(l.save(dir.path)); // creates level.dat_old
    fs::remove(dir.path / "level.dat");
    const auto back = LevelData::load(dir.path);
    REQUIRE(back.has_value());
    CHECK(back->seed == 123);
}

TEST_CASE("chunk storage: saving again while the chunk is being written keeps the newest") {
    // Regression: a save arriving during the write of an older snapshot was dropped.
    TempDir dir("mc_test_resave");
    {
        ChunkStorage storage(dir.path);
        Chunk c({2, 3});
        for (int i = 1; i <= 40; ++i) { // many saves racing the IO thread
            c.set(0, 0, 0, i % 2 ? S(blocks::Stone) : S(blocks::Dirt));
            c.set(1, 0, 0, blockRegistry().defaultState(BlockId(1 + i % 10)));
            storage.save(ChunkSnapshot::of(c));
        }
        storage.flush();
    }
    ChunkStorage reopened(dir.path);
    Chunk d({2, 3});
    REQUIRE(reopened.load(d));
    CHECK(d.get(0, 0, 0) == S(blocks::Dirt)); // the 40th save
    CHECK(d.get(1, 0, 0) == blockRegistry().defaultState(BlockId(1)));
}

TEST_CASE("loading never creates region files") {
    TempDir dir("mc_test_nocreate");
    ChunkStorage storage(dir.path);
    Chunk c({100, 100});
    CHECK_FALSE(storage.load(c));
    CHECK_FALSE(fs::exists(dir.path / "region"));
}

TEST_CASE("region file: malformed headers are ignored, never trusted") {
    TempDir dir("mc_test_badregion");
    const auto path = dir.path / "r.0.0.mca";
    std::vector<uint8_t> file(3 * 4096, 0);
    auto loc = [&](int index, uint32_t offset, uint8_t count) {
        file[size_t(index) * 4 + 0] = uint8_t(offset >> 16);
        file[size_t(index) * 4 + 1] = uint8_t(offset >> 8);
        file[size_t(index) * 4 + 2] = uint8_t(offset);
        file[size_t(index) * 4 + 3] = count;
    };
    loc(0, 2, 0);  // count 0 (would allow any length)
    loc(1, 9, 1);  // beyond the file
    loc(2, 2, 1);  // valid sector...
    loc(3, 2, 1);  // ...shared with chunk 2: dropped
    file[2 * 4096 + 3] = 200; // chunk 2: length 200 > what is there (truncated payload)
    file[2 * 4096 + 4] = 2;
    {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(file.data()), std::streamsize(file.size()));
    }
    RegionFile r;
    REQUIRE(r.open(path));
    CHECK_FALSE(r.has(0));
    CHECK_FALSE(r.has(1));
    CHECK(r.has(2));
    CHECK_FALSE(r.has(3));
    CHECK_FALSE(r.read(2).has_value()); // garbage zlib: rejected, no crash
}

TEST_CASE("NBT lists longer than the remaining input are rejected") {
    // TAG_Compound "" { TAG_List "l" of TAG_Byte, count 2^30 }
    const std::vector<uint8_t> bad = {10, 0, 0, 9, 0, 1, 'l', 1, 0x40, 0, 0, 0, 0};
    CHECK_FALSE(mc::nbt::read(bad).has_value());
}

TEST_CASE("chunk NBT: 17+ states pack 5 bits, 12 per long; dark sections keep SkyLight") {
    Chunk c({0, 0});
    for (int i = 0; i < 17; ++i)
        c.set(i % 16, 0, i / 16, blockRegistry().defaultState(BlockId(1 + i)));
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
    light.fill(std::make_shared<const SectionLight>()); // all dark
    c.setLight(light);
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c, 777));
    CHECK(nbt.integer("LastUpdate") == 777);
    const auto* sec = nbt.list("sections")->items[4].get<mc::nbt::Compound>(); // y 0
    const auto& data = *sec->compound("block_states")->longArray("data");
    CHECK(sec->compound("block_states")->list("palette")->items.size() == 18); // + air
    CHECK(data.size() == 342); // ceil(4096 / 12)
    for (int64_t v : data)
        CHECK((static_cast<uint64_t>(v) >> 60) == 0); // 4 unused high bits
    REQUIRE(sec->byteArray("SkyLight"));
    CHECK(sec->byteArray("SkyLight")->size() == 2048);
    CHECK(sec->byteArray("BlockLight") == nullptr);
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(nbt, d));
    CHECK(sameBlocks(c, d));
}

TEST_CASE("chunk NBT: unknown properties are ignored, bad sections skipped") {
    mc::nbt::Compound entry;
    entry.put("Name", std::string("minecraft:oak_log"));
    mc::nbt::Compound props;
    props.put("axis", std::string("x"));
    props.put("future_property", std::string("yes"));
    entry.put("Properties", props);
    mc::nbt::Compound bs;
    bs.put("palette", mc::nbt::listOf(mc::nbt::TagType::Compound, {entry}));
    mc::nbt::Compound sec;
    sec.put("Y", int8_t{0});
    sec.put("block_states", bs);
    mc::nbt::Compound outOfRange = sec;
    outOfRange.put("Y", int8_t{100});
    mc::nbt::Compound root;
    root.put("xPos", int32_t{0});
    root.put("zPos", int32_t{0});
    root.put("sections", mc::nbt::listOf(mc::nbt::TagType::Compound, {sec, outOfRange}));
    Chunk c({0, 0});
    int unknown = 0;
    REQUIRE(chunkFromNbt(root, c, &unknown));
    CHECK(unknown == 0);
    CHECK(c.get(0, 0, 0) == *blockRegistry().with(S(blocks::OakLog), "axis", "x"));
}

TEST_CASE("session lock: a second holder is refused until the first lets go") {
    TempDir dir("mc_test_lock");
    {
        mc::FileLock a;
        REQUIRE(a.acquire(dir.path / "session.lock"));
        mc::FileLock b;
        CHECK_FALSE(b.acquire(dir.path / "session.lock"));
    }
    mc::FileLock c;
    CHECK(c.acquire(dir.path / "session.lock"));
}

TEST_CASE("chunk NBT: biomes round-trip per 4x4x4 cell with a string palette") {
    Chunk c({0, 0});
    auto b = std::make_shared<ChunkBiomes>();
    b->cells[size_t(ChunkBiomes::index(4, 1, 2, 3))] = Biome::Desert;
    b->cells[size_t(ChunkBiomes::index(4, 0, 0, 0))] = Biome::Taiga;
    b->cells[size_t(ChunkBiomes::index(23, 3, 3, 3))] = Biome::FrozenPeaks;
    c.setBiomes(b);
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    const auto* sec4 = nbt.list("sections")->items[4].get<mc::nbt::Compound>();
    const auto& pal = sec4->compound("biomes")->list("palette")->items;
    REQUIRE(pal.size() == 3); // taiga (cell 0), plains, desert - first-appearance order
    CHECK(*pal[0].get<std::string>() == "minecraft:taiga");
    CHECK(sec4->compound("biomes")->longArray("data")->size() == 2); // 2 bits: 32 per long
    const auto* sec0 = nbt.list("sections")->items[0].get<mc::nbt::Compound>();
    CHECK(sec0->compound("biomes")->longArray("data") == nullptr); // one biome: no data
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(nbt, d));
    CHECK(d.biomes()->cells == b->cells);
    CHECK(d.biomes()->at(5, 9, 14) == Biome::Desert); // section 4 (y 0..15), cell (1, 2, 3)
}

TEST_CASE("level.dat keeps the generator kind; flat stays flat") {
    TempDir dir("mc_test_level_generator");
    LevelData l;
    l.generator = "terrain";
    REQUIRE(l.save(dir.path));
    CHECK(LevelData::load(dir.path)->generator == "terrain");
    l.flat = true;
    REQUIRE(l.save(dir.path));
    CHECK(LevelData::load(dir.path)->flat);
    CHECK(LevelData().generator == "overworld"); // new worlds
}

TEST_CASE("level.dat keeps game mode, health and hunger") {
    TempDir dir("mc_test_level_survival");
    LevelData l;
    l.survival = true;
    l.health = 7.5f;
    l.food = 12;
    l.saturation = 1.5f;
    l.exhaustion = 2.25f;
    l.air = 120;
    l.fire = 85;
    REQUIRE(l.save(dir.path));
    const auto back = LevelData::load(dir.path);
    REQUIRE(back.has_value());
    CHECK(back->survival);
    CHECK(back->health == 7.5f);
    CHECK(back->food == 12);
    CHECK(back->saturation == 1.5f);
    CHECK(back->exhaustion == 2.25f);
    CHECK(back->air == 120); // Player.Air
    CHECK(back->fire == 85); // Player.Fire (burning survives a reload)
}

TEST_CASE("furnaces save as block entities with their contents and timers") {
    Chunk c({-2, 3});
    World w;
    c.set(4, 70, 9, S(blocks::Furnace));
    FurnaceData& f = c.addFurnace(4, 70, 9);
    f.fuel.state = 0;
    f.input = {*itemRegistry().find("raw_iron"), 5};
    f.fuel = {*itemRegistry().find("coal"), 2};
    f.output = {*itemRegistry().find("iron_ingot"), 3};
    f.burnLeft = 800;
    f.cookTime = 120;
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    REQUIRE(nbt.list("block_entities"));
    const auto* e = nbt.list("block_entities")->items[0].get<mc::nbt::Compound>();
    CHECK(*e->string("id") == "minecraft:furnace");
    CHECK(e->integer("x") == -32 + 4);
    CHECK(e->integer("z") == 48 + 9);
    REQUIRE(e->find("lit_time_remaining")); // 1.21.4+ names
    CHECK(e->find("lit_time_remaining")->type() == mc::nbt::TagType::Short);
    CHECK(e->integer("cooking_time_spent") == 120);
    CHECK_FALSE(e->find("BurnTime"));
    Chunk d({-2, 3});
    REQUIRE(chunkFromNbt(*mc::nbt::read(mc::nbt::write(nbt)), d));
    const FurnaceData* back = d.furnace(4, 70, 9);
    REQUIRE(back);
    CHECK(back->input.count == 5);
    CHECK(back->fuel.count == 2);
    CHECK(itemRegistry().item(back->output.item).id == "minecraft:iron_ingot");
    CHECK(back->burnLeft == 800);
    CHECK(back->cookTime == 120);
}

TEST_CASE("placing and breaking a furnace block creates and removes its block entity") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({1, 64, 1}, S(blocks::Furnace));
    CHECK(w.chunk({0, 0})->furnace(1, 64, 1) != nullptr);
    const auto lit = *blockRegistry().with(S(blocks::Furnace), "lit", "true");
    w.setBlock({1, 64, 1}, lit); // a state change keeps it
    CHECK(w.chunk({0, 0})->furnace(1, 64, 1) != nullptr);
    CHECK(blockRegistry().lightEmission(lit) == 13);
    w.setBlock({1, 64, 1}, 0);
    CHECK(w.chunk({0, 0})->furnace(1, 64, 1) == nullptr);
}

TEST_CASE("furnace slots keep exact block states; non-furnace entries are dropped on load") {
    Chunk c({0, 0});
    c.set(1, 64, 1, S(blocks::Furnace));
    FurnaceData& f = c.addFurnace(1, 64, 1);
    f.fuel = mc::Inventory::blockStack(*blockRegistry().with(S(blocks::OakLog), "axis", "x"), 3);
    const BlockStateId expected = f.fuel.state; // (f may move when the list grows)
    c.addFurnace(5, 64, 5); // no furnace block there (a stale entry)
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(*mc::nbt::read(mc::nbt::write(chunkToNbt(ChunkSnapshot::of(c)))), d));
    REQUIRE(d.furnace(1, 64, 1));
    CHECK(d.furnace(1, 64, 1)->fuel.state == expected);
    CHECK(d.furnace(5, 64, 5) == nullptr);
}

TEST_CASE("chunk storage: a reload from the pending save keeps furnaces and biomes") {
    TempDir dir("mc_test_pending_entities");
    ChunkStorage storage(dir.path);
    Chunk c({3, 3});
    c.set(2, 70, 2, S(blocks::Furnace));
    c.addFurnace(2, 70, 2).input = {*itemRegistry().find("raw_iron"), 4};
    auto b = std::make_shared<ChunkBiomes>();
    b->cells[0] = Biome::Desert;
    c.setBiomes(b);
    storage.save(ChunkSnapshot::of(c));
    Chunk d({3, 3});
    REQUIRE(storage.load(d)); // pending or written: either way complete
    REQUIRE(d.furnace(2, 70, 2));
    CHECK(d.furnace(2, 70, 2)->input.count == 4);
    CHECK(d.biomes()->cells[0] == Biome::Desert);
}

TEST_CASE("a reloaded furnace keeps smelting where it left off") {
    // Regression: the input being cooked wasn't restored, so the first tick reset the
    // progress after every reload.
    Chunk c({0, 0});
    c.set(1, 64, 1, S(blocks::Furnace));
    FurnaceData& f = c.addFurnace(1, 64, 1);
    f.input = {*itemRegistry().find("raw_iron"), 3};
    f.fuel = {*itemRegistry().find("coal"), 2};
    f.burnLeft = 800;
    f.cookTime = 120;
    f.cooking = f.input.item;
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(*mc::nbt::read(mc::nbt::write(chunkToNbt(ChunkSnapshot::of(c)))), d));
    FurnaceData* back = d.furnace(1, 64, 1);
    REQUIRE(back);
    mc::tickFurnace(*back);
    CHECK(back->cookTime == 121);
}

TEST_CASE("chunk storage: mobs save to entities/, and a chunk's last mob leaving clears them") {
    TempDir dir("mc_test_entities_storage");
    Chunk c({5, -3});
    MobData cow;
    cow.type = MobType::Cow;
    cow.pos = {5 * 16 + 4.5, 70.0, -3 * 16 + 2.5};
    cow.health = 7.0f;
    c.mobs().push_back(cow);
    {
        ChunkStorage storage(dir.path);
        storage.save(ChunkSnapshot::of(c));
        Chunk early({5, -3}); // served from the queued save
        REQUIRE(storage.load(early));
        CHECK(early.mobs().size() == 1);
        storage.flush();
    }
    CHECK(fs::exists(dir.path / "entities" / "r.0.-1.mca"));
    {
        ChunkStorage storage(dir.path);
        Chunk d({5, -3});
        REQUIRE(storage.load(d));
        REQUIRE(d.mobs().size() == 1);
        CHECK(d.mobs()[0].health == 7.0f);
        d.mobs().clear(); // the cow walked away
        storage.save(ChunkSnapshot::of(d));
        storage.flush();
    }
    ChunkStorage storage(dir.path);
    Chunk e({5, -3});
    REQUIRE(storage.load(e));
    CHECK(e.mobs().empty());
}

TEST_CASE("entities from corrupt files: bad positions skipped, motion and health clamped") {
    using mc::nbt::Compound;
    using mc::nbt::List;
    using mc::nbt::TagType;
    auto entity = [](double x, double motion, double health) {
        Compound e;
        e.put("id", std::string("minecraft:cow"));
        std::vector<mc::nbt::Tag> pos{mc::nbt::Tag(x), mc::nbt::Tag(64.0), mc::nbt::Tag(1.0)};
        e.put("Pos", mc::nbt::listOf(TagType::Double, std::move(pos)));
        std::vector<mc::nbt::Tag> mot{mc::nbt::Tag(motion), mc::nbt::Tag(0.0), mc::nbt::Tag(0.0)};
        e.put("Motion", mc::nbt::listOf(TagType::Double, std::move(mot)));
        e.put("Health", static_cast<float>(health));
        return e;
    };
    std::vector<mc::nbt::Tag> list;
    list.emplace_back(entity(std::nan(""), 0.0, 10.0));
    list.emplace_back(entity(1e9, 0.0, 10.0));
    list.emplace_back(entity(1.0, 1e9, 1e9));
    Compound root;
    root.put("Entities", mc::nbt::listOf(TagType::Compound, std::move(list)));
    Chunk c({0, 0});
    entitiesFromNbt(root, c);
    REQUIRE(c.mobs().size() == 1);
    CHECK(c.mobs()[0].vel.x == 10.0);
    CHECK(c.mobs()[0].health == mobInfo(MobType::Cow).maxHealth);
}

TEST_CASE("level.dat keeps the player's dimension and known portals; old files default to the Overworld") {
    TempDir dir("mc_test_level_dimension");
    LevelData l;
    l.dimension = "minecraft:the_nether";
    l.portals.push_back({"minecraft:overworld", 10, 64, -3});
    l.portals.push_back({"minecraft:the_nether", 1, 70, 0});
    REQUIRE(l.save(dir.path));
    const auto back = LevelData::load(dir.path);
    REQUIRE(back.has_value());
    CHECK(back->dimension == "minecraft:the_nether");
    REQUIRE(back->portals.size() == 2);
    CHECK(back->portals[0].dimension == "minecraft:overworld");
    CHECK(back->portals[0].z == -3);
    CHECK(back->portals[1].y == 70);
    CHECK(LevelData{}.dimension == "minecraft:overworld");
}

TEST_CASE("Nether/End chunks save like vanilla: yPos 0, 16 sections Y 0..15; Overworld yPos -4") {
    Chunk n({2, 3}, kNetherHeight);
    n.set(4, 0, 4, S(blocks::Bedrock));
    n.set(4, 255, 4, S(blocks::Netherrack));
    n.set(4, 256, 4, S(blocks::Netherrack)); // above the Nether's height: ignored
    n.set(4, -1, 4, S(blocks::Netherrack));  // below: ignored
    const auto nbt = chunkToNbt(ChunkSnapshot::of(n));
    CHECK(nbt.integer("yPos") == 0);
    const auto* sections = nbt.list("sections");
    REQUIRE(sections);
    REQUIRE(sections->items.size() == 16);
    CHECK(sections->items.front().get<mc::nbt::Compound>()->integer("Y") == 0);
    CHECK(sections->items.back().get<mc::nbt::Compound>()->integer("Y") == 15);
    Chunk back({2, 3}, kNetherHeight);
    REQUIRE(chunkFromNbt(nbt, back));
    CHECK(blockRegistry().blockOf(back.get(4, 0, 4)) == blocks::Bedrock);
    CHECK(blockRegistry().blockOf(back.get(4, 255, 4)) == blocks::Netherrack);
    Chunk o({2, 3});
    CHECK(chunkToNbt(ChunkSnapshot::of(o)).integer("yPos") == -4);
}

TEST_CASE("a Nether chunk saved in our old 384-block layout loads into the Nether's height") {
    // Before the per-dimension heights, Nether chunks were written with 24 sections
    // (Y -4..19). Sections inside 0..15 are read; the rest is ignored.
    Chunk old({0, 0}); // overworld height, as written then
    old.set(1, 10, 1, S(blocks::Netherrack));
    old.set(1, 300, 1, S(blocks::Stone)); // outside the Nether: dropped
    Chunk n({0, 0}, kNetherHeight);
    REQUIRE(chunkFromNbt(chunkToNbt(ChunkSnapshot::of(old)), n));
    CHECK(blockRegistry().blockOf(n.get(1, 10, 1)) == blocks::Netherrack);
    CHECK(n.get(1, 300, 1) == 0);
}

TEST_CASE("chunk reuse across dimension heights; Nether chunks round-trip through storage") {
    Chunk c({1, 1});
    c.set(3, -60, 3, S(blocks::Stone));
    c.reset({2, 2}, kNetherHeight);
    CHECK(c.sectionCount() == 16);
    CHECK(c.get(3, -60, 3) == 0);
    c.set(3, 200, 3, S(blocks::Netherrack));
    CHECK(blockRegistry().blockOf(c.get(3, 200, 3)) == blocks::Netherrack);
    c.reset({1, 1}, kOverworldHeight);
    CHECK(c.sectionCount() == 24);
    c.set(3, -60, 3, S(blocks::Stone));
    CHECK(c.get(3, -60, 3) == S(blocks::Stone));

    TempDir dir("mc_test_nether_storage");
    Chunk n({-3, 4}, kNetherHeight);
    n.set(5, 0, 5, S(blocks::Bedrock));
    n.set(5, 250, 5, S(blocks::Glowstone));
    {
        ChunkStorage storage(dir.path);
        storage.save(ChunkSnapshot::of(n));
        Chunk early({-3, 4}, kNetherHeight); // from the pending snapshot
        REQUIRE(storage.load(early));
        CHECK(sameBlocks(n, early));
    }
    ChunkStorage storage(dir.path);
    Chunk back({-3, 4}, kNetherHeight);
    REQUIRE(storage.load(back));
    CHECK(sameBlocks(n, back));
    auto b = std::make_shared<ChunkBiomes>();
    b->cells[size_t(ChunkBiomes::index(15, 0, 2, 0))] = Biome::NetherWastes; // top section, cell y 248..251
    CHECK(b->at(1, 250, 1, kNetherHeight) == Biome::NetherWastes);
}

TEST_CASE("1.21.11 chunk extras: heightmaps packed 9 bits x 7 per long, empty lists vanilla writes") {
    Chunk c({0, 0});
    c.set(0, -64, 0, S(blocks::Stone));        // column 0: height 1
    c.set(1, 70, 0, S(blocks::Water));         // column 1: fluid - motion blocking, not ocean floor
    c.set(1, 60, 0, S(blocks::Sand));
    c.set(2, 80, 0, S(blocks::OakLeaves));      // column 2: leaves
    c.set(2, 75, 0, S(blocks::Dirt));
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    const auto* maps = nbt.compound("Heightmaps");
    REQUIRE(maps);
    auto value = [&](const char* name, int column) {
        const auto* t = maps->find(name);
        const auto& longs = *t->get<std::vector<int64_t>>();
        REQUIRE(longs.size() == 37);
        return int((uint64_t(longs[size_t(column / 7)]) >> ((column % 7) * 9)) & 511);
    };
    CHECK(value("WORLD_SURFACE", 0) == 1);
    CHECK(value("MOTION_BLOCKING", 1) == 70 + 64 + 1);
    CHECK(value("OCEAN_FLOOR", 1) == 60 + 64 + 1);
    CHECK(value("MOTION_BLOCKING", 2) == 80 + 64 + 1);
    CHECK(value("MOTION_BLOCKING_NO_LEAVES", 2) == 75 + 64 + 1);
    CHECK(value("WORLD_SURFACE", 5) == 0);
    {
        Chunk top({0, 0});
        top.set(3, 319, 0, S(blocks::Stone)); // the highest value: 384
        const auto tn = chunkToNbt(ChunkSnapshot::of(top));
        const auto& longs = *tn.compound("Heightmaps")->find("WORLD_SURFACE")->get<std::vector<int64_t>>();
        CHECK(int((uint64_t(longs[0]) >> (3 * 9)) & 511) == 384);
        Chunk nether({0, 0}, kNetherHeight);
        nether.set(0, 255, 0, S(blocks::Netherrack));
        const auto nn = chunkToNbt(ChunkSnapshot::of(nether));
        const auto& nl = *nn.compound("Heightmaps")->find("WORLD_SURFACE")->get<std::vector<int64_t>>();
        CHECK(nl.size() == 37); // still 9 bits for a 256-high world
        CHECK(int(uint64_t(nl[0]) & 511) == 256);
    }
    REQUIRE(nbt.list("PostProcessing"));
    CHECK(nbt.list("PostProcessing")->items.size() == 24);
    CHECK(nbt.compound("structures"));
    CHECK(nbt.integer("DataVersion") == 4671);
}

TEST_CASE("1.21.11 level.dat: spawn compound, version 1.21.11, game rules, dimensions; older fields still read") {
    TempDir dir("mc_test_level_12111");
    LevelData l;
    l.spawn[0] = 12;
    l.spawn[1] = 70;
    l.spawn[2] = -5;
    REQUIRE(l.save(dir.path));
    std::ifstream f(dir.path / "level.dat", std::ios::binary);
    const std::vector<uint8_t> gz((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const auto root = mc::nbt::read(*mc::gzipDecompress(gz));
    REQUIRE(root);
    const auto* data = root->compound("Data");
    REQUIRE(data);
    CHECK(*data->compound("Version")->string("Name") == "1.21.11");
    REQUIRE(data->compound("spawn"));
    CHECK(*data->compound("spawn")->string("dimension") == "minecraft:overworld");
    CHECK_FALSE(data->find("SpawnX"));
    CHECK(*data->compound("GameRules")->string("minecraft:keep_inventory") == "false");
    CHECK(data->compound("WorldGenSettings")->compound("dimensions")->compound("minecraft:the_nether"));
    const auto back = LevelData::load(dir.path);
    REQUIRE(back);
    CHECK(back->spawn[0] == 12);
    CHECK(back->spawn[2] == -5);
    // A level.dat from before 1.21.9: SpawnX/Y/Z, no spawn compound.
    mc::nbt::Compound oldData;
    oldData.put("SpawnX", int32_t{-40});
    oldData.put("SpawnY", int32_t{80});
    oldData.put("SpawnZ", int32_t{7});
    mc::nbt::Compound oldRoot;
    oldRoot.put("Data", std::move(oldData));
    TempDir oldDir("mc_test_level_old_spawn");
    const auto bytes = mc::gzipCompress(mc::nbt::write(oldRoot));
    std::ofstream(oldDir.path / "level.dat", std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    const auto old = LevelData::load(oldDir.path);
    REQUIRE(old);
    CHECK(old->spawn[0] == -40);
    CHECK(old->spawn[1] == 80);
    CHECK(old->spawn[2] == 7);
}

TEST_CASE("older saves still load: BurnTime/CookTime, FallDistance, no status tag") {
    Chunk c({0, 0});
    c.set(1, 64, 1, S(blocks::Furnace));
    c.addFurnace(1, 64, 1).cookTime = 50;
    auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    // Rewrite as a 1.21.1-era save.
    auto& e = const_cast<mc::nbt::Compound&>(*nbt.list("block_entities")->items[0].get<mc::nbt::Compound>());
    std::erase_if(e.entries, [](const auto& kv) {
        return kv.name == "cooking_time_spent" || kv.name == "lit_time_remaining" || kv.name == "lit_total_time";
    });
    e.put("CookTime", int16_t{77});
    e.put("BurnTime", int16_t{300});
    std::erase_if(nbt.entries, [](const auto& kv) { return kv.name == "Status"; }); // (not needed to load)
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(nbt, d));
    CHECK(d.furnace(1, 64, 1)->cookTime == 77);
    CHECK(d.furnace(1, 64, 1)->burnLeft == 300);
    CHECK(d.furnace(1, 64, 1)->burnDuration == 300);
    mc::nbt::Compound mob;
    mob.put("id", std::string("minecraft:cow"));
    mob.put("Pos", mc::nbt::listOf(mc::nbt::TagType::Double, {1.0, 64.0, 1.0}));
    mob.put("FallDistance", 2.5f);
    mc::nbt::Compound ents;
    ents.put("Entities", mc::nbt::listOf(mc::nbt::TagType::Compound, {mob}));
    entitiesFromNbt(ents, d);
    REQUIRE(d.mobs().size() == 1);
    CHECK(d.mobs()[0].fallDistance == 2.5f);
}

TEST_CASE("chests: placed chests get storage that saves as Items (Slot 0..26) and loads back") {
    World w;
    w.createChunk({0, 0});
    const auto chest = blockRegistry().defaultState(blocks::Chest);
    w.setBlock({3, 70, 4}, chest);
    Chunk& c = *w.chunk({0, 0});
    ChestData* d = c.chest(3, 70, 4);
    REQUIRE(d);
    d->items[5] = {*itemRegistry().find("diamond"), 3};
    d->items[26] = {*itemRegistry().find("stick"), 64};
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    Chunk e({0, 0});
    REQUIRE(chunkFromNbt(nbt, e));
    const ChestData* back = e.chest(3, 70, 4);
    REQUIRE(back);
    CHECK(back->items[5].count == 3);
    CHECK(back->items[26].item == *itemRegistry().find("stick"));
    w.setBlock({3, 70, 4}, 0); // the block goes: its storage too
    CHECK(c.chest(3, 70, 4) == nullptr);
}

TEST_CASE("level.dat: worn armor and the offhand save in 1.21.5+'s equipment compound") {
    TempDir dir("mc_test_level_equipment");
    LevelData l;
    l.inventory.push_back({103, "minecraft:diamond_helmet", "", 1, 7});
    l.inventory.push_back({100, "minecraft:iron_boots", "", 1, 0});
    l.inventory.push_back({150, "minecraft:shield", "", 1, 0});
    l.inventory.push_back({0, "minecraft:stone", "", 3, 0});
    REQUIRE(l.save(dir.path));
    const auto back = LevelData::load(dir.path);
    REQUIRE(back);
    int found = 0;
    for (const auto& it : back->inventory) {
        if (it.slot == 103) found += it.id == "minecraft:diamond_helmet" && it.damage == 7;
        if (it.slot == 100) found += it.id == "minecraft:iron_boots";
        if (it.slot == 150) found += it.id == "minecraft:shield";
        if (it.slot == 0) found += it.count == 3;
    }
    CHECK(found == 4);
}

TEST_CASE("level.dat keeps the bed respawn point (1.21.5+ respawn compound)") {
    TempDir dir("mc_test_level_respawn");
    LevelData l;
    l.hasRespawn = true;
    l.respawn[0] = -12, l.respawn[1] = 70, l.respawn[2] = 33;
    REQUIRE(l.save(dir.path));
    const auto back = LevelData::load(dir.path);
    REQUIRE(back);
    CHECK(back->hasRespawn);
    CHECK(back->respawn[2] == 33);
}

TEST_CASE("enchanted items save as 1.21.5+ minecraft:enchantments and load back (also the old 'levels' form)") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({1, 70, 1}, blockRegistry().defaultState(blocks::Chest));
    Chunk& c = *w.chunk({0, 0});
    ItemStack sword{*itemRegistry().find("diamond_sword"), 1, 3};
    setEnchantment(sword, Enchantment::Sharpness, 5);
    setEnchantment(sword, Enchantment::Looting, 2);
    sword.repairCost = 3;
    c.chest(1, 70, 1)->items[0] = sword;
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    Chunk e({0, 0});
    REQUIRE(chunkFromNbt(nbt, e));
    const ItemStack& back = e.chest(1, 70, 1)->items[0];
    CHECK(enchantLevel(back, Enchantment::Sharpness) == 5);
    CHECK(enchantLevel(back, Enchantment::Looting) == 2);
    CHECK(back.repairCost == 3);
    CHECK(back.damage == 3);
}

TEST_CASE("items keep up to 8 enchantments through a save; furnaces keep their stored experience") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({1, 70, 1}, blockRegistry().defaultState(blocks::Chest));
    w.setBlock({3, 70, 1}, blockRegistry().defaultState(blocks::Furnace));
    Chunk& c = *w.chunk({0, 0});
    ItemStack helmet{*itemRegistry().find("diamond_helmet"), 1};
    for (Enchantment e : {Enchantment::Protection, Enchantment::Unbreaking, Enchantment::Respiration,
                          Enchantment::AquaAffinity, Enchantment::Thorns})
        REQUIRE(setEnchantment(helmet, e, 1));
    c.chest(1, 70, 1)->items[0] = helmet;
    c.furnace(3, 70, 1)->experience = 2.5f;
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c));
    Chunk e({0, 0});
    REQUIRE(chunkFromNbt(nbt, e));
    CHECK(enchantLevel(e.chest(1, 70, 1)->items[0], Enchantment::Thorns) == 1); // the 5th survives
    CHECK(e.furnace(3, 70, 1)->experience == doctest::Approx(2.5f));
}
