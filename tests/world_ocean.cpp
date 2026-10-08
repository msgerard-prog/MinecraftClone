// Oceans (M25.1): the "overworld4" generator's ocean biomes, floors, icebergs and
// flooded caves; waterlogged blocks.
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/OverworldGenerator.h"
#include "world/World.h"

#include <doctest/doctest.h>

#include <optional>
#include <ostream>
#include <string>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
constexpr int kSeaLevel = OverworldGenerator::kSeaLevel;

// The first chunk (spiralling out from the origin, centres every 4 chunks) whose centre
// column has one of these biomes.
std::optional<ChunkPos> findChunk(const OverworldGenerator& gen, std::initializer_list<Biome> want, int reach = 160) {
    for (int ring = 0; ring <= reach; ring += 4)
        for (int cz = -ring; cz <= ring; cz += 4)
            for (int cx = -ring; cx <= ring; cx += 4) {
                if (std::max(std::abs(cx), std::abs(cz)) != ring) continue;
                const Biome b = gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8));
                for (Biome w : want)
                    if (b == w) return ChunkPos{cx, cz};
            }
    return std::nullopt;
}

int countBlock(const Chunk& c, BlockId b) {
    int n = 0;
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) n += R().blockOf(c.get(x, y, z)) == b;
    return n;
}

} // namespace

TEST_CASE("waterlogged blocks: kelp and seagrass always, corals and pickles by their property; they hold water") {
    CHECK(R().waterlogged(R().defaultState(blocks::Kelp)));
    CHECK(R().waterlogged(R().defaultState(blocks::Seagrass)));
    const BlockStateId pickle = R().defaultState(blocks::SeaPickle);
    CHECK(R().waterlogged(pickle));
    CHECK_FALSE(R().waterlogged(R().set(pickle, properties::waterlogged, 1)));
    CHECK(R().lightEmission(R().set(pickle, properties::pickles, 3)) == 15); // 4 pickles in water
    CHECK(R().lightEmission(R().set(pickle, properties::waterlogged, 1)) == 0);
    CHECK(R().lightOpacity(R().defaultState(blocks::Kelp)) == 1); // (like water)
    CHECK(R().findBlock("dead_brain_coral_fan").has_value());
    CHECK(leftAfterBreaking(R().defaultState(blocks::Kelp)) == R().defaultState(blocks::Water));
    CHECK(leftAfterBreaking(R().defaultState(blocks::Stone)) == 0);
    CHECK(BlockUpdates::fluidAmount(R().defaultState(blocks::Seagrass)) == 8); // a source for its neighbours
}

TEST_CASE("overworld4: deep lukewarm/cold/frozen oceans; seagrass and kelp on ocean floors, always under water") {
    const OverworldGenerator gen(42);
    CHECK(gen.kind() == "overworld6"); // (the newest keeps overworld4's oceans)
    const auto deep = findChunk(gen, {Biome::DeepColdOcean, Biome::DeepLukewarmOcean, Biome::DeepFrozenOcean});
    REQUIRE(deep.has_value());
    const auto ocean = findChunk(gen, {Biome::Ocean, Biome::ColdOcean, Biome::LukewarmOcean, Biome::DeepOcean,
                                       Biome::DeepColdOcean, Biome::DeepLukewarmOcean});
    REQUIRE(ocean.has_value());
    Chunk c(*ocean);
    gen.generate(c);
    MESSAGE("kelp ocean chunk " << ocean->x << ", " << ocean->z);
    CHECK(countBlock(c, blocks::Seagrass) + countBlock(c, blocks::TallSeagrass) > 10);
    int kelpTips = 0;
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x)
            for (int y = 0; y < kSeaLevel; ++y) {
                const BlockStateId s = c.get(x, y, z);
                const BlockId b = R().blockOf(s);
                if (b != blocks::Kelp && b != blocks::Seagrass && b != blocks::KelpPlant) continue;
                const BlockId below = R().blockOf(c.get(x, y - 1, z));
                CHECK((R().collides(c.get(x, y - 1, z)) || below == blocks::KelpPlant)); // standing on something
                if (b == blocks::Kelp) {
                    ++kelpTips;
                    CHECK(R().blockOf(c.get(x, y + 1, z)) == blocks::Water); // the tip stays under water
                }
            }
    CHECK(kelpTips > 0);
}

