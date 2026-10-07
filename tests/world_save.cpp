// Saves: Anvil region files and the 1.21 chunk NBT (wiki: Region file format,
// Chunk format).
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
    for (int s = 0; s < kSectionsPerChunk; ++s)
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
    REQUIRE(nbt.string("Status"));
    CHECK(*nbt.string("Status") == "minecraft:full");
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
    c.reset({1, 1});
    CHECK_FALSE(c.dirty());
}

#include "world/LevelData.h"

TEST_CASE("level.dat round-trips the world settings, time and player") {
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
    l.hotbar[0] = "minecraft:stone";
    l.hotbar[4] = "minecraft:oak_log[axis=x]";
    l.selectedSlot = 4;
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
    CHECK(back->hotbar[0] == "minecraft:stone");
    CHECK(back->hotbar[4] == "minecraft:oak_log[axis=x]");
    CHECK(back->hotbar[1].empty());
    CHECK(back->selectedSlot == 4);
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
    CHECK_FALSE(world.chunk({0, 0})->dirty()); // freshly generated: nothing to save
    world.setBlock({3, 200, 3}, S(blocks::Glowstone));
    REQUIRE(world.chunk({0, 0})->dirty());
    settle({100, 100}); // far away: (0,0) unloads and is saved
    REQUIRE_FALSE(world.chunk({0, 0}));
    settle({0, 0}); // back: loaded from storage (or its queued snapshot)
    REQUIRE(world.chunk({0, 0}));
    CHECK(world.getBlock({3, 200, 3}) == S(blocks::Glowstone));
    CHECK_FALSE(world.chunk({0, 0})->dirty());
}
