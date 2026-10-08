#include "rendering/EntityRenderer.h"

#include "core/Log.h"
#include "rendering/MobModels.h"
#include "rendering/ResourcePack.h"
#include "rendering/SpriteImage.h"
#include "rendering/TextureAtlas.h"
#include "world/Blocks.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <string>
#include <cmath>
#include <string>

namespace mc::gfx {

namespace {

// Same curve as assets/shaders/block.vert brightness().
float brightness(float level, float ambient = 0.0f) {
    const float f = std::clamp(level / 15.0f, 0.0f, 1.0f);
    const float b = std::clamp(f / (4.0f - 3.0f * f) * (1.0f - ambient) + ambient, 0.0f, 1.0f);
    const float x = 1.0f - b, x2 = x * x;
    const float lifted = 1.0f - x2 * x2;
    return 0.05f + 0.95f * (b + (lifted - b) * 0.5f);
}

uint32_t pack(const glm::vec3& c, float a = 1.0f) {
    auto ch = [](float v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return ch(c.r) | (ch(c.g) << 8) | (ch(c.b) << 16) | (ch(a) << 24);
}

} // namespace

glm::vec3 lightColor(int sky, int block, float skyDarken, float ambient, bool forceBright) {
    const glm::vec3 skyPart(brightness(float(sky) - skyDarken, ambient));
    const glm::vec3 blockPart = brightness(float(block), ambient) * glm::vec3(1.0f, 0.93f, 0.82f);
    glm::vec3 light = glm::min(skyPart + blockPart, glm::vec3(1.0f));
    if (forceBright) light = glm::min(glm::mix(light, glm::vec3(0.99f, 1.12f, 1.0f), 0.25f), glm::vec3(1.0f));
    return light; // same as block.vert
}

EntityRenderer::~EntityRenderer() {
    if (m_mobTexture) glDeleteTextures(1, &m_mobTexture);
    if (m_weatherTexture) glDeleteTextures(1, &m_weatherTexture);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool EntityRenderer::init(const TextureAtlas& atlas, const BlockModels& models, const ItemIcons& icons,
                          const PackStack& packs) {
    if (!m_shader.load("entity")) return false;
    // Mob textures stacked vertically: 64 x 64 per mob type.
    {
        constexpr int n = kMobTextureRows;
        Image strip{64, 64 * n, std::vector<uint8_t>(size_t(64) * 64 * n * 4, 0)};
        for (int t = 0; t < n; ++t) {
            const char* path = mobTexturePath(t);
            const auto bytes = packs.read(path);
            const auto img = bytes ? decodePng(*bytes) : std::nullopt;
            if (!img || img->width != 64 || img->height != 64) {
                MC_LOG_WARN("Mob texture %s missing or not 64x64", path);
                continue;
            }
            std::copy(img->pixels.begin(), img->pixels.end(), strip.pixels.begin() + size_t(t) * 64 * 64 * 4);
        }
        glCreateTextures(GL_TEXTURE_2D, 1, &m_mobTexture);
        glTextureStorage2D(m_mobTexture, 1, GL_RGBA8, strip.width, strip.height);
        glTextureSubImage2D(m_mobTexture, 0, 0, 0, strip.width, strip.height, GL_RGBA, GL_UNSIGNED_BYTE,
                            strip.pixels.data());
        glTextureParameteri(m_mobTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(m_mobTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        m_mobs.reserve(size_t(kMaxQuads) * 6);
    }
    m_atlasTexture = atlas.texture();
    m_columns = atlas.columns();
    m_cell = atlas.cellSize();
    m_models = &models;
    m_icons = &icons;
    for (int i = 0; i < 10; ++i)
        m_crackSprites[i] = static_cast<uint16_t>(atlas.spriteIndex("destroy_stage_" + std::to_string(i)));
    m_orbSprite = static_cast<uint16_t>(atlas.spriteIndex("experience_orb")); // (our texture, in the block atlas)
    m_items.reserve(size_t(kMaxQuads) * 6);
    {
        // Rain and snow in one texture that repeats vertically (vanilla draws each
        // column as one long quad with a scrolling v).
        Image both{32, 16, std::vector<uint8_t>(size_t(32) * 16 * 4, 0)};
        const char* paths[2] = {"assets/minecraft/textures/environment/rain.png",
                                "assets/minecraft/textures/environment/snow.png"};
        for (int k = 0; k < 2; ++k) {
            const auto bytes = packs.read(paths[k]);
            const auto img = bytes ? decodePng(*bytes) : std::nullopt;
            if (!img || img->width <= 0 || img->height <= 0) {
                MC_LOG_WARN("Weather texture %s missing", paths[k]);
                continue;
            }
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) { // (resampled to 16x16)
                    const size_t from = (size_t(y * img->height / 16) * size_t(img->width) + size_t(x * img->width / 16)) * 4;
                    std::copy_n(img->pixels.begin() + std::ptrdiff_t(from), 4,
                                both.pixels.begin() + std::ptrdiff_t((size_t(y) * 32 + size_t(k * 16 + x)) * 4));
                }
        }
        glCreateTextures(GL_TEXTURE_2D, 1, &m_weatherTexture);
        glTextureStorage2D(m_weatherTexture, 1, GL_RGBA8, 32, 16);
        glTextureSubImage2D(m_weatherTexture, 0, 0, 0, 32, 16, GL_RGBA, GL_UNSIGNED_BYTE, both.pixels.data());
        glTextureParameteri(m_weatherTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(m_weatherTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_weatherTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_weatherTexture, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    m_boltSprite = static_cast<uint16_t>(atlas.spriteIndex("weather_bolt"));
    for (int i = 0; i < 8; ++i)
        m_particleSprites[i] = static_cast<uint16_t>(atlas.spriteIndex("particle_generic_" + std::to_string(i)));
    for (int i = 0; i < 4; ++i)
        m_particleSprites[int(ParticleSprite::Splash0) + i] =
            static_cast<uint16_t>(atlas.spriteIndex("particle_splash_" + std::to_string(i)));
    m_particleSprites[int(ParticleSprite::Flame)] = static_cast<uint16_t>(atlas.spriteIndex("particle_flame"));
    m_particleSprites[int(ParticleSprite::Lava)] = static_cast<uint16_t>(atlas.spriteIndex("particle_lava"));
    m_particleSprites[int(ParticleSprite::Crit)] = static_cast<uint16_t>(atlas.spriteIndex("particle_crit"));
    m_particleSprites[int(ParticleSprite::Effect)] = static_cast<uint16_t>(atlas.spriteIndex("particle_effect"));
    m_particleSprites[int(ParticleSprite::Drip)] = static_cast<uint16_t>(atlas.spriteIndex("particle_drip"));
    m_weather.reserve(size_t(kMaxWeatherQuads) * 6);
    m_bolts.reserve(size_t(1024) * 6);
    m_crack.reserve(36);
    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, GLsizeiptr(kMaxQuads + kMaxWeatherQuads + 1024) * 6 * sizeof(Vertex), nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, x));
    glVertexArrayAttribBinding(m_vao, 0, 0);
    glEnableVertexArrayAttrib(m_vao, 1);
    glVertexArrayAttribFormat(m_vao, 1, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, u));
    glVertexArrayAttribBinding(m_vao, 1, 0);
    glEnableVertexArrayAttrib(m_vao, 2);
    glVertexArrayAttribFormat(m_vao, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(Vertex, color));
    glVertexArrayAttribBinding(m_vao, 2, 0);
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(Vertex));
    return true;
}

void EntityRenderer::quad(const glm::vec3 (&p)[4], float u0, float v0, float u1, float v1,
                          uint32_t color, std::vector<Vertex>& out) {
    if (out.size() + 6 > out.capacity()) return; // hard cap: never reallocate per frame
    const Vertex a{p[0].x, p[0].y, p[0].z, u0, v0, color}, b{p[1].x, p[1].y, p[1].z, u0, v1, color},
        c{p[2].x, p[2].y, p[2].z, u1, v1, color}, d{p[3].x, p[3].y, p[3].z, u1, v0, color};
    out.insert(out.end(), {a, b, c, a, c, d});
}

void EntityRenderer::cube(const glm::vec3& mn, const glm::vec3& mx, const uint16_t (&sprites)[6],
                          const glm::vec3& light, const uint32_t (&tints)[6], std::vector<Vertex>& out, bool shade) {
    // Faces in world::Direction order (down, up, north, south, west, east); corners
    // top-left, bottom-left, bottom-right, top-right seen from outside.
    const glm::vec3 c[8] = {{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
                            {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}};
    static constexpr int kFaces[6][4] = {{4, 0, 1, 5}, {3, 7, 6, 2}, {2, 1, 0, 3},
                                         {7, 4, 5, 6}, {3, 0, 4, 7}, {6, 5, 1, 2}};
    static constexpr float kShade[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    for (int f = 0; f < 6; ++f) {
        const uint32_t t = tints[f];
        const glm::vec3 tint(float(t & 255) / 255.0f, float((t >> 8) & 255) / 255.0f, float((t >> 16) & 255) / 255.0f);
        const glm::vec3 p[4] = {c[kFaces[f][0]], c[kFaces[f][1]], c[kFaces[f][2]], c[kFaces[f][3]]};
        const float u0 = float(sprites[f] % m_columns) * m_cell, v0 = float(sprites[f] / m_columns) * m_cell;
        const glm::vec3 col = light * (shade ? kShade[f] : 1.0f) * tint;
        quad(p, u0, v0, u0 + m_cell, v0 + m_cell, pack(col), out);
    }
}

void EntityRenderer::addItem(const world::ItemStack& stack, const glm::dvec3& pos, float spin, float bob,
                             const glm::vec3& light, const glm::dvec3& cameraPos) {
    if (stack.empty()) return;
    const world::ItemDef& def = world::itemRegistry().item(stack.item);
    const glm::vec3 base(pos - cameraPos);
    const float cs = std::cos(spin), sn = std::sin(spin);
    auto rot = [&](float x, float y, float z) { // spin about the item's vertical axis
        return base + glm::vec3(x * cs - z * sn, y + bob, x * sn + z * cs);
    };
    const uint16_t sprite = m_icons->sprite(stack.item);
    if (def.block && !sprite) {
        const world::BlockStateId state = stack.state ? stack.state : world::blockRegistry().defaultState(def.block);
        const BakedModel& m = (*m_models)[state];
        if (!m.visible) return;
        if (m.cross || m.boxCount) { // plants, torches: their sprite, flat
            const uint16_t s = m.cross ? m.crossSprite : m.boxes[0].faces[2].sprite;
            const float u0 = float(s % m_columns) * m_cell, v0 = float(s / m_columns) * m_cell;
            const glm::vec3 p[4] = {rot(-0.25f, 0.5f, 0), rot(-0.25f, 0, 0), rot(0.25f, 0, 0), rot(0.25f, 0.5f, 0)};
            const glm::vec3 q[4] = {p[3], p[2], p[1], p[0]};
            quad(p, u0, v0, u0 + m_cell, v0 + m_cell, pack(light), m_items);
            quad(q, u0 + m_cell, v0, u0, v0 + m_cell, pack(light), m_items);
            return;
        }
        // A 0.25 cube; rotation about Y applied to its corners.
        uint16_t sprites[6];
        uint32_t tints[6];
        for (int f = 0; f < 6; ++f) {
            const BakedFace& face = m.variants[0].faces[f];
            sprites[f] = face.sprite;
            // Item colours (wiki: Grass Block #7CBD6B; leaves: plains foliage #77AB2F),
            // stored R | G << 8 | B << 16.
            tints[f] = face.tint == Tint::Grass ? 0x6BBD7Cu : face.tint == Tint::Foliage ? 0x2FAB77u : 0xFFFFFFu;
        }
        const size_t start = m_items.size();
        cube({-0.125f, 0.0f, -0.125f}, {0.125f, 0.25f, 0.125f}, sprites, light, tints, m_items, true);
        for (size_t i = start; i < m_items.size(); ++i) {
            Vertex& v = m_items[i];
            const glm::vec3 r = rot(v.x, v.y, v.z);
            v.x = r.x;
            v.y = r.y;
            v.z = r.z;
        }
        return;
    }
    const float u0 = float(sprite % m_columns) * m_cell, v0 = float(sprite / m_columns) * m_cell;
    const glm::vec3 p[4] = {rot(-0.25f, 0.5f, 0), rot(-0.25f, 0, 0), rot(0.25f, 0, 0), rot(0.25f, 0.5f, 0)};
    const glm::vec3 q[4] = {p[3], p[2], p[1], p[0]}; // back side
    quad(p, u0, v0, u0 + m_cell, v0 + m_cell, pack(light), m_items);
    quad(q, u0 + m_cell, v0, u0, v0 + m_cell, pack(light), m_items);
}

void EntityRenderer::addBlock(world::BlockStateId state, const glm::dvec3& pos, const glm::vec3& light,
                              const glm::dvec3& cameraPos) {
    const BakedModel& m = (*m_models)[state];
    if (!m.visible || m.cross || m.boxCount) return; // (only cube blocks fall)
    uint16_t sprites[6];
    uint32_t tints[6];
    for (int f = 0; f < 6; ++f) {
        sprites[f] = m.variants[0].faces[f].sprite;
        tints[f] = 0xFFFFFFu;
    }
    const glm::vec3 base(pos - cameraPos);
    cube(base + glm::vec3(-0.5f, 0.0f, -0.5f), base + glm::vec3(0.5f, 1.0f, 0.5f), sprites, light, tints, m_items, true);
}

void EntityRenderer::addOrb(const glm::dvec3& pos, int value, float time, const glm::dvec3& cameraPos) {
    // Faces the camera; pulses between green and yellow (vanilla's orb shimmer).
    const float size = value >= 37 ? 0.35f : value >= 7 ? 0.25f : 0.18f;
    const glm::vec3 c(pos - cameraPos + glm::dvec3(0, size, 0));
    const glm::vec3 toCam = glm::length(c) > 1e-4f ? -glm::normalize(c) : glm::vec3(0, 0, 1);
    const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), toCam)) * size;
    const glm::vec3 up = glm::cross(toCam, right);
    const float pulse = 0.5f + 0.5f * std::sin(time * 0.3f);
    const uint32_t color = pack(glm::vec3(0.5f + 0.5f * pulse, 1.0f, 0.2f));
    const float u0 = float(m_orbSprite % m_columns) * m_cell, v0 = float(m_orbSprite / m_columns) * m_cell;
    const glm::vec3 p[4] = {c - right + up, c - right - up, c + right - up, c + right + up};
    quad(p, u0, v0, u0 + m_cell, v0 + m_cell, color, m_items);
}

void EntityRenderer::addBeam(const glm::dvec3& from, const glm::dvec3& to, const glm::dvec3& cameraPos) {
    const glm::dvec3 d = to - from;
    const double len = glm::length(d);
    if (len < 1e-3) return;
    const glm::vec3 f(d / len);
    const glm::vec3 helper = std::abs(f.y) > 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    const glm::vec3 a = glm::normalize(glm::cross(f, helper)), b = glm::cross(f, a);
    const glm::vec3 s(from - cameraPos), e(to - cameraPos);
    const uint32_t color = pack(glm::vec3(1.0f, 0.55f, 0.95f));
    const float u0 = float(m_orbSprite % m_columns) * m_cell + m_cell * 0.45f, v0 = float(m_orbSprite / m_columns) * m_cell;
    const float u1 = u0 + m_cell * 0.1f, v1 = v0 + m_cell;
    for (const glm::vec3& side : {a, b}) {
        const glm::vec3 w = side * 0.08f;
        const glm::vec3 p[4] = {s + w, s - w, e - w, e + w};
        const glm::vec3 q[4] = {p[3], p[2], p[1], p[0]};
        quad(p, u0, v0, u1, v1, color, m_items);
        quad(q, u0, v0, u1, v1, color, m_items);
    }
}

void EntityRenderer::addPrecipitation(int32_t x, int32_t z, int y0, int y1, bool snow, float scroll, float drift,
                                      float alpha, const glm::vec3& light, const glm::dvec3& cameraPos) {
    if (y1 <= y0 || m_weather.size() + 6 > m_weather.capacity()) return;
    // The quad faces the camera around the vertical axis through the column's centre
    // (vanilla turns each column's quad toward the viewer).
    const glm::vec3 c(float(double(x) + 0.5 - cameraPos.x), 0.0f, float(double(z) + 0.5 - cameraPos.z));
    const float len = std::sqrt(c.x * c.x + c.z * c.z);
    const glm::vec3 side = len > 1e-3f ? glm::vec3(-c.z / len, 0.0f, c.x / len) * 0.5f : glm::vec3(0.5f, 0, 0);
    // Texels: u is the rain (0..16) or snow (16..32) half; v repeats every 16 texels
    // (one block) and grows as the texture slides down.
    const float u0 = snow ? 16.0f : 0.0f, u1 = u0 + 16.0f;
    const float vt = (-scroll - float(y1)) * 16.0f, vb = (-scroll - float(y0)) * 16.0f; // (features fall)
    const float yt = float(double(y1) - cameraPos.y), yb = float(double(y0) - cameraPos.y);
    const glm::vec3 base = c + side * 2.0f * drift; // (snow slides sideways)
    const uint32_t color = pack(light, alpha);
    const Vertex v[4] = {{base.x - side.x, yt, base.z - side.z, u0, vt, color},
                         {base.x - side.x, yb, base.z - side.z, u0, vb, color},
                         {base.x + side.x, yb, base.z + side.z, u1, vb, color},
                         {base.x + side.x, yt, base.z + side.z, u1, vt, color}};
    for (const int k : {0, 1, 2, 0, 2, 3})
        m_weather.push_back(v[k]);
}

void EntityRenderer::addLightning(const glm::dvec3& ground, uint32_t seed, const glm::dvec3& cameraPos) {
    // 8 legs of 16 blocks each step sideways up to 4 blocks (wiki: Lightning - a
    // jagged bolt with branches), from 128 blocks up down to the strike.
    uint32_t r = seed * 747796405u + 2891336453u;
    auto next = [&] {
        r = r * 1664525u + 1013904223u;
        return float((r >> 8) & 0xFFFF) / 65535.0f - 0.5f;
    };
    const float u = float(m_boltSprite % m_columns) * m_cell + m_cell * 0.5f,
                v = float(m_boltSprite / m_columns) * m_cell + m_cell * 0.5f;
    auto segment = [&](const glm::dvec3& a, const glm::dvec3& b, float width) {
        if (m_bolts.size() + 12 > m_bolts.capacity()) return;
        const glm::vec3 s(a - cameraPos), e(b - cameraPos);
        const glm::vec3 mid = (s + e) * 0.5f;
        const glm::vec3 toCam = glm::length(mid) > 1e-3f ? -glm::normalize(mid) : glm::vec3(0, 0, 1);
        const glm::vec3 dir = glm::normalize(e - s);
        glm::vec3 w = glm::cross(dir, toCam);
        w = glm::length(w) > 1e-4f ? glm::normalize(w) * width : glm::vec3(width, 0, 0);
        for (const float scale : {1.0f, 2.5f}) { // a bright core and a fainter glow
            const uint32_t color = pack(glm::vec3(0.45f, 0.45f, 0.7f) * (scale > 1.0f ? 0.4f : 1.0f), 1.0f);
            const glm::vec3 p[4] = {s - w * scale, e - w * scale, e + w * scale, s + w * scale};
            Vertex vtx[4];
            for (int k = 0; k < 4; ++k)
                vtx[k] = {p[k].x, p[k].y, p[k].z, u, v, color};
            for (const int k : {0, 1, 2, 0, 2, 3})
                m_bolts.push_back(vtx[k]);
        }
    };
    glm::dvec3 points[9];
    points[8] = ground;
    for (int i = 7; i >= 0; --i)
        points[i] = points[i + 1] + glm::dvec3(next() * 8.0, 16.0, next() * 8.0);
    for (int i = 0; i < 8; ++i)
        segment(points[i], points[i + 1], 0.12f);
    for (int b = 0; b < 3; ++b) { // branches from the upper legs, fading out
        glm::dvec3 p = points[1 + b * 2];
        for (int i = 0; i < 3; ++i) {
            const glm::dvec3 q = p + glm::dvec3(next() * 10.0, -8.0, next() * 10.0);
            segment(p, q, 0.08f);
            p = q;
        }
    }
}

void EntityRenderer::addParticle(const glm::dvec3& pos, float size, ParticleSprite sprite, world::BlockStateId state,
                                 uint8_t u, uint8_t v, const glm::vec3& color, const glm::dvec3& cameraPos,
                                 const glm::vec3& right, const glm::vec3& up) {
    float u0, v0, u1, v1;
    if (sprite == ParticleSprite::Terrain) {
        // The block's particle texture: its first visible face (side for cubes).
        const BakedModel& m = (*m_models)[state];
        const uint16_t s = m.cross            ? m.crossSprite
                           : m.boxCount > 0 ? m.boxes[0].faces[int(world::Direction::North)].sprite
                                            : m.variants[0].faces[int(world::Direction::North)].sprite;
        const float q = float(m_cell) / 4.0f;
        u0 = float(s % m_columns) * m_cell + float(u) * q;
        v0 = float(s / m_columns) * m_cell + float(v) * q;
        u1 = u0 + q;
        v1 = v0 + q;
    } else {
        const uint16_t s = m_particleSprites[int(sprite)];
        u0 = float(s % m_columns) * m_cell;
        v0 = float(s / m_columns) * m_cell;
        u1 = u0 + float(m_cell);
        v1 = v0 + float(m_cell);
    }
    const glm::vec3 c(pos - cameraPos), r = right * size, w = up * size;
    const glm::vec3 q[4] = {c - r + w, c - r - w, c + r - w, c + r + w};
    quad(q, u0, v0, u1, v1, pack(color), m_items);
}

void EntityRenderer::addCloud(const glm::dvec3& centre, float radius, float time, const glm::dvec3& cameraPos) {
    // 13 puffs: the middle and two rings, drifting slowly.
    for (int i = 0; i < 13; ++i) {
        const float ring = i == 0 ? 0.0f : i <= 4 ? 0.45f : 0.85f;
        const float a = float(i) * 2.399f + time * 0.01f;
        const glm::dvec3 p = centre + glm::dvec3(std::cos(a) * ring * radius, 0.3 + 0.2 * std::sin(time * 0.05f + float(i)),
                                                 std::sin(a) * ring * radius);
        const glm::vec3 c(p - cameraPos);
        const glm::vec3 toCam = glm::length(c) > 1e-4f ? -glm::normalize(c) : glm::vec3(0, 0, 1);
        const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), toCam)) * 0.6f;
        const glm::vec3 up = glm::cross(toCam, right);
        const uint32_t color = pack(glm::vec3(0.75f, 0.3f, 0.95f));
        const float u0 = float(m_orbSprite % m_columns) * m_cell, v0 = float(m_orbSprite / m_columns) * m_cell;
        const glm::vec3 q[4] = {c - right + up, c - right - up, c + right - up, c + right + up};
        quad(q, u0, v0, u0 + m_cell, v0 + m_cell, color, m_items);
    }
}

