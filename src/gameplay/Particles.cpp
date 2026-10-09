#include "gameplay/Particles.h"

#include "world/ItemExtras.h"

#include "world/Biome.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mc {

using namespace world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
glm::vec3 rgb(uint32_t c) {
    return glm::vec3(float((c >> 16) & 255), float((c >> 8) & 255), float(c & 255)) / 255.0f;
}
double centred(Xoroshiro& rng) { return rng.nextDouble() * 2.0 - 1.0; }
// Vanilla's usual particle lifetime: base / (random x spread + floor) ticks.
int16_t life(Xoroshiro& rng, float base, float spread, float floor) {
    return static_cast<int16_t>(
        std::clamp(base / (rng.nextFloat() * spread + floor), 1.0f, 400.0f));
}

} // namespace

void Particles::fillGrid(const World& world, const glm::dvec3& player) {
    m_gridX = blockToChunk(int32_t(std::floor(player.x))) - 2;
    m_gridZ = blockToChunk(int32_t(std::floor(player.z))) - 2;
    for (int dz = 0; dz < 5; ++dz)
        for (int dx = 0; dx < 5; ++dx)
            m_grid[size_t(dz * 5 + dx)] = world.chunk({m_gridX + dx, m_gridZ + dz});
}

const Chunk* Particles::chunkAt(const World& world, int32_t x, int32_t z) const {
    const int32_t cx = blockToChunk(x) - m_gridX, cz = blockToChunk(z) - m_gridZ;
    if (cx >= 0 && cx < 5 && cz >= 0 && cz < 5) return m_grid[size_t(cz * 5 + cx)];
    return world.chunk({blockToChunk(x), blockToChunk(z)});
}

BlockStateId Particles::blockAt(const World& world, const BlockPos& p) const {
    const Chunk* c = chunkAt(world, p.x, p.z);
    return c ? c->get(blockToLocal(p.x), p.y, blockToLocal(p.z)) : BlockStateId(0);
}

Particle& Particles::add(const Particle& p) {
    Particle* slot = nullptr;
    if (m_particles.size() < size_t(kMax)) {
        m_particles.push_back(p);
        slot = &m_particles.back();
    } else { // full: replace the oldest-ish (round robin)
        m_next %= m_particles.size();
        slot = &m_particles[m_next++];
        *slot = p;
    }
    slot->prevPos = slot->pos;
    slot->origin = slot->pos;
    return *slot;
}

void Particles::blockBreak(BlockStateId state, const BlockPos& b, Xoroshiro& rng) {
    if (state == 0 || BlockUpdates::isFluid(R().blockOf(state))) return;
    // 4 x 4 x 4 pieces of its texture fly out from the middle (wiki: Particles ›
    // block; vanilla's destroy effect), falling and bouncing off blocks.
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) {
                Particle p;
                const glm::dvec3 f((i + 0.5) / 4.0, (j + 0.5) / 4.0, (k + 0.5) / 4.0);
                p.pos = glm::dvec3(b.x, b.y, b.z) + f;
                glm::dvec3 dir = f - glm::dvec3(0.5) +
                                 glm::dvec3(centred(rng), centred(rng), centred(rng)) * 0.4;
                const double len = glm::length(dir);
                if (len > 1e-6) dir /= len;
                p.vel = dir * ((rng.nextDouble() + rng.nextDouble() + 1.0) * 0.15 * 0.4) +
                        glm::dvec3(0, 0.1, 0);
                p.sprite = ParticleSprite::Terrain;
                p.state = state;
                p.u = uint8_t(rng.nextInt(4));
                p.v = uint8_t(rng.nextInt(4));
                p.color = glm::vec3(0.6f);
                p.size = 0.05f * (rng.nextFloat() * 0.5f + 0.5f) * 2.0f;
                p.gravity = 0.04f;
                p.lifetime = life(rng, 4.0f, 0.9f, 0.1f);
                add(p);
            }
}

void Particles::blockHit(BlockStateId state, const BlockPos& b, Direction face, Xoroshiro& rng) {
    if (state == 0) return;
    // One crumb off the face being mined each tick (vanilla's crack particles).
    Particle p;
    p.pos = glm::dvec3(b.x + 0.1 + rng.nextDouble() * 0.8, b.y + 0.1 + rng.nextDouble() * 0.8,
                       b.z + 0.1 + rng.nextDouble() * 0.8);
    const glm::ivec3 n = kDirectionNormals[int(face)];
    for (int a = 0; a < 3; ++a)
        if (n[a] != 0)
            p.pos[a] = double((a == 0 ? b.x : a == 1 ? b.y : b.z)) + (n[a] > 0 ? 1.1 : -0.1);
    p.vel = glm::dvec3(centred(rng), centred(rng), centred(rng)) * 0.02;
    p.sprite = ParticleSprite::Terrain;
    p.state = state;
    p.u = uint8_t(rng.nextInt(4));
    p.v = uint8_t(rng.nextInt(4));
    p.color = glm::vec3(0.6f);
    p.size = 0.06f * (rng.nextFloat() * 0.5f + 0.5f);
    p.gravity = 0.04f;
    p.lifetime = life(rng, 4.0f, 0.9f, 0.1f);
    add(p);
}