TEST_CASE("overworld4: warm oceans grow coral reefs and sea pickles; frozen oceans carry icebergs") {
    const OverworldGenerator gen(42);
    const auto warm = findChunk(gen, {Biome::WarmOcean}, 400);
    REQUIRE(warm.has_value());
    Chunk w(*warm);
    gen.generate(w);
    MESSAGE("warm ocean chunk " << warm->x << ", " << warm->z);
    int coral = 0;
    for (const char* kind : kCoralKinds) {
        coral += countBlock(w, *R().findBlock(std::string(kind) + "_coral_block"));
        coral += countBlock(w, *R().findBlock(std::string(kind) + "_coral_fan"));
    }
    CHECK(coral > 5);
    const auto frozen = findChunk(gen, {Biome::FrozenOcean, Biome::DeepFrozenOcean}, 400);
    REQUIRE(frozen.has_value());
    MESSAGE("frozen ocean chunk " << frozen->x << ", " << frozen->z);
    int iceAbove = 0;
    for (int dz = -3; dz <= 3; ++dz) // (icebergs start in 1 of 8 frozen chunks: look around)
        for (int dx = -3; dx <= 3; ++dx) {
            Chunk f({frozen->x + dx, frozen->z + dz});
            gen.generate(f);
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    for (int y = kSeaLevel; y < kSeaLevel + 14; ++y) {
                        const BlockId b = R().blockOf(f.get(x, y, z));
                        iceAbove += b == blocks::PackedIce || b == blocks::SnowBlock;
                    }
        }
    CHECK(iceAbove > 0);
}

TEST_CASE("overworld4: flooded caves - no water inside a chunk hangs over or beside cave air below the sea") {
    const OverworldGenerator gen(42);
    const auto ocean = findChunk(gen, {Biome::Ocean, Biome::DeepOcean, Biome::ColdOcean});
    REQUIRE(ocean.has_value());
    int caveWater = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            Chunk c({ocean->x + dx, ocean->z + dz});
            gen.generate(c);
            const BlockStateId water = R().defaultState(blocks::Water);
            for (int y = OverworldGenerator::kLavaLevel; y < kSeaLevel; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        if (c.get(x, y, z) != water) continue;
                        if (y < 40) ++caveWater;
                        // (inside the chunk: its edges meet neighbours with their own barrier)
                        auto air = [&](int ax, int ay, int az) {
                            return ax >= 0 && ax < 16 && az >= 0 && az < 16 && c.get(ax, ay, az) == 0;
                        };
                        const bool leak = air(x, y - 1, z) || air(x - 1, y, z) || air(x + 1, y, z) ||
                                          air(x, y, z - 1) || air(x, y, z + 1);
                        CHECK_FALSE(leak);
                        if (leak) return;
                    }
        }
    CHECK(caveWater > 0); // (deep water: flooded caves under the sea floor)
}

TEST_CASE("overworld4 is deterministic; older generators keep their output (pinned)") {
    const OverworldGenerator a(42, 4), b(42, 4);
    Chunk c1({5, -3}), c2({5, -3});
    a.generate(c1);
    b.generate(c2);
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); y += 3)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) REQUIRE(c1.get(x, y, z) == c2.get(x, y, z));
    // A kelp-ocean chunk, pinned (re-pinned while M25 builds overworld4, frozen at v0.25.0).
    Chunk o({0, 0});
    a.generate(o);
    uint64_t h = 1469598103934665603ull;
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                for (const char ch : R().toString(o.get(x, y, z))) { // (names: state ids move as blocks are added)
                    h ^= uint8_t(ch);
                    h *= 1099511628211ull;
                }
            }
    CHECK(h == 3424206281726334893ull);
}

namespace {

// A stone floor (top y 63) under water up to y 70 over 3x3 chunks.
struct Sea {
    World world;
    BlockUpdates updates{world};
    int64_t time = 0;
    explicit Sea(bool water = true) {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 63, z, R().defaultState(blocks::Stone));
                        if (water)
                            for (int y = 64; y <= 70; ++y) c.set(x, y, z, R().defaultState(blocks::Water));
                    }
            }
    }
    void put(BlockPos p, BlockStateId s) {
        updates.setTime(time);
        world.updateBlock(p, s);
    }
    void tick(int n) {
        for (int i = 0; i < n; ++i) {
            ++time;
            updates.setTime(time);
            updates.tick();
        }
    }
    BlockId block(BlockPos p) const { return R().blockOf(world.getBlock(p)); }
};

} // namespace

TEST_CASE("kelp grows into the water above, its old tip becoming a stem; a stem without its tip is the tip again") {
    Sea s;
    s.updates.setRandomTicks({0, 0}, 1, 1000); // (often; not 4096: the random repeats its low bits every 4096)
    s.put({0, 64, 0}, R().defaultState(blocks::Kelp));
    s.tick(200);
    CHECK(s.block({0, 64, 0}) == blocks::KelpPlant);
    CHECK((s.block({0, 65, 0}) == blocks::KelpPlant || s.block({0, 65, 0}) == blocks::Kelp));
    int top = 64;
    while (s.block({0, top + 1, 0}) == blocks::Kelp || s.block({0, top + 1, 0}) == blocks::KelpPlant) ++top;
    CHECK(top <= 70); // never out of the water
    CHECK(s.block({0, top, 0}) == blocks::Kelp);
    // Breaking the middle: the part above pops, the stem below becomes the tip, water stays.
    s.updates.setRandomTicks({0, 0}, 1, 0);
    s.put({0, 65, 0}, leftAfterBreaking(s.world.getBlock({0, 65, 0})));
    s.tick(2);
    CHECK(s.block({0, 65, 0}) == blocks::Water);
    CHECK(s.block({0, 64, 0}) == blocks::Kelp);
    if (top > 65) CHECK(s.block({0, 66, 0}) == blocks::Water);
}

