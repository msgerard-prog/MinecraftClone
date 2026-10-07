// Weather (M22.1; wiki: Weather, Lightning, Snow, Ice, Farmland, Fire).
#include "gameplay/Commands.h"
#include "gameplay/Mobs.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/DayTime.h"
#include "world/LevelData.h"
#include "world/Weather.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

// 3x3 chunks of one biome with a stone floor at y 63.
struct Scene {
    World world;
    BlockUpdates updates{world};
    Weather weather;
    int64_t time = 0;
    explicit Scene(Biome biome = Biome::Plains) {
        auto b = std::make_shared<ChunkBiomes>();
        b->cells.fill(biome);
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                c.setBiomes(b);
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, S(blocks::Stone));
            }
        updates.setRandomTicks({0, 0}, 1, BlockUpdates::kDefaultRandomTickSpeed);
        updates.setWeather(&weather);
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

TEST_CASE("weather: rain comes and goes on its timer and fades in by 0.01 a tick") {
    Weather w;
    w.rainTime = 3;
    Xoroshiro rng(1);
    w.tick(rng);
    w.tick(rng);
    CHECK_FALSE(w.raining);
    w.tick(rng); // the countdown reaches 0: it starts raining
    CHECK(w.raining);
    w.tick(rng); // a new length is drawn: 12,000-23,999 ticks of rain
    CHECK(w.rainTime >= 12000);
    CHECK(w.rainTime < 24000);
    for (int i = 0; i < 50; ++i)
        w.tick(rng);
    CHECK(w.rain == doctest::Approx(0.52f).epsilon(0.05));
    CHECK(w.thunder == 0.0f); // no thunder unless thundering
}

TEST_CASE("weather: /weather clear holds clear skies for its duration") {
    Weather w;
    w.set(Weather::Kind::Rain, 100);
    CHECK(w.raining);
    w.set(Weather::Kind::Clear, 50);
    Xoroshiro rng(2);
    for (int i = 0; i < 49; ++i)
        w.tick(rng);
    CHECK_FALSE(w.raining);
    CHECK(w.clearTime == 1);
}

TEST_CASE("weather: rain and thunder each darken the daylight by up to 5/16") {
    const double noon = celestialAngle(6000);
    CHECK(skyDarken(noon) == doctest::Approx(0.0));
    CHECK(skyDarken(noon, 1.0, 0.0) == doctest::Approx(11.0 * 5.0 / 16.0));
    CHECK(skyDarken(noon, 1.0, 1.0) == doctest::Approx(11.0 * (1.0 - (11.0 / 16.0) * (11.0 / 16.0))));
}

TEST_CASE("weather: what falls depends on the biome's temperature and height") {
    Scene plains;
    CHECK(precipitationAt(plains.world, {0, 64, 0}) == Precipitation::Rain);
    // Above Y 80 it gets colder: plains (0.8) turn to snow 520 blocks up - beyond the
    // world, so still rain at the top.
    CHECK(precipitationAt(plains.world, {0, 300, 0}) == Precipitation::Rain);
    Scene snowy(Biome::SnowyPlains);
    CHECK(precipitationAt(snowy.world, {0, 64, 0}) == Precipitation::Snow);
    Scene desert(Biome::Desert);
    CHECK(precipitationAt(desert.world, {0, 64, 0}) == Precipitation::None);
}

TEST_CASE("weather: rain only falls where the sky is open above") {
    Scene s;
    s.weather.set(Weather::Kind::Rain, 1000);
    CHECK(rainHeight(s.world, 0, 0) == 64);
    CHECK(rainingAt(s.world, s.weather, {0, 64, 0}));
    s.world.setBlock({0, 70, 0}, S(blocks::Stone));
    CHECK_FALSE(rainingAt(s.world, s.weather, {0, 64, 0}));
    CHECK(rainingAt(s.world, s.weather, {0, 71, 0}));
}

TEST_CASE("weather: rain puts out fire, except on netherrack") {
    Scene s;
    s.weather.set(Weather::Kind::Rain, 100000);
    s.updates.setTime(0);
    s.world.setBlock({1, 64, 0}, S(blocks::OakLog)); // (keeps a dry fire alive)
    s.world.updateBlock({0, 64, 0}, BlockUpdates::fireState(0));
    s.world.setBlock({8, 63, 0}, S(blocks::Netherrack)); // infiniburn: rain doesn't matter
    s.world.updateBlock({8, 64, 0}, BlockUpdates::fireState(0));
    s.tick(400); // 20%+ per fire tick (30-40 ticks)
    CHECK(s.block({0, 64, 0}) == 0);
    CHECK(s.block({8, 64, 0}) == blocks::Fire);
}

TEST_CASE("weather: rain keeps farmland moist far from water") {
    Scene s;
    s.world.setBlock({0, 63, 0}, S(blocks::Farmland));
    s.weather.set(Weather::Kind::Rain, 100000);
    for (int i = 0; i < 20 && R().get(s.world.getBlock({0, 63, 0}), properties::moisture) != 7; ++i)
        s.tick(400);
    REQUIRE(s.block({0, 63, 0}) == blocks::Farmland);
    CHECK(R().get(s.world.getBlock({0, 63, 0}), properties::moisture) == 7);
}