void Particles::puff(const glm::dvec3& at, Xoroshiro& rng) {
    // One big grey puff (vanilla's explosion particle: half-size 1-2 blocks, 6-9
    // ticks, its frames stepping as it grows), glowing a little.
    Particle p;
    p.pos = at;
    p.vel = glm::dvec3(0.0);
    p.color = glm::vec3(rng.nextFloat() * 0.6f + 0.4f);
    p.size = 2.0f * (1.0f - rng.nextFloat() * 0.5f);
    p.sprite = ParticleSprite::Generic0;
    p.frames = 8;
    p.lifetime = int16_t(6 + rng.nextInt(4));
    p.physics = false;
    p.fullBright = true;
    add(p);
}

void Particles::explosion(const glm::dvec3& at, float power, Xoroshiro& rng) {
    // Power 2+ (TNT, creepers...): an emitter puffing within 4 blocks for 8 ticks;
    // smaller blasts (fireballs) one puff (wiki: Particles › explosion_emitter).
    if (power < 2.0f) {
        puff(at, rng);
        return;
    }
    if (m_emitterCount < int(m_emitters.size())) m_emitters[size_t(m_emitterCount++)] = {at, 8};
}

void Particles::poof(const glm::dvec3& feet, double width, double height, Xoroshiro& rng) {
    // The cloud a mob leaves when its body vanishes (vanilla: 20 "poof" particles).
    for (int i = 0; i < 20; ++i) {
        Particle p;
        p.pos = feet +
                glm::dvec3(centred(rng) * width, rng.nextDouble() * height, centred(rng) * width);
        p.vel = glm::dvec3(centred(rng), centred(rng), centred(rng)) * 0.02;
        p.color = glm::vec3(rng.nextFloat() * 0.3f + 0.7f);
        p.size = 0.1f * (rng.nextFloat() * 0.6f + 0.6f);
        p.sprite = ParticleSprite::Generic0;
        p.frames = 8;
        p.reverseFrames = true;
        p.gravity = -0.004f;
        p.friction = 0.9f;
        p.lifetime = life(rng, 16.0f, 0.8f, 0.2f);
        add(p);
    }
}

void Particles::splashPotion(const glm::dvec3& at, uint32_t c, Xoroshiro& rng) {
    // A ring of coloured sparkles spreading from the impact (wiki: Splash Potion).
    const glm::vec3 base = rgb(c);
    for (int i = 0; i < 50; ++i) {
        Particle p;
        const double speed = rng.nextDouble() * 0.4, a = rng.nextDouble() * 2.0 * std::numbers::pi;
        p.pos = at;
        p.vel = {std::cos(a) * speed, 0.01 + rng.nextDouble() * 0.15, std::sin(a) * speed};
        p.color = base * (0.75f + rng.nextFloat() * 0.25f);
        p.size = 0.08f;
        p.sprite = ParticleSprite::Effect;
        p.gravity = -0.004f;
        p.friction = 0.85f;
        p.lifetime = life(rng, 8.0f, 0.8f, 0.2f);
        add(p);
    }
}