void EntityRenderer::addArrow(const glm::dvec3& tip, const glm::dvec3& dir, const glm::vec3& light,
                              const glm::dvec3& cameraPos) {
    const double len = glm::length(dir);
    const glm::vec3 f = len > 1e-9 ? glm::vec3(dir / len) : glm::vec3(0, -1, 0);
    const glm::vec3 helper = std::abs(f.y) > 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    const glm::vec3 a = glm::normalize(glm::cross(f, helper)), b = glm::cross(f, a);
    const glm::vec3 t(tip - cameraPos);
    const glm::vec3 tail = t - f * 0.5f; // 16 px long, 5 px wide
    const float u0 = 0.0f, u1 = 16.0f, v0 = float(kProjectileRow * 64), v1 = v0 + 5.0f;
    const uint32_t c = pack(light);
    for (const glm::vec3& side : {a, b}) {
        const glm::vec3 w = side * (2.5f / 32.0f);
        const glm::vec3 p[4] = {tail + w, tail - w, t - w, t + w};
        const glm::vec3 q[4] = {p[3], p[2], p[1], p[0]}; // back side
        quad(p, u0, v0, u1, v1, c, m_mobs);
        quad(q, u1, v0, u0, v1, c, m_mobs);
    }
}

void EntityRenderer::addMob(const world::MobData& mob, const glm::dvec3& pos, float bodyYaw, float headYaw,
                            float pitch, const glm::vec3& light, const glm::dvec3& cameraPos) {
    constexpr float kDeg = 3.14159265f / 180.0f;
    // Rotations as matrices, built once per mob / part (same conventions as vanilla's
    // model parts: x pitches, y turns, z rolls).
    auto rotX = [](float a) {
        const float c = std::cos(a), s = std::sin(a);
        return glm::mat3(1, 0, 0, 0, c, s, 0, -s, c);
    };
    auto rotY = [](float a) {
        const float c = std::cos(a), s = std::sin(a);
        return glm::mat3(c, 0, -s, 0, 1, 0, s, 0, c);
    };
    auto rotZ = [](float a) {
        const float c = std::cos(a), s = std::sin(a);
        return glm::mat3(c, s, 0, -s, c, 0, 0, 0, 1);
    };
    const float swing = std::cos(mob.limbSwing * 0.6662f) * 1.4f * mob.limbSwingAmount;
    // Body: vanilla yaw turns from +Z towards -X; dying tips over sideways over 20 ticks.
    glm::mat3 body = rotY(-bodyYaw * kDeg);
    if (mob.deathTime > 0 && mob.type != world::MobType::EnderDragon) body = body * rotZ(std::min(1.0f, float(mob.deathTime) / 20.0f) * 90.0f * kDeg);
    const glm::mat3 head = rotY((headYaw - bodyYaw) * kDeg) * rotX(-pitch * kDeg);
    const glm::mat3 legA = rotX(swing), legB = rotX(-swing), arm = rotX(-90.0f * kDeg + swing * 0.2f);
    const float flap = std::sin(mob.limbSwing) * 0.6f; // (dragon wings)
    const glm::mat3 wingL = rotZ(flap), wingR = rotZ(-flap);
    const bool red = mob.hurtTime > 0 || mob.deathTime > 0;
    const glm::vec3 base(pos - cameraPos);
    const float vrow = float(gfx::mobTextureRow(mob.type) * 64);
    const glm::vec3 tint = red ? glm::vec3(1.0f, 0.45f, 0.45f) : glm::vec3(1.0f);
    // Box corners: bit 0 = max x, bit 1 = max y, bit 2 = max z. Faces list their
    // corners TL, BL, BR, TR as seen from outside, with the box-UV region and shade.
    struct Face {
        uint8_t c[4];
        float shade;
    };
    static constexpr Face kFaces[6] = {
        {{6, 4, 5, 7}, 0.8f}, // front +Z
        {{3, 1, 0, 2}, 0.8f}, // back -Z
        {{2, 0, 4, 6}, 0.6f}, // right -X
        {{7, 5, 1, 3}, 0.6f}, // left +X
        {{2, 6, 7, 3}, 1.0f}, // top
        {{4, 0, 1, 5}, 0.5f}, // bottom
    };
    float scale = mob.age < 0 ? 0.5f : 1.0f; // babies: half size
    scale *= world::mobInfo(mob.type).modelScale; // (ghasts: 4.5)
    if (mob.type == world::MobType::MagmaCube || mob.type == world::MobType::Slime)
        scale *= float(mob.size); // its model is the size-1 cube
    glm::vec3 flash(0.0f);
    if (mob.fuse > 0) { // a swelling creeper grows and flashes white (wiki: Creeper)
        const float f = std::min(1.0f, float(mob.fuse) / 30.0f);
        scale *= 1.0f + f * 0.2f;
        if ((mob.fuse / 3) % 2 == 1) flash = glm::vec3(0.6f * f);
    }
    for (const MobPart& part : mobModel(mob.type)) {
        if (part.layer == 1 && mob.sheared) continue;
        if (part.layer == 2 && !mob.showBottom) continue;
        glm::vec3 mn(part.from[0], part.from[1], part.from[2]), mx(part.to[0], part.to[1], part.to[2]);
        const glm::vec3 pivot(part.pivot[0], part.pivot[1], part.pivot[2]);
        const float w = mx.x - mn.x, h = mx.y - mn.y, d = mx.z - mn.z; // UV size (before inflating)
        mn -= glm::vec3(part.inflate);
        mx += glm::vec3(part.inflate);
        const float u = float(part.u), v = float(part.v) + (part.layer == 1 ? float(kSheepWoolRow * 64) : vrow);
        glm::vec3 partTint = tint;
        if (part.layer == 1) {
            const uint32_t c = kWoolColours[mob.woolColour & 15];
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        const glm::mat3* anim = part.anim == MobPart::Anim::Head         ? &head
                                : part.anim == MobPart::Anim::LegA       ? &legA
                                : part.anim == MobPart::Anim::LegB       ? &legB
                                : part.anim == MobPart::Anim::ArmForward ? &arm
                                : part.anim == MobPart::Anim::WingL      ? &wingL
                                : part.anim == MobPart::Anim::WingR      ? &wingR
                                                                         : nullptr;
        glm::vec3 corners[8];
        for (int i = 0; i < 8; ++i) {
            glm::vec3 c(i & 1 ? mx.x : mn.x, i & 2 ? mx.y : mn.y, i & 4 ? mx.z : mn.z);
            if (anim) c = *anim * (c - pivot) + pivot;
            if (part.anim == MobPart::Anim::Lift) c.y += float(mob.peek) * 0.08f;
            corners[i] = base + body * c * (scale / 16.0f); // pixels -> blocks
        }
        const float uv[6][4] = {
            {u + d, v + d, w, h}, {u + 2 * d + w, v + d, w, h}, {u, v + d, d, h},
            {u + d + w, v + d, d, h}, {u + d, v, w, d}, {u + d + w, v, w, d},
        };
        for (int f = 0; f < 6; ++f) {
            const Face& face = kFaces[f];
            const glm::vec3 p[4] = {corners[face.c[0]], corners[face.c[1]], corners[face.c[2]], corners[face.c[3]]};
            quad(p, uv[f][0], uv[f][1], uv[f][0] + uv[f][2], uv[f][1] + uv[f][3],
                 pack(glm::min(light * partTint * face.shade + flash, glm::vec3(1.0f))),
                 m_mobs);
        }
    }
}

void EntityRenderer::setCrack(const world::BlockPos& block, int stage) {
    m_crackBlock = block;
    m_crackStage = std::clamp(stage, 0, 9);
}

void EntityRenderer::draw(const Camera& camera, float aspect) {
    m_crack.clear();
    if (m_crackStage >= 0) {
        const glm::vec3 o(glm::dvec3(m_crackBlock.x, m_crackBlock.y, m_crackBlock.z) - camera.position);
        constexpr float g = 0.002f; // slightly larger than the block: no z-fighting
        uint16_t s[6];
        std::fill(std::begin(s), std::end(s), m_crackSprites[m_crackStage]);
        static constexpr uint32_t kWhite[6] = {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF};
        cube(o - glm::vec3(g), o + glm::vec3(1.0f + g), s, glm::vec3(1.0f), kWhite, m_crack, false);
    }
    // Everything shares one buffer: items first, then the crack, then mobs (saturating).
    const size_t cap = size_t(kMaxQuads) * 6, items = std::min(m_items.size(), cap),
                 crack = std::min(m_crack.size(), cap - items),
                 mobs = std::min(m_mobs.size(), cap - items - crack);
    const size_t weatherBase = cap, weather = std::min(m_weather.size(), size_t(kMaxWeatherQuads) * 6),
                 bolts = std::min(m_bolts.size(), size_t(1024) * 6);
    if (weather) glNamedBufferSubData(m_vbo, GLintptr(weatherBase * sizeof(Vertex)), GLsizeiptr(weather * sizeof(Vertex)), m_weather.data());
    if (bolts)
        glNamedBufferSubData(m_vbo, GLintptr((weatherBase + weather) * sizeof(Vertex)), GLsizeiptr(bolts * sizeof(Vertex)),
                             m_bolts.data());
    if (items + crack + mobs + weather + bolts == 0) return;
    glNamedBufferSubData(m_vbo, 0, GLsizeiptr(items * sizeof(Vertex)), m_items.data());
    if (crack)
        glNamedBufferSubData(m_vbo, GLintptr(items * sizeof(Vertex)), GLsizeiptr(crack * sizeof(Vertex)),
                             m_crack.data());
    if (mobs)
        glNamedBufferSubData(m_vbo, GLintptr((items + crack) * sizeof(Vertex)), GLsizeiptr(mobs * sizeof(Vertex)),
                             m_mobs.data());
    m_shader.bind();
    glBindVertexArray(m_vao);
    const glm::mat4 vp = camera.viewProjectionAtOrigin(aspect);
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(vp));
    glBindTextureUnit(0, m_atlasTexture);
    glDisable(GL_CULL_FACE);
    if (items) {
        glUniform1f(1, 0.1f); // alpha cutoff
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(items));
    }
    if (mobs) {
        glUniform1f(1, 0.1f);
        glBindTextureUnit(0, m_mobTexture);
        glDrawArrays(GL_TRIANGLES, GLint(items + crack), GLsizei(mobs));
        glBindTextureUnit(0, m_atlasTexture);
    }
    if (crack) { // vanilla crumbling: multiply the block underneath
        glUniform1f(1, 0.01f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, GLint(items), GLsizei(crack));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    if (weather || bolts) { // after the world and entities, blended, no depth writes
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glUniform1f(1, 0.01f);
        if (weather) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindTextureUnit(0, m_weatherTexture);
            glDrawArrays(GL_TRIANGLES, GLint(weatherBase), GLsizei(weather));
            glBindTextureUnit(0, m_atlasTexture);
        }
        if (bolts) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); // lightning glows (vanilla: additive)
            glDrawArrays(GL_TRIANGLES, GLint(weatherBase + weather), GLsizei(bolts));
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glEnable(GL_CULL_FACE);
    m_items.clear();
    m_mobs.clear();
    m_weather.clear();
    m_bolts.clear();
}

} // namespace mc::gfx