TEST_CASE("living coral dies out of water; in water it lives") {
    Sea dry(false);
    const BlockStateId block = R().defaultState(*R().findBlock("tube_coral_block"));
    dry.put({0, 64, 0}, block);
    dry.tick(120);
    CHECK(dry.block({0, 64, 0}) == *R().findBlock("dead_tube_coral_block"));
    Sea wet;
    wet.put({0, 64, 0}, block);
    wet.put({1, 64, 0}, R().defaultState(*R().findBlock("fire_coral_fan"))); // (waterlogged)
    wet.tick(120);
    CHECK(wet.block({0, 64, 0}) == *R().findBlock("tube_coral_block"));
    CHECK(wet.block({1, 64, 0}) == *R().findBlock("fire_coral_fan"));
    const BlockStateId dryFan = R().set(R().defaultState(*R().findBlock("fire_coral_fan")), properties::waterlogged, 1);
    dry.put({3, 64, 0}, dryFan);
    dry.tick(120);
    CHECK(dry.block({3, 64, 0}) == *R().findBlock("dead_fire_coral_fan"));
}

TEST_CASE("a waterlogged block's water flows out like a source; water beside it stays a source") {
    Sea s(false);
    s.put({0, 64, 0}, R().defaultState(blocks::Seagrass));
    s.put({5, 64, 0}, R().defaultState(blocks::Stone)); // (a neighbour change wakes it)
    s.put({1, 64, 0}, R().defaultState(blocks::Stone));
    s.put({1, 64, 0}, 0);
    s.tick(40);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    CHECK(R().get(s.world.getBlock({1, 64, 0}), properties::level) == 1); // flowing from it
    CHECK(s.block({0, 64, 0}) == blocks::Seagrass);
    // Seagrass without its floor pops and leaves its water.
    Sea t;
    t.put({2, 64, 2}, R().defaultState(blocks::Seagrass));
    t.put({2, 63, 2}, R().defaultState(blocks::Water));
    t.tick(2);
    CHECK(t.block({2, 64, 2}) == blocks::Water);
    REQUIRE_FALSE(t.updates.drops().empty());
}

#include "world/StructurePlacement.h"

TEST_CASE("overworld4: shipwrecks and ocean ruins on the sea floor with their chests; buried treasure under beaches (M25.4)") {
    const OverworldGenerator gen(42);
    auto findStart = [&](const RandomSpread& spread, bool beachToo) -> std::optional<ChunkPos> {
        for (int rz = -60; rz <= 60; ++rz)
            for (int rx = -60; rx <= 60; ++rx) {
                const ChunkPos c{rx, rz};
                if (!isSpreadCandidate(42, spread, c)) continue;
                const Biome b = gen.biomeAt(gen.column(rx * 16 + 8, rz * 16 + 8));
                const bool ocean = b == Biome::Ocean || b == Biome::DeepOcean || b == Biome::ColdOcean ||
                                   b == Biome::LukewarmOcean || b == Biome::WarmOcean || b == Biome::DeepColdOcean ||
                                   b == Biome::DeepLukewarmOcean || b == Biome::FrozenOcean || b == Biome::DeepFrozenOcean;
                if (ocean || (beachToo && b == Biome::Beach)) return c;
            }
        return std::nullopt;
    };
    const auto ship = findStart(kShipwrecks, true);
    REQUIRE(ship.has_value());
    int chests = 0, planks = 0, filled = 0;
    const BlockId sprucePlanks = *R().findBlock("spruce_planks");
    for (int dz = 0; dz <= 1; ++dz)
        for (int dx = 0; dx <= 1; ++dx) {
            Chunk c({ship->x + dx, ship->z + dz});
            gen.generate(c);
            chests += int(c.chests().size());
            for (const auto& ch : c.chests())
                for (const ItemStack& s : ch.data.items) filled += !s.empty();
            planks += countBlock(c, sprucePlanks);
        }
    MESSAGE("shipwreck at chunk " << ship->x << ", " << ship->z);
    CHECK(planks > 40);
    CHECK(chests >= 2);
    CHECK(filled > 3);
    const auto ruin = findStart(kOceanRuins, false);
    REQUIRE(ruin.has_value());
    int ruinChests = 0;
    for (int dz = 0; dz <= 1; ++dz)
        for (int dx = 0; dx <= 1; ++dx) {
            Chunk c({ruin->x + dx, ruin->z + dz});
            gen.generate(c);
            ruinChests += int(c.chests().size());
        }
    MESSAGE("ocean ruin at chunk " << ruin->x << ", " << ruin->z);
    CHECK(ruinChests >= 1);
    // Buried treasure: a chest with a heart of the sea in a beach chunk (1 in 100).
    bool heart = false;
    for (int rz = -80; rz <= 80 && !heart; ++rz)
        for (int rx = -80; rx <= 80 && !heart; ++rx) {
            if (gen.biomeAt(gen.column(rx * 16 + 9, rz * 16 + 9)) != Biome::Beach) continue;
            Chunk c({rx, rz});
            gen.generate(c);
            for (const auto& ch : c.chests())
                for (const ItemStack& s : ch.data.items)
                    heart = heart || s.item == *itemRegistry().find("heart_of_the_sea");
            if (heart) MESSAGE("buried treasure in chunk " << rx << ", " << rz);
        }
    CHECK(heart);
}