void Particles::firework(const glm::dvec3& at, uint32_t id, Xoroshiro& rng) {
    // (M28.4c; wiki: Firework Star › Shapes) sparks in the explosion's colours, fading to its
    // fade colours half way; a trail doubles them, slower; twinkling ones glitter (brightness).
    const auto f = world::fireworks(id);
    if (!f || f->count == 0) {
        smoke(at, false, rng);
        return;
    }
    static constexpr const char* kCreeper[8] = {"........", ".##..##.", ".##..##.", "...##...",
                                                "..####..", "..####..", "..#..#..", "........"};
    for (int k = 0; k < f->count; ++k) {
        const world::FireworkExplosion& e = f->explosions[size_t(k)];
        std::array<glm::vec3, 16> cols{}, fades{};
        int nc = 0, nf = 0;
        for (int d = 0; d < 16; ++d) {
            if (e.colours >> d & 1) cols[size_t(nc++)] = rgb(world::kFireworkColours[d]);
            if (e.fades >> d & 1) fades[size_t(nf++)] = rgb(world::kFireworkColours[d]);
        }
        if (nc == 0) cols[size_t(nc++)] = glm::vec3(1.0f);
        auto spark = [&](const glm::dvec3& vel) {
            for (int t = 0; t < (e.trail ? 2 : 1); ++t) {
                Particle p;
                p.pos = at;
                p.vel = vel * (t == 0 ? 1.0 : 0.6);
                p.color = cols[rng.nextInt(uint32_t(nc))] *
                          (e.twinkle ? 0.6f + rng.nextFloat() * 0.4f : 1.0f);
                if (nf > 0) p.fade = fades[rng.nextInt(uint32_t(nf))];
                p.size = 0.08f;
                p.sprite = ParticleSprite::Effect;
                p.fullBright = true;
                p.physics = false;
                p.gravity = 0.004f;
                p.friction = 0.91f;
                p.lifetime = int16_t(30 + rng.nextInt(15) + (t ? 10 : 0));
                add(p);
            }
        };
        const double turn =
            rng.nextDouble() * 2.0 * std::numbers::pi; // (flat shapes face a random way)
        const glm::dvec3 right(std::cos(turn), 0.0, std::sin(turn)), up(0.0, 1.0, 0.0);
        switch (e.shape) {
        case 1:   // large ball
        case 0: { // small ball: a shell of sparks
            const int n = e.shape == 1 ? 150 : 70;
            const double speed = e.shape == 1 ? 0.5 : 0.25;
            for (int i = 0; i < n; ++i) {
                glm::dvec3 d(centred(rng), centred(rng), centred(rng));
                if (glm::length(d) < 1e-3) continue;
                spark(glm::normalize(d) * speed * (0.85 + rng.nextDouble() * 0.15));
            }
            break;
        }
        case 2: // a five-pointed star's outline
            for (int i = 0; i < 100; ++i) {
                const double t = double(i) / 100.0 * 10.0;
                const int v = int(t);
                const double a0 = double(v) * std::numbers::pi / 5.0,
                             a1 = double(v + 1) * std::numbers::pi / 5.0;
                const double r0 = v % 2 ? 0.4 : 1.0, r1 = v % 2 ? 1.0 : 0.4, w = t - v;
                const double x = (1 - w) * r0 * std::sin(a0) + w * r1 * std::sin(a1);
                const double y = (1 - w) * r0 * std::cos(a0) + w * r1 * std::cos(a1);
                spark((right * x + up * y) * 0.5);
            }
            break;
        case 3: // a creeper's face
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    if (kCreeper[y][x] == '#')
                        for (int r = 0; r < 2; ++r)
                            spark((right * ((x - 3.5) / 3.5) + up * ((3.5 - y) / 3.5)) *
                                  (0.5 + 0.05 * r));
            break;
        default: // burst: a fountain upward
            for (int i = 0; i < 80; ++i) {
                const glm::dvec3 d(centred(rng) * 0.4, 1.0, centred(rng) * 0.4);
                spark(glm::normalize(d) * (0.2 + rng.nextDouble() * 0.3));
            }
            break;
        }
    }
}

void Particles::crit(const glm::dvec3& at, Xoroshiro& rng) {
    // Sparks bursting from a critical hit, quickly slowing and dropping.
    for (int i = 0; i < 16; ++i) {
        Particle p;
        p.pos = at + glm::dvec3(centred(rng), centred(rng), centred(rng)) * 0.3;
        p.vel = glm::dvec3(centred(rng), centred(rng) + 0.2, centred(rng)) * 0.4;
        const float v = rng.nextFloat() * 0.3f + 0.6f;
        p.color = glm::vec3(v, v, v * 0.85f);
        p.size = 0.075f;
        p.sprite = ParticleSprite::Crit;
        p.gravity = 0.02f;
        p.friction = 0.7f;
        p.lifetime = life(rng, 6.0f, 0.8f, 0.6f);
        add(p);
    }
}

void Particles::smoke(const glm::dvec3& at, bool large, Xoroshiro& rng) {
    Particle p;
    p.pos = at;
    p.vel = glm::dvec3(centred(rng) * 0.01, 0.0, centred(rng) * 0.01);
    p.color = glm::vec3(rng.nextFloat() * 0.3f);
    p.size = 0.075f * (rng.nextFloat() * 0.5f + 0.5f) * (large ? 2.5f : 1.0f);
    p.sprite = ParticleSprite::Generic0;
    p.frames = 8;
    p.reverseFrames = true;
    p.gravity = -0.004f; // smoke rises
    p.friction = 0.96f;
    p.lifetime = life(rng, large ? 20.0f : 8.0f, 0.8f, 0.2f);
    add(p);
}