TEST_CASE("weather: snow settles and still water freezes in cold biomes") {
    Scene s(Biome::SnowyPlains);
    s.weather.set(Weather::Kind::Rain, 1000000);
    for (int x = -2; x <= 2; ++x) // a pond with stone around it
        s.world.setBlock({x, 63, 0}, S(blocks::Water));
    s.tick(20000); // 1/16 of ticks check a random column of each chunk
    int snow = 0, ice = 0;
    for (int z = -16; z < 32; ++z)
        for (int x = -16; x < 32; ++x)
            snow += s.block({x, 64, z}) == blocks::Snow;
    for (int x = -2; x <= 2; ++x)
        ice += s.block({x, 63, 0}) == blocks::Ice;
    CHECK(snow > 20);
    CHECK(ice >= 1);
    CHECK(s.block({0, 64, 0}) != blocks::Snow); // (none on water or the new ice above it... yet)
}

TEST_CASE("weather: no snow or ice where it is warm") {
    Scene s;
    s.weather.set(Weather::Kind::Rain, 1000000);
    s.world.setBlock({0, 63, 0}, S(blocks::Water));
    s.tick(5000);
    for (int z = -16; z < 32; ++z)
        for (int x = -16; x < 32; ++x)
            REQUIRE(s.block({x, 64, z}) != blocks::Snow);
    CHECK(s.block({0, 63, 0}) == blocks::Water);
}

TEST_CASE("lightning sets fire where it strikes") {
    Scene s;
    s.updates.setTime(0);
    s.updates.strikeLightning({0, 64, 0});
    CHECK(s.block({0, 64, 0}) == blocks::Fire);
    REQUIRE(s.updates.lightning().size() == 1);
}

TEST_CASE("lightning charges creepers, turns pigs into zombified piglins and hurts the rest") {
    Scene s;
    Xoroshiro rng(3);
    REQUIRE(mc::Mobs::add(s.world, mc::Mobs::make(MobType::Creeper, {1.5, 64.0, 0.5}, rng)));
    REQUIRE(mc::Mobs::add(s.world, mc::Mobs::make(MobType::Pig, {-1.5, 64.0, 0.5}, rng)));
    REQUIRE(mc::Mobs::add(s.world, mc::Mobs::make(MobType::Cow, {0.5, 64.0, 2.5}, rng)));
    REQUIRE(mc::Mobs::add(s.world, mc::Mobs::make(MobType::Cow, {10.5, 64.0, 0.5}, rng))); // too far
    CHECK(mc::Mobs::strikeLightning(s.world, {0.5, 64.0, 0.5}));
    int powered = 0, piglins = 0, hurtCows = 0, wholeCows = 0;
    for (const MobData& m : s.world.chunk({0, 0})->mobs()) {
        powered += m.type == MobType::Creeper && m.powered;
        piglins += m.type == MobType::ZombifiedPiglin;
        if (m.type == MobType::Cow) (m.health < mobInfo(MobType::Cow).maxHealth ? hurtCows : wholeCows)++;
    }
    for (const MobData& m : s.world.chunk({-1, 0})->mobs())
        piglins += m.type == MobType::ZombifiedPiglin;
    CHECK(powered == 1);
    CHECK(piglins == 1);
    CHECK(hurtCows == 1);
    CHECK(wholeCows == 1);
}

TEST_CASE("weather: /weather sets the state and its duration") {
    Scene s;
    mc::Player player;
    mc::Inventory inventory;
    int64_t dayTime = 0;
    std::vector<BlockPos> bolts;
    mc::CommandContext ctx{player, inventory, dayTime, 0, 0};
    ctx.weather = &s.weather;
    ctx.lightning = &bolts;
    CHECK(mc::runCommand("/weather thunder 30s", ctx).ok);
    CHECK(s.weather.raining);
    CHECK(s.weather.thundering);
    CHECK(s.weather.rainTime == 600);
    CHECK(mc::runCommand("/weather clear 100", ctx).message == "Set the weather to clear");
    CHECK(s.weather.clearTime == 100);
    CHECK_FALSE(mc::runCommand("/weather snow", ctx).ok);
    CHECK(mc::runCommand("/summon lightning_bolt 1 64 2", ctx).ok);
    REQUIRE(bolts.size() == 1);
    CHECK(bolts[0] == BlockPos{1, 64, 2});
}

TEST_CASE("weather is saved in level.dat") {
    const auto dir = std::filesystem::temp_directory_path() / "mc_weather_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    LevelData l;
    l.raining = true;
    l.thundering = true;
    l.rainTime = 1234;
    l.thunderTime = 567;
    l.clearWeatherTime = 0;
    REQUIRE(l.save(dir));
    const auto back = LevelData::load(dir);
    REQUIRE(back);
    CHECK(back->raining);
    CHECK(back->thundering);
    CHECK(back->rainTime == 1234);
    CHECK(back->thunderTime == 567);
    std::filesystem::remove_all(dir);
}
