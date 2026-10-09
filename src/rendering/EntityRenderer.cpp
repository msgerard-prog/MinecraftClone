#include "rendering/EntityRenderer.h"

#include "world/Banners.h"
#include "world/Paintings.h"

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
    if (m_fontTexture) glDeleteTextures(1, &m_fontTexture);
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
    m_frameSprite = static_cast<uint16_t>(atlas.spriteIndex("item_frame"));
    m_glowFrameSprite = static_cast<uint16_t>(atlas.spriteIndex("glow_item_frame"));
    m_frameWood = static_cast<uint16_t>(atlas.spriteIndex("birch_planks"));
    m_paintingBack = static_cast<uint16_t>(atlas.spriteIndex("painting/back"));
    m_bannerMasks.clear();
    for (size_t i = 0; i <= world::kBannerPatterns.size(); ++i) {
        const std::string name = i < world::kBannerPatterns.size() ? std::string(world::kBannerPatterns[i].name) : "base";
        m_bannerMasks.push_back({static_cast<uint16_t>(atlas.spriteIndex("clone_banner/" + name + ":0,0")),
                                 static_cast<uint16_t>(atlas.spriteIndex("clone_banner/" + name + ":0,1"))});
    }
    m_bannerWood = static_cast<uint16_t>(atlas.spriteIndex("oak_planks"));
    m_paintingTiles.clear();
    for (const world::PaintingVariant& v : world::kPaintings) {
        std::vector<uint16_t>& tiles = m_paintingTiles.emplace_back();
        for (int y = 0; y < v.height; ++y)
            for (int x = 0; x < v.width; ++x)
                tiles.push_back(static_cast<uint16_t>(
                    atlas.spriteIndex("painting/" + std::string(v.name) + ":" + std::to_string(x) + "," + std::to_string(y))));
    }
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
    m_particleSprites[int(ParticleSprite::Note)] = static_cast<uint16_t>(atlas.spriteIndex("particle_note"));
    m_weather.reserve(size_t(kMaxWeatherQuads) * 6);
    m_text.reserve(size_t(4096) * 6);
    { // the font sheet for sign text (as the GUI's)
        const auto bytes = packs.read("assets/minecraft/textures/font/ascii.png");
        const auto img = bytes ? decodePng(*bytes) : std::nullopt;
        if (img && img->width == img->height) {
            m_font = FontMetrics::fromImage(img->pixels.data(), img->width);
            glCreateTextures(GL_TEXTURE_2D, 1, &m_fontTexture);
            glTextureStorage2D(m_fontTexture, 1, GL_RGBA8, img->width, img->height);
            glTextureSubImage2D(m_fontTexture, 0, 0, 0, img->width, img->height, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels.data());
            glTextureParameteri(m_fontTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(m_fontTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } else {
            MC_LOG_WARN("Entity: no font sheet - signs show no text");
        }
    }
    m_bolts.reserve(size_t(1024) * 6);
    m_crack.reserve(36);
    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, GLsizeiptr(kMaxQuads + kMaxWeatherQuads + 1024 + 4096) * 6 * sizeof(Vertex), nullptr,
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

namespace {
// A wall's in-plane axes for a facing (seen from in front): right and up.
void wallAxes(int facing, glm::vec3& n, glm::vec3& r, glm::vec3& u) {
    n = glm::vec3(world::kDirectionNormals[facing % 6]);
    u = facing == int(world::Direction::Up) ? glm::vec3(0, 0, -1) : facing == int(world::Direction::Down) ? glm::vec3(0, 0, 1)
                                                                                                          : glm::vec3(0, 1, 0);
    r = glm::cross(u, n);
}
} // namespace

void EntityRenderer::addItemFrame(const glm::dvec3& centre, int facing, bool glow, const world::ItemStack& item,
                                  int rotation, const glm::vec3& light, const glm::dvec3& cameraPos) {
    glm::vec3 n, r, u;
    wallAxes(facing, n, r, u);
    const glm::vec3 c(centre - cameraPos);
    const glm::vec3 half = (glm::vec3(1.0f) - glm::abs(n)) * 0.375f + glm::abs(n) * (1.0f / 32.0f);
    uint16_t sprites[6];
    uint32_t tints[6];
    for (int f = 0; f < 6; ++f) {
        sprites[f] = f == facing % 6 ? (glow ? m_glowFrameSprite : m_frameSprite) : m_frameWood;
        tints[f] = 0xFFFFFFu;
    }
    // (glow item frames light their item: drawn at full brightness - wiki: Glow Item Frame)
    cube(c - half, c + half, sprites, light, tints, m_items, true);
    if (item.empty()) return;
    // The item: its sprite (or a block's face) flat on the frame, half a block across.
    const uint16_t sprite = itemSprite(item);
    if (!sprite) return;
    const float a = float(rotation % 8) * 0.785398f, cs = std::cos(a), sn = std::sin(a);
    const glm::vec3 rr = r * cs - u * sn, uu = u * cs + r * sn;
    const glm::vec3 o = c + n * (1.0f / 32.0f + 0.004f);
    const float s = 0.25f;
    const glm::vec3 p[4] = {o - rr * s + uu * s, o - rr * s - uu * s, o + rr * s - uu * s, o + rr * s + uu * s};
    const float u0 = float(sprite % m_columns) * m_cell, v0 = float(sprite / m_columns) * m_cell;
    quad(p, u0, v0, u0 + m_cell, v0 + m_cell, pack(glow ? glm::vec3(1.0f) : light), m_items);
}

uint16_t EntityRenderer::itemSprite(const world::ItemStack& item) const {
    if (const uint16_t s = m_icons->sprite(item.item)) return s;
    const world::ItemDef& def = world::itemRegistry().item(item.item);
    if (!def.block) return 0;
    const BakedModel& m = (*m_models)[item.state ? item.state : world::blockRegistry().defaultState(def.block)];
    if (!m.visible) return 0;
    return m.cross ? m.crossSprite : m.boxCount ? m.boxes[0].faces[2].sprite : m.variants[0].faces[2].sprite;
}

void EntityRenderer::addBanner(const glm::dvec3& cell, bool wall, int turn, int base, const world::BannerLayers& layers,
                               const glm::vec3& light, const glm::dvec3& cameraPos) {
    // Which way the cloth faces, and its right as seen from in front.
    glm::vec3 out;
    if (wall) {
        out = glm::vec3(world::kDirectionNormals[turn % 6]);
    } else {
        const float a = float(turn & 15) * 22.5f * 0.0174533f;
        out = glm::vec3(-std::sin(a), 0.0f, std::cos(a));
    }
    const glm::vec3 up(0.0f, 1.0f, 0.0f), right(out.z, 0.0f, -out.x);
    const glm::vec3 centre = glm::vec3(cell - cameraPos) + glm::vec3(0.5f, 0.0f, 0.5f);
    const float px = 1.0f / 16.0f;
    // The wood: a pole and crossbar (standing) or a bar against the wall.
    uint16_t sprites[6];
    uint32_t tints[6];
    for (int f = 0; f < 6; ++f) {
        sprites[f] = m_bannerWood;
        tints[f] = 0xFFFFFFu;
    }
    glm::vec3 barCentre, flagTop;
    if (wall) {
        const glm::vec3 back = centre - out * (0.5f - 1.5f * px);
        barCentre = back + up * (15.0f * px);
        flagTop = back + out * (0.5f * px) + up * (14.0f * px);
    } else {
        cube(centre + glm::vec3(-px, 0.0f, -px), centre + glm::vec3(px, 42.0f * px, px), sprites, light, tints, m_items, true);
        barCentre = centre + up * (43.0f * px);
        flagTop = centre + out * (1.5f * px) + up * (42.0f * px);
    }
    const glm::vec3 bh = glm::abs(right) * (10.0f * px) + up * px + glm::abs(out) * px;
    cube(barCentre - bh, barCentre + bh, sprites, light, tints, m_items, true);
    // The cloth: 20 x 40 pixels in two halves, the base colour then each layer in its dye.
    const float hw = 10.0f * px, hh = 20.0f * px;
    auto layer = [&](int mask, int dye, float lift) {
        const uint32_t c = kWoolColours[dye & 15];
        const glm::vec3 tint = light * glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        for (int half = 0; half < 2; ++half) {
            const glm::vec3 o = flagTop - up * (hh * 0.5f + float(half) * hh) + out * lift;
            const uint16_t s = m_bannerMasks[size_t(mask)][size_t(half)];
            const float u0 = float(s % m_columns) * m_cell, v0 = float(s / m_columns) * m_cell;
            const glm::vec3 front[4] = {o - right * hw + up * (hh * 0.5f), o - right * hw - up * (hh * 0.5f),
                                        o + right * hw - up * (hh * 0.5f), o + right * hw + up * (hh * 0.5f)};
            quad(front, u0, v0, u0 + m_cell, v0 + m_cell, pack(tint), m_items);
            const glm::vec3 b = -out * (2.0f * lift); // (the back: mirrored, as vanilla shows it)
            const glm::vec3 backQ[4] = {front[3] + b, front[2] + b, front[1] + b, front[0] + b};
            quad(backQ, u0 + m_cell, v0, u0, v0 + m_cell, pack(tint * 0.8f), m_items);
        }
    };
    const int baseMask = int(world::kBannerPatterns.size());
    layer(baseMask, base, 0.001f);
    for (int i = 0; i < layers.count && i < world::BannerLayers::kMax; ++i)
        layer(layers.pattern[size_t(i)] % baseMask, layers.colour[size_t(i)], 0.001f * float(i + 2));
}

void EntityRenderer::addKnot(const glm::dvec3& pos, const glm::dvec3& cameraPos) {
    const glm::vec3 c(pos - cameraPos);
    uint16_t sprites[6];
    uint32_t tints[6];
    for (int f = 0; f < 6; ++f) {
        sprites[f] = m_frameWood;
        tints[f] = 0x305A8Cu; // (rope brown: R | G << 8 | B << 16)
    }
    cube(c + glm::vec3(-0.1875f, 0.0f, -0.1875f), c + glm::vec3(0.1875f, 0.5f, 0.1875f), sprites, glm::vec3(1.0f), tints,
         m_items, true);
}

void EntityRenderer::addPainting(int variant, const glm::dvec3& centre, int facing, const glm::vec3& light,
                                 const glm::dvec3& cameraPos) {
    if (variant < 0 || variant >= int(m_paintingTiles.size())) return;
    const world::PaintingVariant& v = world::kPaintings[size_t(variant)];
    glm::vec3 n, r, u;
    wallAxes(facing, n, r, u);
    const glm::vec3 c = glm::vec3(centre - cameraPos);
    const uint32_t col = pack(light * (facing % 6 >= 4 ? 0.6f : 0.8f)); // (directional shade, as blocks)
    for (int ty = 0; ty < v.height; ++ty)
        for (int tx = 0; tx < v.width; ++tx) {
            const glm::vec3 o = c + n * (1.0f / 32.0f) + r * (float(tx) - (v.width - 1) / 2.0f) +
                                u * ((v.height - 1) / 2.0f - float(ty));
            const glm::vec3 p[4] = {o - r * 0.5f + u * 0.5f, o - r * 0.5f - u * 0.5f, o + r * 0.5f - u * 0.5f,
                                    o + r * 0.5f + u * 0.5f};
            const uint16_t sprite = m_paintingTiles[size_t(variant)][size_t(ty * v.width + tx)];
            const float u0 = float(sprite % m_columns) * m_cell, v0 = float(sprite / m_columns) * m_cell;
            quad(p, u0, v0, u0 + m_cell, v0 + m_cell, col, m_items);
        }
    // The back of the canvas (seen only from inside the wall).
    const glm::vec3 b = c - n * (1.0f / 32.0f);
    const glm::vec3 hr = r * (v.width / 2.0f), hu = u * (v.height / 2.0f);
    const glm::vec3 q[4] = {b + hr + hu, b + hr - hu, b - hr - hu, b - hr + hu};
    const float u0 = float(m_paintingBack % m_columns) * m_cell, v0 = float(m_paintingBack / m_columns) * m_cell;
    quad(q, u0, v0, u0 + m_cell, v0 + m_cell, pack(light * 0.6f), m_items);
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

void EntityRenderer::addBeam(const glm::dvec3& from, const glm::dvec3& to, const glm::dvec3& cameraPos,
                             const glm::vec3& colour, float halfWidth) {
    const glm::dvec3 d = to - from;
    const double len = glm::length(d);
    if (len < 1e-3) return;
    const glm::vec3 f(d / len);
    const glm::vec3 helper = std::abs(f.y) > 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    const glm::vec3 a = glm::normalize(glm::cross(f, helper)), b = glm::cross(f, a);
    const glm::vec3 s(from - cameraPos), e(to - cameraPos);
    const uint32_t color = pack(colour);
    const float u0 = float(m_orbSprite % m_columns) * m_cell + m_cell * 0.45f, v0 = float(m_orbSprite / m_columns) * m_cell;
    const float u1 = u0 + m_cell * 0.1f, v1 = v0 + m_cell;
    for (const glm::vec3& side : {a, b}) {
        const glm::vec3 w = side * halfWidth;
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

void EntityRenderer::addText(std::string_view text, const glm::dvec3& centre, const glm::vec3& right, const glm::vec3& up,
                             float pixel, uint32_t rgb, const glm::dvec3& cameraPos) {
    if (!m_fontTexture || text.empty()) return;
    // Width in font pixels (vanilla advances), then glyph quads left to right.
    int width = 0;
    for (const char c : text)
        width += m_font.advance[uint8_t(c)];
    const glm::vec3 c(centre - cameraPos);
    glm::vec3 pen = c - right * (float(width) * pixel * 0.5f) + up * (4.0f * pixel);
    const float cell = float(m_font.cell);
    const uint32_t colour = pack(glm::vec3(float((rgb >> 16) & 255), float((rgb >> 8) & 255), float(rgb & 255)) / 255.0f);
    for (const char ch : text) {
        const uint8_t code = uint8_t(ch);
        if (ch != ' ' && m_text.size() + 6 <= m_text.capacity()) {
            const float u = float(code % 16) * cell, v = float(code / 16) * cell;
            const glm::vec3 r8 = right * (8.0f * pixel), d8 = up * (8.0f * pixel);
            const glm::vec3 q[4] = {pen, pen - d8, pen - d8 + r8, pen + r8};
            quad(q, u, v, u + cell, v + cell, colour, m_text);
        }
        pen += right * (float(m_font.advance[code]) * pixel);
    }
}

void EntityRenderer::addCloud(const glm::dvec3& centre, float radius, float time, const glm::dvec3& cameraPos,
                              const glm::vec3& colour) {
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
        const uint32_t color = pack(colour);
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
    const float swing = mob.sitting ? 0.0f : std::cos(mob.limbSwing * 0.6662f) * 1.4f * mob.limbSwingAmount;
    // Body: vanilla yaw turns from +Z towards -X; dying tips over sideways over 20 ticks.
    glm::mat3 body = rotY(-bodyYaw * kDeg);
    if (mob.deathTime > 0 && mob.type != world::MobType::EnderDragon) body = body * rotZ(std::min(1.0f, float(mob.deathTime) / 20.0f) * 90.0f * kDeg);
    if (mob.sleeping) body = body * rotX(-90.0f * kDeg); // in bed: lying on its back, head toward the pillow
    const bool roosting = mob.type == world::MobType::Bat && mob.sitting; // (M29.1c: hanging upside down)
    if (roosting) body = body * rotZ(180.0f * kDeg);
    const glm::mat3 head = rotY((headYaw - bodyYaw) * kDeg) * rotX(-pitch * kDeg);
    // (M29.1b) a jockey sits: legs forward and a little apart (vanilla's riding pose).
    const bool seated = mob.vehicle != 0;
    const glm::mat3 legA = seated ? rotX(-72.0f * kDeg) * rotY(-18.0f * kDeg) : rotX(swing),
                    legB = seated ? rotX(-72.0f * kDeg) * rotY(18.0f * kDeg) : rotX(-swing),
                    arm = rotX(-90.0f * kDeg + swing * 0.2f);
    const float flap = std::sin(mob.limbSwing) * 0.6f; // (dragon wings)
    const glm::mat3 wingL = rotZ(flap), wingR = rotZ(-flap);
    const glm::mat3 tail = rotY(std::sin(mob.limbSwing * 0.8f) * 0.45f); // (fish tails wag side to side)
    const bool red = mob.hurtTime > 0 || mob.deathTime > 0;
    glm::vec3 base(pos - cameraPos);
    if (roosting) base.y += float(world::mobInfo(mob.type).height);
    else if (mob.sitting && mob.type != world::MobType::Villager) // (a sitting pet sinks onto its haunches; a camel lies down)
        base.y -= world::isCamel(mob.type) ? 1.0f : 0.25f;
    if (mob.convertTicks > 0) // a curing zombie villager shakes (wiki)
        base.x += 0.05f * std::sin(float(mob.convertTicks) * 2.5f);
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
    float scale = mob.age < 0 ? (mob.type == world::MobType::HappyGhast ? 0.2375f : 0.5f) : 1.0f; // babies: half size (ghastlings: 0.95 of 4)
    scale *= world::mobInfo(mob.type).modelScale; // (ghasts: 4.5)
    if (mob.type == world::MobType::MagmaCube || mob.type == world::MobType::Slime)
        scale *= float(mob.size); // its model is the size-1 cube
    if (mob.type == world::MobType::Pufferfish) scale *= 0.5f + 0.25f * float(mob.size); // puffing up (0..2)
    // Glow squid glow in the dark (wiki: Glow Squid - its texture is drawn bright).
    const glm::vec3 glowing = mob.type == world::MobType::GlowSquid ? glm::max(light, glm::vec3(0.9f)) : light;
    glm::vec3 flash(0.0f);
    if (mob.fuse > 0) { // a swelling creeper grows and flashes white (wiki: Creeper)
        const float f = std::min(1.0f, float(mob.fuse) / 30.0f);
        scale *= 1.0f + f * 0.2f;
        if ((mob.fuse / 3) % 2 == 1) flash = glm::vec3(0.6f * f);
    }
    const bool chestBoat = mob.type == world::MobType::Boat && mob.hasChest;
    // (M29.1f) what it holds in its right hand: where the hand is and which way the arm points.
    const world::ItemId held = world::heldItemOf(mob);
    glm::vec3 hand(0.0f), armOut(0.0f);
    bool haveHand = false;
    for (const MobPart& part : chestBoat ? gfx::chestBoatModel() : mobModel(mob.type)) {
        // Mount gear (M26.2): what it wears.
        if ((part.layer == 9 && !mob.saddled) || (part.layer == 10 && mob.horseArmor == 0) ||
            (part.layer == 11 && !mob.hasChest) || (part.layer == 12 && mob.decor == 0) ||
            (part.layer == 13 && !(mob.horns & 1)) || (part.layer == 14 && !(mob.horns & 2))) // (M26.3: goat horns)
            continue;
        if (part.layer >= 15 && part.layer <= 18 && mob.worn[size_t(part.layer - 15)] == 0) continue; // (M28.3b)
        if (part.layer == 1 && mob.sheared) continue;
        if (part.layer == 2 && !mob.showBottom) continue;
        if (part.layer == 3 && mob.type != world::MobType::Villager && mob.type != world::MobType::ZombieVillager)
            continue; // (profession robes: villagers and zombie villagers)
        glm::vec3 mn(part.from[0], part.from[1], part.from[2]), mx(part.to[0], part.to[1], part.to[2]);
        const glm::vec3 pivot(part.pivot[0], part.pivot[1], part.pivot[2]);
        const float w = mx.x - mn.x, h = mx.y - mn.y, d = mx.z - mn.z; // UV size (before inflating)
        mn -= glm::vec3(part.inflate);
        mx += glm::vec3(part.inflate);
        const float u = float(part.u), v = float(part.v) + (part.layer == 1   ? float(kSheepWoolRow * 64)
                                                             : part.layer == 3 ? float(kVillagerApronRow * 64)
                                                             : part.layer >= 9 && part.layer <= 12 ? float(kMountGearRow * 64)
                                                                               : vrow);
        glm::vec3 partTint = tint;
        if (mob.type == world::MobType::Mooshroom && mob.woolColour == 1) // (M29.1c) a brown mooshroom
            partTint *= glm::vec3(0.78f, 0.58f, 0.46f);
        // (M29.1d) warm farm animals are ruddier, cold ones paler and greyer (ours: tints
        // of the one skin; vanilla gives each its own model and coat)
        if ((mob.type == world::MobType::Cow || mob.type == world::MobType::Pig ||
             mob.type == world::MobType::Chicken) && mob.woolColour % 3 != 0)
            partTint *= mob.woolColour % 3 == 1 ? glm::vec3(1.0f, 0.72f, 0.5f) : glm::vec3(0.82f, 0.84f, 0.9f);
        if (part.layer == 3) { // the profession's colour (M24.1)
            const uint32_t c = world::professionInfo(static_cast<world::Profession>(mob.profession)).colour;
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        if (part.layer == 7 && !mob.tamed) continue; // (collars: pets only - M26.1)
        if (part.layer == 7) {
            const uint32_t c = kWoolColours[mob.color2 & 15];
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        if (part.layer == 8) { // fur by variant (M26.1)
            const uint32_t c = mob.type == world::MobType::Wolf ? world::kWolfVariants[mob.woolColour % 9].colour
                               : mob.type == world::MobType::Cat ? world::kCatVariants[mob.woolColour % 11].colour
                               : mob.type == world::MobType::Parrot ? world::kParrotColours[mob.woolColour % 5]
                               : mob.type == world::MobType::Horse  ? world::kHorseColours[mob.woolColour % 7].colour
                               : mob.type == world::MobType::Llama  ? world::kLlamaVariants[mob.woolColour % 4].colour
                               : mob.type == world::MobType::Rabbit ? world::kRabbitKinds[mob.woolColour % 6].colour
                               : mob.type == world::MobType::Frog   ? world::kFrogVariants[mob.woolColour % 3].colour
                               : mob.type == world::MobType::CopperGolem
                                   ? std::array<uint32_t, 4>{0xD07A50u, 0xB08C6Cu, 0x6EA07Au, 0x52B096u}[mob.woolColour % 4]
                               : mob.type == world::MobType::Axolotl ? world::kAxolotlColours[mob.woolColour % 5].colour
                               : mob.type == world::MobType::Fox    ? (mob.woolColour == 1 ? 0xF2F2F2u : 0xD87A30u)
                               : mob.type == world::MobType::Panda && world::pandaPersonality(mob.woolColour, mob.color2) == 4
                                   ? 0xA07850u // (a brown panda)
                                   : 0xFFFFFFu;
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        if (part.layer == 10) { // horse armor's material (M26.2): leather, iron, gold, diamond
            static constexpr uint32_t kArmor[5] = {0xFFFFFF, 0xA0643A, 0xDADADA, 0xF4D040, 0x5CE0D8};
            const uint32_t c = kArmor[mob.horseArmor % 5];
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        if (part.layer >= 15 && part.layer <= 18) { // an armor stand's armor (M28.3b), by material
            static constexpr uint32_t kMaterials[10] = {0xFFFFFF, 0xA0643A, 0x9A9AA0, 0xDADADA, 0xF4D040,
                                                        0x5CE0D8, 0x4A4048, 0x4E9A3A, 0xD9804F, 0xC87A2A};
            const uint32_t c = kMaterials[mob.worn[size_t(part.layer - 15)] % 10];
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        if (part.layer == 12) { // a llama's carpet (M26.2)
            const uint32_t c = kWoolColours[(mob.decor + 15) & 15];
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        if (part.layer == 6) { // a boat's wood (M25.2b)
            const uint32_t c = world::kBoatWoods[mob.woolColour % 10].colour;
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
        if (part.layer == 4 || part.layer == 5) { // a tropical fish's colours (M25.2)
            const uint32_t c = kWoolColours[(part.layer == 4 ? mob.woolColour : mob.color2) & 15];
            partTint *= glm::vec3(float(c >> 16 & 255), float(c >> 8 & 255), float(c & 255)) / 255.0f;
        }
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
                                : part.anim == MobPart::Anim::Tail       ? &tail
                                                                         : nullptr;
        glm::vec3 corners[8];
        for (int i = 0; i < 8; ++i) {
            glm::vec3 c(i & 1 ? mx.x : mn.x, i & 2 ? mx.y : mn.y, i & 4 ? mx.z : mn.z);
            if (anim) c = *anim * (c - pivot) + pivot;
            if (part.anim == MobPart::Anim::Lift) c.y += float(mob.peek) * 0.08f;
            corners[i] = base + body * c * (scale / 16.0f); // pixels -> blocks
        }
        if (held != 0 && part.anim == MobPart::Anim::ArmForward && part.pivot[0] < 0.0f && !haveHand) {
            const glm::vec3 tip((mn.x + mx.x) * 0.5f, mn.y, (mn.z + mx.z) * 0.5f); // the hand: the arm's end
            hand = base + body * (*anim * (tip - pivot) + pivot) * (scale / 16.0f);
            armOut = glm::normalize(body * (*anim * glm::vec3(0.0f, -1.0f, 0.0f)));
            haveHand = true;
        }
        const float uv[6][4] = {
            {u + d, v + d, w, h}, {u + 2 * d + w, v + d, w, h}, {u, v + d, d, h},
            {u + d + w, v + d, d, h}, {u + d, v, w, d}, {u + d + w, v, w, d},
        };
        for (int f = 0; f < 6; ++f) {
            const Face& face = kFaces[f];
            const glm::vec3 p[4] = {corners[face.c[0]], corners[face.c[1]], corners[face.c[2]], corners[face.c[3]]};
            quad(p, uv[f][0], uv[f][1], uv[f][0] + uv[f][2], uv[f][1] + uv[f][3],
                 pack(glm::min(glowing * partTint * face.shade + flash, glm::vec3(1.0f))),
                 m_mobs);
        }
    }
    // The held item (vanilla draws it in the hand): its sprite upright beside the arm, the
    // handle at the hand, drawn from both sides.
    if (haveHand)
        if (const uint16_t sprite = itemSprite({held, 1})) {
            const glm::vec3 up = glm::normalize(body * glm::vec3(0.0f, 1.0f, 0.0f) - armOut * glm::dot(armOut, body * glm::vec3(0.0f, 1.0f, 0.0f)) + glm::vec3(0.0f, 0.001f, 0.0f));
            const float s = 0.32f * scale;
            const glm::vec3 o = hand + (armOut + up) * (s * 0.6f);
            const float u0 = float(sprite % m_columns) * m_cell, v0 = float(sprite / m_columns) * m_cell;
            const glm::vec3 front[4] = {o - armOut * s + up * s, o - armOut * s - up * s, o + armOut * s - up * s,
                                        o + armOut * s + up * s};
            const glm::vec3 back[4] = {front[3], front[2], front[1], front[0]};
            quad(front, u0, v0, u0 + m_cell, v0 + m_cell, pack(light), m_items);
            quad(back, u0 + m_cell, v0, u0, v0 + m_cell, pack(light), m_items);
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
                 bolts = std::min(m_bolts.size(), size_t(1024) * 6), text = std::min(m_text.size(), size_t(4096) * 6);
    const size_t textBase = weatherBase + weather + bolts;
    if (text)
        glNamedBufferSubData(m_vbo, GLintptr(textBase * sizeof(Vertex)), GLsizeiptr(text * sizeof(Vertex)), m_text.data());
    if (weather) glNamedBufferSubData(m_vbo, GLintptr(weatherBase * sizeof(Vertex)), GLsizeiptr(weather * sizeof(Vertex)), m_weather.data());
    if (bolts)
        glNamedBufferSubData(m_vbo, GLintptr((weatherBase + weather) * sizeof(Vertex)), GLsizeiptr(bolts * sizeof(Vertex)),
                             m_bolts.data());
    if (items + crack + mobs + weather + bolts + text == 0) return;
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
    if (text) { // sign text: cut out of the font sheet (M23.3c)
        glUniform1f(1, 0.5f);
        glBindTextureUnit(0, m_fontTexture);
        glDrawArrays(GL_TRIANGLES, GLint(textBase), GLsizei(text));
        glBindTextureUnit(0, m_atlasTexture);
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
    m_text.clear();
}

} // namespace mc::gfx