Particle& Particles::flame(const glm::dvec3& at, Xoroshiro& rng) {
    Particle p;
    p.pos = at;
    p.vel = glm::dvec3(centred(rng) * 0.002, 0.0, centred(rng) * 0.002);
    p.size = 0.05f * (rng.nextFloat() * 0.2f + 0.5f) * 2.0f;
    p.sprite = ParticleSprite::Flame;
    p.friction = 0.96f;
    p.physics = false;
    p.fullBright = true;
    p.lifetime = int16_t(life(rng, 8.0f, 0.8f, 0.2f) + 4);
    return add(p);
}

void Particles::portal(const glm::dvec3& at, Xoroshiro& rng) {
    // Purple sparks that start a little out and drift back to where they began
    // (vanilla's portal particle), glowing.
    Particle p;
    p.pos = at;
    p.vel = glm::dvec3(centred(rng), centred(rng), centred(rng)) * 0.5;
    const float b = rng.nextFloat() * 0.6f + 0.4f;
    p.color = glm::vec3(b * 0.9f, b * 0.3f, b);
    p.size = 0.05f * (rng.nextFloat() * 0.2f + 0.5f) * 2.0f;
    p.sprite = static_cast<ParticleSprite>(rng.nextInt(4));
    p.physics = false;
    p.toOrigin = true;
    p.fullBright = true;
    p.lifetime = int16_t(rng.nextInt(10) + 40);
    add(p);
}

void Particles::effectSwirl(const glm::dvec3& feet, double width, double height, uint32_t c,
                            Xoroshiro& rng) {
    if (rng.nextInt(2) != 0) return; // (vanilla: half the ticks for non-ambient effects)
    Particle p;
    p.pos = feet + glm::dvec3(centred(rng) * width * 0.5, rng.nextDouble() * height,
                              centred(rng) * width * 0.5);
    p.vel = glm::dvec3(0.0);
    p.color = rgb(c);
    p.size = 0.08f;
    p.sprite = ParticleSprite::Effect;
    p.gravity = -0.004f;
    p.friction = 0.96f;
    p.lifetime = life(rng, 8.0f, 0.8f, 0.2f);
    add(p);
}

