// Particles (M22.3; wiki: Particles).
#include "gameplay/Particles.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

struct Scene {
    World world;
    Particles particles;
    Xoroshiro rng{7};
    Scene() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, S(blocks::Stone));
            }
    }
    void tick(int n, const Weather* weather = nullptr) {
        for (int i = 0; i < n; ++i) {
            particles.tick(world, world.levelEvents(), {0.5, 65.6, 0.5}, weather, rng);
            world.levelEvents().clear();
        }
    }
    int count(ParticleSprite s) const {
        int n = 0;
        for (const Particle& p : particles.all())
            n += p.sprite == s;
        return n;
    }
};

} // namespace

TEST_CASE("particles: a broken block bursts into 64 pieces of its texture that fall and fade") {
    Scene s;
    s.world.levelEvent(LevelEvent::Type::BlockBreak, 0, 64, 0, S(blocks::Dirt));
    s.tick(1);
    REQUIRE(s.count(ParticleSprite::Terrain) == 64);
    for (const Particle& p : s.particles.all())
        CHECK(p.state == S(blocks::Dirt));
    s.tick(45); // lifetimes are 4-40 ticks
    CHECK(s.particles.all().empty());
}

TEST_CASE("particles: pieces don't fall through the ground") {
    Scene s;
    s.particles.blockBreak(S(blocks::Dirt), {0, 64, 0}, s.rng);
    s.tick(3);
    for (const Particle& p : s.particles.all())
        CHECK(p.pos.y >= 64.0);
}

TEST_CASE("particles: torches smoke and flicker, furnaces only while lit") {
    Scene s;
    s.world.setBlock({1, 64, 1}, S(blocks::Torch));
    s.tick(200); // animate ticks pick random blocks near the player
    CHECK(s.count(ParticleSprite::Flame) > 0);
    Scene cold;
    cold.world.setBlock({1, 64, 1}, S(blocks::Furnace)); // (unlit)
    cold.tick(200);
    CHECK(cold.count(ParticleSprite::Flame) == 0);
}

TEST_CASE("particles: explosions, deaths, splashes, crits and teleports have their effects") {
    Scene s;
    s.world.levelEvent(LevelEvent::Type::Explosion, 0, 65, 0, 40); // power 4: an emitter
    s.world.levelEvent(LevelEvent::Type::MobDeath, 3, 64, 0, 60 | 180 << 16);
    s.world.levelEvent(LevelEvent::Type::PotionSplash, -3, 64, 0, 0xF82423);
    s.world.levelEvent(LevelEvent::Type::Crit, 0, 65, 3);
    s.world.levelEvent(LevelEvent::Type::Portal, 0, 64, -3);
    s.tick(1);
    CHECK(s.count(ParticleSprite::Crit) == 16);
    CHECK(s.count(ParticleSprite::Effect) == 50);
    int red = 0;
    for (const Particle& p : s.particles.all())
        red += p.sprite == ParticleSprite::Effect && p.color.r > p.color.g * 2.0f;
    CHECK(red == 50); // the potion's colour
    const size_t first = s.particles.all().size();
    s.tick(3); // the explosion keeps puffing for 8 ticks
    CHECK(s.particles.all().size() > first - 20);
}

TEST_CASE("particles: rain splashes on the ground near the player") {
    Scene s;
    Weather w;
    w.set(Weather::Kind::Rain, 10000);
    w.rain = 1.0f;
    s.tick(5, &w);
    CHECK(s.count(ParticleSprite::Splash0) > 50);
    for (const Particle& p : s.particles.all())
        CHECK(p.pos.y >= 64.0);
}

TEST_CASE("particles: the pool never grows past its cap") {
    Scene s;
    for (int i = 0; i < 100; ++i)
        s.particles.blockBreak(S(blocks::Stone), {0, 70, 0}, s.rng);
    CHECK(s.particles.all().size() == size_t(Particles::kMax));
    CHECK(s.particles.all().capacity() == size_t(Particles::kMax));
}