TEST_CASE("overworld4: ocean monuments - prismarine, gold blocks inside, elder guardians (M25.5)") {
    const OverworldGenerator gen(42);
    std::optional<ChunkPos> found;
    for (int rz = -80; rz <= 80 && !found; ++rz)
        for (int rx = -80; rx <= 80 && !found; ++rx) {
            if (!isSpreadCandidate(42, kMonuments, {rx, rz})) continue;
            const Biome b = gen.biomeAt(gen.column(rx * 16 + 8, rz * 16 + 8));
            if (b == Biome::DeepOcean || b == Biome::DeepColdOcean || b == Biome::DeepLukewarmOcean ||
                b == Biome::DeepFrozenOcean)
                found = ChunkPos{rx, rz};
        }
    REQUIRE(found.has_value());
    MESSAGE("monument at chunk " << found->x << ", " << found->z);
    int gold = 0, bricks = 0, elders = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            Chunk c({found->x + dx, found->z + dz});
            gen.generate(c);
            gold += countBlock(c, blocks::GoldBlock);
            bricks += countBlock(c, *R().findBlock("prismarine_bricks"));
            for (const MobData& m : c.mobs()) elders += m.type == MobType::ElderGuardian;
        }
    CHECK(gold == 8);
    CHECK(bricks > 500);
    CHECK(elders == 3);
}

TEST_CASE("overworld5 (M26.3b): bee nests with 2-3 bees on meadow and plains trees; berry bushes in taigas; pinned") {
    const OverworldGenerator gen(42, 5); // (frozen as of v0.26.0)
    CHECK(gen.kind() == "overworld5");
    // Around a meadow or plains: nests hang on trunk sides and hold their bees. Around a
    // taiga: berry bushes.
    int nests = 0, bushes = 0;
    const auto meadow = findChunk(gen, {Biome::Meadow, Biome::FlowerForest, Biome::CherryGrove});
    const auto taiga = findChunk(gen, {Biome::Taiga, Biome::SnowyTaiga, Biome::OldGrowthSpruceTaiga});
    REQUIRE(meadow.has_value());
    REQUIRE(taiga.has_value());
    MESSAGE("nest search near " << meadow->x << ", " << meadow->z << "; bushes near " << taiga->x << ", " << taiga->z);
    for (const ChunkPos centre : {*meadow, *taiga})
        for (int cz = centre.z - 5; cz <= centre.z + 5 && (centre == *meadow ? nests == 0 : bushes == 0); ++cz)
            for (int cx = centre.x - 5; cx <= centre.x + 5; ++cx) {
                Chunk c({cx, cz});
                gen.generate(c);
                for (const auto& h : c.beehives()) {
                    ++nests;
                    CHECK(R().blockOf(c.get(h.x, h.y, h.z)) == blocks::BeeNest);
                    CHECK(h.data.count >= 2);
                    CHECK(h.data.count <= 3);
                }
                for (int y = 60; y < 200; ++y)
                    for (int z = 0; z < 16; ++z)
                        for (int x = 0; x < 16; ++x) bushes += R().blockOf(c.get(x, y, z)) == blocks::SweetBerryBush;
            }
    CHECK(nests > 0);
    CHECK(bushes > 0);
    // A land chunk with the new features about, pinned (re-pinned while M26 builds it).
    Chunk o(*meadow);
    gen.generate(o);
    uint64_t h = 1469598103934665603ull;
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                for (const char ch : R().toString(o.get(x, y, z))) {
                    h ^= uint8_t(ch);
                    h *= 1099511628211ull;
                }
    MESSAGE("overworld5 hash " << h);
    CHECK(h == 804509575149335546ull);
}