void Particles::animate(World& world, const BlockPos& b, Xoroshiro& rng) {
    const BlockStateId s = blockAt(world, b);
    if (s == 0) return;
    const BlockId id = R().blockOf(s);
    const glm::dvec3 c(b.x + 0.5, b.y + 0.5, b.z + 0.5);
    auto dust = [&](const glm::dvec3& at, uint32_t colour) { // redstone (vanilla DustParticle)
        Particle p;
        p.pos = at;
        p.vel = glm::dvec3(0.0, 0.004, 0.0);
        p.color = rgb(colour) * (rng.nextFloat() * 0.4f + 0.6f);
        p.size = 0.06f;
        p.sprite = ParticleSprite::Generic0;
        p.frames = 8;
        p.reverseFrames = true;
        p.friction = 0.96f;
        p.physics = false;
        p.lifetime = life(rng, 8.0f, 0.8f, 0.2f);
        add(p);
    };
    // (M28.5a) a small flame over a lit candle (or a candle cake's); fireflies about a bush
    if ((R().likeOf(id) == blocks::Candle || R().likeOf(id) == blocks::CandleCake) &&
        R().get(s, properties::lit) == 0) {
        const double top = R().likeOf(id) == blocks::Candle ? 0.45 : 0.95;
        Particle& f = flame({c.x + centred(rng) * 0.15, b.y + top, c.z + centred(rng) * 0.15}, rng);
        f.size *= 0.6f;
        return;
    }
    if (id == blocks::FireflyBush && rng.nextInt(4) == 0) {
        Particle p;
        p.pos = {b.x + rng.nextDouble(), b.y + 0.3 + rng.nextDouble() * 1.2,
                 b.z + rng.nextDouble()};
        p.vel = {centred(rng) * 0.01, centred(rng) * 0.01, centred(rng) * 0.01};
        p.color = glm::vec3(0.85f, 1.0f, 0.35f);
        p.size = 0.04f;
        p.sprite = ParticleSprite::Effect;
        p.fullBright = true;
        p.physics = false;
        p.friction = 1.0f;
        p.lifetime = int16_t(40 + rng.nextInt(40));
        add(p);
        return;
    }
    switch (id) {
    case blocks::Torch: // smoke and a flame over the tip (wiki: Torch)
    case blocks::SoulTorch:
    case blocks::WallTorch: // (wall torches: the tip leans out from the wall)
    case blocks::SoulWallTorch: {
        glm::dvec3 tip(c.x, b.y + 0.7, c.z);
        if (id == blocks::WallTorch ||
            id == blocks::SoulWallTorch) { // (our stick stands upright by the wall)
            const glm::ivec3 out = kDirectionNormals[R().get(s, properties::facing) + 2];
            tip = {c.x - out.x * 0.3125, b.y + 0.85, c.z - out.z * 0.3125};
        }
        smoke(tip, false, rng);
        Particle& f = flame(tip, rng);
        if (id == blocks::SoulTorch || id == blocks::SoulWallTorch)
            f.color = glm::vec3(0.45f, 0.85f, 1.0f); // soul fire
        break;
    }
    case blocks::RedstoneTorch:
    case blocks::RedstoneWallTorch:
        if (R().get(s, properties::lit) == 0)
            dust({c.x + centred(rng) * 0.1, b.y + 0.7 + centred(rng) * 0.1,
                  c.z + centred(rng) * 0.1},
                 0xFF0000);
        break;
    case blocks::RedstoneWire: {
        const int power = R().get(s, properties::power);
        if (power > 0 && rng.nextInt(5) == 0)
            dust({b.x + rng.nextDouble(), b.y + 0.06, b.z + rng.nextDouble()},
                 redstoneColor(power));
        break;
    }
    case blocks::Campfire: // campfire smoke: tall columns, taller from a signal fire (wiki:
                           // Campfire)
    case blocks::SoulCampfire:
        if (R().get(s, properties::lit) == 0 && rng.nextInt(3) == 0) {
            smoke({c.x + centred(rng) * 0.3, b.y + 0.5, c.z + centred(rng) * 0.3}, true, rng);
            Particle& last =
                m_particles[m_particles.size() < size_t(kMax) ? m_particles.size() - 1
                                                              : (m_next + kMax - 1) % kMax];
            last.lifetime =
                int16_t(R().get(s, properties::signalFire) == 0 ? 240 : 100); // (rises 5+ blocks)
            last.gravity = -0.006f;
            last.color = glm::vec3(0.55f);
            if (rng.nextInt(10) == 0)
                world.playSound(Sound::FireAmbient, c.x, c.y, c.z, 1.0f, 1.0f);
        }
        break;
    case blocks::Fire: // rising large smoke (wiki: Fire); crackling 1 in 24
        if (rng.nextInt(24) == 0)
            world.playSound(Sound::FireAmbient, c.x, c.y, c.z, 1.0f + rng.nextFloat(), 1.0f);
        smoke({b.x + rng.nextDouble(), b.y + 0.5 + rng.nextDouble() * 0.5, b.z + rng.nextDouble()},
              true, rng);
        break;
    case blocks::Furnace:
    case blocks::Smoker:
    case blocks::BlastFurnace:
        if (R().get(s, properties::lit) == 0) { // flames and smoke at the front while smelting
            const glm::ivec3 n = kDirectionNormals[R().get(s, properties::facing) + 2];
            const double side = centred(rng) * 0.3;
            const glm::dvec3 at(c.x + n.x * 0.52 + (n.x == 0 ? side : 0.0),
                                b.y + rng.nextDouble() * 6.0 / 16.0,
                                c.z + n.z * 0.52 + (n.z == 0 ? side : 0.0));
            smoke(at, false, rng);
            flame(at, rng);
            if (rng.nextInt(10) == 0)
                world.playSound(Sound::FireAmbient, c.x, c.y, c.z, 0.5f, 1.0f); // (crackle)
        }
        break;
    case blocks::Lava:
        // Lava pops: an ember jumps from the surface now and then (wiki: Lava).
        if (blockAt(world, {b.x, b.y + 1, b.z}) == 0 && rng.nextInt(100) == 0) {
            world.playSound(Sound::LavaPop, b.x + 0.5, b.y + 1.0, b.z + 0.5);
            Particle p;
            p.pos = {b.x + rng.nextDouble(), b.y + 1.0, b.z + rng.nextDouble()};
            p.vel = {centred(rng) * 0.04, rng.nextDouble() * 0.25 + 0.05, centred(rng) * 0.04};
            p.size = 0.05f * (rng.nextFloat() * 0.2f + 0.75f) * 2.0f;
            p.sprite = ParticleSprite::Lava;
            p.gravity = 0.03f;
            p.friction = 0.999f;
            p.fullBright = true;
            p.lifetime = life(rng, 16.0f, 0.8f, 0.2f);
            add(p);
        }
        break;
    case blocks::BubbleColumn: { // (M29.5; wiki: Bubble Column) bubbles rise, or are pulled down
        const bool down = blockRegistry().get(world.getBlock(b), properties::drag) == 0;
        Particle p;
        p.pos = {b.x + 0.2 + rng.nextDouble() * 0.6, b.y + rng.nextDouble(),
                 b.z + 0.2 + rng.nextDouble() * 0.6};
        p.vel = {0.0, down ? -0.08 : 0.12, 0.0};
        p.size = 0.06f;
        p.sprite = ParticleSprite::Bubble;
        p.gravity = 0.0f;
        p.friction = 1.0f;
        p.lifetime = 12;
        add(p);
        break;
    }
    case blocks::NetherPortal:
        if (rng.nextInt(100) == 0) world.playSound(Sound::PortalAmbient, c.x, c.y, c.z);
        for (int i = 0; i < 4; ++i)
            portal({b.x + rng.nextDouble(), b.y + rng.nextDouble(), b.z + rng.nextDouble()}, rng);
        break;
    case blocks::EndRod:
        if (rng.nextInt(5) == 0) { // white glints drifting from the rod (wiki: End Rod)
            Particle p;
            p.pos = c + glm::dvec3(centred(rng), centred(rng), centred(rng)) * 0.3;
            p.vel = glm::dvec3(centred(rng), centred(rng), centred(rng)) * 0.01;
            p.color = glm::vec3(1.0f, 0.98f, 0.9f);
            p.size = 0.06f;
            p.sprite = ParticleSprite::Effect;
            p.friction = 0.91f;
            p.physics = false;
            p.fullBright = true;
            p.lifetime = int16_t(60 + rng.nextInt(12));
            add(p);
        }
        break;
    default:
        break;
    }
    // Drips: under a solid block with water or lava resting on it (wiki: Particles ›
    // dripping_water / dripping_lava) - they hang, then fall.
    if (R().opaqueCube(s) && rng.nextInt(10) == 0 && blockAt(world, {b.x, b.y - 1, b.z}) == 0) {
        const BlockId above = R().blockOf(blockAt(world, {b.x, b.y + 1, b.z}));
        if (above == blocks::Water || above == blocks::Lava) {
            Particle p;
            p.pos = {b.x + 0.1 + rng.nextDouble() * 0.8, b.y - 0.05,
                     b.z + 0.1 + rng.nextDouble() * 0.8};
            p.color = above == blocks::Water ? glm::vec3(0.25f, 0.35f, 1.0f)
                                             : glm::vec3(1.0f, 0.45f, 0.05f);
            p.size = 0.05f;
            p.sprite = ParticleSprite::Drip;
            p.hangs = true;
            p.gravity = 0.06f;
            p.fullBright = above == blocks::Lava;
            p.lifetime = 40 + 60; // 40 hanging (lava: 40 too here), then up to 60 falling
            add(p);
        }
    }
}

