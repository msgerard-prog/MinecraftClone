// Sounds (M22.4; wiki: Sounds.json, ADR 0008).
#include "core/Files.h"
#include "world/Blocks.h"
#include "world/Sounds.h"
#include "world/World.h"

#include <doctest/doctest.h>

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

using namespace mc::world;

TEST_CASE("sounds: every event's files exist as 22,050 Hz mono 16-bit WAVs") {
    int files = 0;
    for (int s = 0; s < kSoundCount; ++s)
        for (const std::string& f : soundInfo(static_cast<Sound>(s)).files) {
            std::ifstream in(mc::assetPath(("minecraft/sounds/" + f + ".wav").c_str()), std::ios::binary);
            INFO(f);
            REQUIRE(in.good());
            const std::vector<char> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            REQUIRE(b.size() > 44);
            CHECK(std::memcmp(b.data(), "RIFF", 4) == 0);
            uint16_t channels = 0, bits = 0;
            uint32_t rate = 0;
            std::memcpy(&channels, b.data() + 22, 2);
            std::memcpy(&rate, b.data() + 24, 4);
            std::memcpy(&bits, b.data() + 34, 2);
            CHECK(channels == 1);
            CHECK(rate == 22050u);
            CHECK(bits == 16);
            ++files;
        }
    CHECK(files > 150);
}

TEST_CASE("sounds: blocks sound like their material") {
    const auto& r = blockRegistry();
    CHECK(soundTypeOf(r.defaultState(blocks::Stone)) == SoundType::Stone);
    CHECK(soundTypeOf(r.defaultState(blocks::OakPlanks)) == SoundType::Wood);
    CHECK(soundTypeOf(r.defaultState(blocks::GrassBlock)) == SoundType::Grass);
    CHECK(soundTypeOf(r.defaultState(blocks::Sand)) == SoundType::Sand);
    CHECK(soundTypeOf(r.defaultState(blocks::Sandstone)) == SoundType::Stone);
    CHECK(soundTypeOf(r.defaultState(blocks::Gravel)) == SoundType::Gravel);
    CHECK(soundTypeOf(r.defaultState(blocks::Glass)) == SoundType::Glass);
    CHECK(soundTypeOf(r.defaultState(blocks::WhiteWool)) == SoundType::Wool);
    CHECK(soundTypeOf(r.defaultState(blocks::IronBlock)) == SoundType::Metal);
    // Glass shatters when broken but steps like stone.
    CHECK(soundInfo(blockSoundOf(r.defaultState(blocks::Glass), BlockSound::Break)).files[0] == "random/glass1");
    CHECK(soundInfo(blockSound(SoundType::Stone, BlockSound::Break)).id == "block.stone.break");
    CHECK(soundInfo(blockSound(SoundType::Grass, BlockSound::Step)).volume == doctest::Approx(0.15f));
}

TEST_CASE("sounds: mobs have their calls; creepers and slimes are quiet until hurt") {
    CHECK(soundInfo(mobSound(MobType::Cow, MobSound::Ambient)).id == "entity.cow.ambient");
    CHECK(soundInfo(mobSound(MobType::Cow, MobSound::Ambient)).files.size() == 3);
    CHECK(soundInfo(mobSound(MobType::Creeper, MobSound::Ambient)).files.empty());
    CHECK(soundInfo(mobSound(MobType::Slime, MobSound::Ambient)).files.empty());
    CHECK_FALSE(soundInfo(mobSound(MobType::Creeper, MobSound::Hurt)).files.empty());
    // No death sound of its own: the hurt one (vanilla cows).
    CHECK(soundInfo(mobSound(MobType::Cow, MobSound::Death)).files == soundInfo(mobSound(MobType::Cow, MobSound::Hurt)).files);
    CHECK(soundInfo(mobSound(MobType::Minecart, MobSound::Hurt)).files.empty());
}

TEST_CASE("sounds: the world queues sounds up to its capacity") {
    World w;
    for (int i = 0; i < 2000; ++i)
        w.playSound(Sound::Click, 0, 0, 0);
    CHECK(w.soundEvents().size() == w.soundEvents().capacity());
}