void Particles::rain(const World& world, const glm::dvec3& player, const Weather& weather,
                     Xoroshiro& rng) {
    // Splashes where the rain lands near the player (vanilla: 100 x rain^2 tries a tick
    // within 10 blocks; over lava, smoke instead).
    const int tries = int(100.0f * weather.rain * weather.rain);
    for (int i = 0; i < tries; ++i) {
        const int x = int(std::floor(player.x)) + int(rng.nextInt(21)) - 10;
        const int z = int(std::floor(player.z)) + int(rng.nextInt(21)) - 10;
        const int top = rainHeight(world, x, z);
        if (top > player.y + 10.0 || top < player.y - 10.0) continue;
        if (!rainFallsOn(world, weather, {x, top, z})) continue; // (top is the rain height already)
        const glm::dvec3 at(x + rng.nextDouble(), top + 0.02, z + rng.nextDouble());
        if (R().blockOf(blockAt(world, {x, top - 1, z})) == blocks::Lava) {
            smoke(at, false, rng);
            continue;
        }
        Particle p;
        p.pos = at;
        p.vel = {centred(rng) * 0.03, rng.nextDouble() * 0.1 + 0.05, centred(rng) * 0.03};
        p.size = 0.05f;
        p.sprite = ParticleSprite::Splash0;
        p.frames = 4;
        p.gravity = 0.06f;
        p.lifetime = life(rng, 8.0f, 0.8f, 0.2f);
        add(p);
    }
}

void Particles::move(const World& world, Particle& p) {
    p.prevPos = p.pos;
    if (p.toOrigin) { // portal sparks: from origin + vel back to the origin
        const double t = double(p.age) / double(std::max<int>(1, p.lifetime));
        p.pos = p.origin + p.vel * (1.0 - t);
        return;
    }
    if (p.hangs && p.age < 40) return; // a drip clinging to the block
    p.vel.y -= p.gravity;
    if (!p.physics) {
        p.pos += p.vel;
    } else {
        // Axis by axis against block cells (full cells: our simplification).
        p.onGround = false;
        for (const int a : {1, 0, 2}) {
            glm::dvec3 next = p.pos;
            next[a] += p.vel[a];
            if (R().collides(blockAt(world, {int(std::floor(next.x)), int(std::floor(next.y)),
                                             int(std::floor(next.z))}))) {
                if (a == 1 && p.vel.y < 0.0) p.onGround = true;
                p.vel[a] = 0.0;
            } else {
                p.pos = next;
            }
        }
    }
    p.vel *= double(p.friction);
    if (p.onGround) {
        p.vel.x *= 0.7;
        p.vel.z *= 0.7;
    }
}

void Particles::tick(World& world, const std::vector<LevelEvent>& events, const glm::dvec3& player,
                     const Weather* weather, Xoroshiro& rng) {
    fillGrid(world, player);
    for (const LevelEvent& e : events) {
        const glm::dvec3 at(e.x, e.y, e.z);
        const BlockPos b{int(std::floor(e.x)), int(std::floor(e.y)), int(std::floor(e.z))};
        switch (e.type) {
        case LevelEvent::Type::BlockBreak:
            blockBreak(BlockStateId(e.data), b, rng);
            break;
        case LevelEvent::Type::BlockHit:
            blockHit(BlockStateId(e.data & 0xFFFF), b, static_cast<Direction>((e.data >> 16) & 7),
                     rng);
            break;
        case LevelEvent::Type::Explosion:
            explosion(at, float(e.data) / 10.0f, rng);
            break;
        case LevelEvent::Type::MobDeath:
            poof(at, double(e.data & 0xFFFF) / 100.0, double(e.data >> 16) / 100.0, rng);
            break;
        case LevelEvent::Type::PotionSplash:
            splashPotion(at, e.data, rng);
            break;
        case LevelEvent::Type::Firework:
            firework(at, e.data, rng);
            break;
        case LevelEvent::Type::Crit:
            crit(at, rng);
            break;
        case LevelEvent::Type::SulfurGas: { // (M33.2b; wiki: Potent Sulfur) a second's bubbles and haze
            // (M33 perf review) only near the player: a cave of vents far away would fill the pool
            if (glm::dot(at - player, at - player) > 48.0 * 48.0) break;
            const int depth = int(e.data);
            for (int i = 0; i < 3 * depth; ++i) {
                Particle p;
                p.pos = at + glm::dvec3(0.3 + rng.nextDouble() * 0.4, 1.0 + rng.nextDouble() * depth, 0.3 + rng.nextDouble() * 0.4);
                p.vel = {0.0, 0.12, 0.0};
                p.size = 0.06f;
                p.sprite = ParticleSprite::Bubble;
                p.friction = 1.0f;
                p.physics = false;
                p.lifetime = int16_t(std::max(2.0, (at.y + 1.0 + depth - p.pos.y) / 0.12));
                add(p);
            }
            for (int i = 0; i < 6; ++i) {
                Particle g;
                g.pos = at + glm::dvec3(0.5 + centred(rng) * 2.5, 1.0 + depth + rng.nextDouble() * 0.6, 0.5 + centred(rng) * 2.5);
                g.vel = {centred(rng) * 0.01, 0.005, centred(rng) * 0.01};
                g.color = glm::vec3(0.82f, 0.9f, 0.6f);
                g.size = 0.25f;
                g.sprite = ParticleSprite::Generic0;
                g.frames = 8;
                g.physics = false;
                g.friction = 0.96f;
                g.lifetime = int16_t(30 + rng.nextInt(20));
                add(g);
            }
            break;
        }
        case LevelEvent::Type::Happy: // (M32.5; vanilla happy_villager) green sparkles about it
            for (int i = 0; i < 12; ++i) {
                Particle p;
                const double h = double(e.data) / 100.0;
                p.pos = at + glm::dvec3(centred(rng) * 0.6, 0.5 + rng.nextDouble() * h,
                                        centred(rng) * 0.6);
                p.vel = glm::dvec3(0.0, 0.02, 0.0);
                p.color = glm::vec3(0.3f, 0.95f, 0.3f);
                p.size = 0.06f;
                p.sprite = ParticleSprite::Effect;
                p.fullBright = true;
                p.physics = false;
                p.friction = 0.9f;
                p.lifetime = int16_t(20 + rng.nextInt(10));
                add(p);
            }
            break;
        case LevelEvent::Type::Totem: // (M32.3; wiki: Totem of Undying) green and yellow sparks
            for (int i = 0; i < 80; ++i) {
                Particle p;
                p.pos = at;
                p.vel = glm::dvec3(centred(rng), centred(rng) * 0.6 + 0.3, centred(rng)) * 1.4;
                const float r = rng.nextFloat();
                p.color = rng.nextInt(4) == 0
                              ? glm::vec3(0.6f + r * 0.2f, 0.6f + r * 0.3f, r * 0.2f)
                              : glm::vec3(0.1f + r * 0.2f, 0.4f + r * 0.3f, r * 0.2f);
                p.size = 0.08f;
                p.sprite = ParticleSprite::Effect;
                p.fullBright = true;
                p.gravity = 0.012f;
                p.friction = 0.82f + rng.nextFloat() * 0.1f;
                p.lifetime = int16_t(60 + rng.nextInt(12));
                add(p);
            }
            break;
        case LevelEvent::Type::BlockPlace:
            break; // (a sound only)
        case LevelEvent::Type::Extinguish:
            for (int i = 0; i < 8; ++i)
                smoke(at + glm::dvec3(centred(rng), rng.nextDouble(), centred(rng)) * 0.5, true,
                      rng);
            break;
        case LevelEvent::Type::Note: { // one note rising, its colour around the wheel by pitch
                                       // (wiki)
            Particle p;
            p.pos = at;
            p.vel = {0.0, 0.2, 0.0};
            const float h = float(e.data) / 24.0f;
            auto ch = [&](float o) {
                return std::clamp(std::sin((h + o) * 6.2832f) * 0.65f + 0.35f, 0.0f, 1.0f);
            };
            p.color = glm::vec3(ch(0.0f), ch(0.33f), ch(0.67f));
            p.size = 0.15f;
            p.sprite = ParticleSprite::Note;
            p.friction = 0.66f;
            p.lifetime = 6;
            p.physics = false;
            p.fullBright = true;
            add(p);
            break;
        }
        case LevelEvent::Type::Portal:
            for (int i = 0; i < 32; ++i)
                portal(
                    at + glm::dvec3(centred(rng) * 0.5, rng.nextDouble() * 2.0, centred(rng) * 0.5),
                    rng);
            break;
        }
    }
    for (int i = 0; i < m_emitterCount;) {
        Emitter& e = m_emitters[size_t(i)];
        for (int k = 0; k < 6; ++k)
            puff(e.at + glm::dvec3(centred(rng), centred(rng), centred(rng)) * 4.0, rng);
        if (--e.ticks <= 0)
            e = m_emitters[size_t(--m_emitterCount)];
        else
            ++i;
    }
    // Vanilla's animate ticks: 667 random blocks within 16 and 667 within 32 of the
    // player get a chance to show their particles every tick.
    const BlockPos centre{int(std::floor(player.x)), int(std::floor(player.y)),
                          int(std::floor(player.z))};
    for (const int range : {16, 32})
        for (int i = 0; i < 667; ++i)
            animate(
                world,
                {centre.x + int(rng.nextInt(uint32_t(range))) - int(rng.nextInt(uint32_t(range))),
                 centre.y + int(rng.nextInt(uint32_t(range))) - int(rng.nextInt(uint32_t(range))),
                 centre.z + int(rng.nextInt(uint32_t(range))) - int(rng.nextInt(uint32_t(range)))},
                rng);
    if (weather && weather->rain > 0.0f) rain(world, player, *weather, rng);

    for (Particle& p : m_particles) {
        ++p.age;
        if (p.age >= p.lifetime) continue;
        if (p.fade.r >= 0.0f && p.age == p.lifetime / 2) p.color = p.fade; // (M28.4c)
        move(world, p);
        if (p.hangs && p.onGround) p.age = p.lifetime; // a drip hitting the floor is gone
        // Rain splashes on the ground vanish half the time each tick (vanilla's water drops).
        if (p.sprite == ParticleSprite::Splash0 && p.onGround && (rng.nextInt(2) == 0))
            p.age = p.lifetime;
        const BlockPos at{int(std::floor(p.pos.x)), int(std::floor(p.pos.y)),
                          int(std::floor(p.pos.z))};
        if (const Chunk* c = chunkAt(world, at.x, at.z); c && c->lit()) {
            p.skyLight = c->skyLight(blockToLocal(at.x), at.y, blockToLocal(at.z));
            p.blockLight = c->blockLight(blockToLocal(at.x), at.y, blockToLocal(at.z));
        }
    }
    std::erase_if(m_particles, [](const Particle& p) { return p.age >= p.lifetime; });
    if (m_next >= m_particles.size()) m_next = 0;
}

} // namespace mc
