#include "rendering/EntityRenderer.h"

#include "rendering/TextureAtlas.h"
#include "world/Blocks.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace mc::gfx {

namespace {

// Same curve as assets/shaders/block.vert brightness().
float brightness(float level) {
    const float f = std::clamp(level / 15.0f, 0.0f, 1.0f);
    const float b = f / (4.0f - 3.0f * f);
    const float x = 1.0f - b, x2 = x * x;
    const float lifted = 1.0f - x2 * x2;
    return 0.05f + 0.95f * (b + (lifted - b) * 0.5f);
}

uint32_t pack(const glm::vec3& c, float a = 1.0f) {
    auto ch = [](float v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return ch(c.r) | (ch(c.g) << 8) | (ch(c.b) << 16) | (ch(a) << 24);
}

} // namespace

glm::vec3 lightColor(int sky, int block, float skyDarken) {
    const glm::vec3 skyPart(brightness(float(sky) - skyDarken));
    const glm::vec3 blockPart = brightness(float(block)) * glm::vec3(1.0f, 0.93f, 0.82f);
    return glm::min(skyPart + blockPart, glm::vec3(1.0f));
}

EntityRenderer::~EntityRenderer() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool EntityRenderer::init(const TextureAtlas& atlas, const BlockModels& models, const ItemIcons& icons) {
    if (!m_shader.load("entity")) return false;
    m_atlasTexture = atlas.texture();
    m_columns = atlas.columns();
    m_cell = atlas.cellSize();
    m_models = &models;
    m_icons = &icons;
    for (int i = 0; i < 10; ++i)
        m_crackSprites[i] = static_cast<uint16_t>(atlas.spriteIndex("destroy_stage_" + std::to_string(i)));
    m_items.reserve(size_t(kMaxQuads) * 6);
    m_crack.reserve(36);
    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, GLsizeiptr(kMaxQuads) * 6 * sizeof(Vertex), nullptr, GL_DYNAMIC_STORAGE_BIT);
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
                          const glm::vec3& light, uint32_t tintRgb, std::vector<Vertex>& out, bool shade) {
    // Faces in world::Direction order (down, up, north, south, west, east); corners
    // top-left, bottom-left, bottom-right, top-right seen from outside.
    const glm::vec3 c[8] = {{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
                            {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}};
    static constexpr int kFaces[6][4] = {{4, 0, 1, 5}, {3, 7, 6, 2}, {2, 1, 0, 3},
                                         {7, 4, 5, 6}, {3, 0, 4, 7}, {6, 5, 1, 2}};
    static constexpr float kShade[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    const glm::vec3 tint(float(tintRgb & 255) / 255.0f, float((tintRgb >> 8) & 255) / 255.0f,
                         float((tintRgb >> 16) & 255) / 255.0f);
    for (int f = 0; f < 6; ++f) {
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
        uint32_t tint = 0xFFFFFF;
        for (int f = 0; f < 6; ++f) {
            sprites[f] = m.variants[0].faces[f].sprite;
            if (m.variants[0].faces[f].tint == Tint::Grass) tint = 0x6BBD7C; // item grass colour (BGR order)
        }
        const size_t start = m_items.size();
        cube({-0.125f, 0.0f, -0.125f}, {0.125f, 0.25f, 0.125f}, sprites, light, 0xFFFFFF, m_items, true);
        for (size_t i = start; i < m_items.size(); ++i) {
            Vertex& v = m_items[i];
            const glm::vec3 r = rot(v.x, v.y, v.z);
            v.x = r.x;
            v.y = r.y;
            v.z = r.z;
        }
        (void)tint;
        return;
    }
    const float u0 = float(sprite % m_columns) * m_cell, v0 = float(sprite / m_columns) * m_cell;
    const glm::vec3 p[4] = {rot(-0.25f, 0.5f, 0), rot(-0.25f, 0, 0), rot(0.25f, 0, 0), rot(0.25f, 0.5f, 0)};
    const glm::vec3 q[4] = {p[3], p[2], p[1], p[0]}; // back side
    quad(p, u0, v0, u0 + m_cell, v0 + m_cell, pack(light), m_items);
    quad(q, u0 + m_cell, v0, u0, v0 + m_cell, pack(light), m_items);
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
        cube(o - glm::vec3(g), o + glm::vec3(1.0f + g), s, glm::vec3(1.0f), 0xFFFFFF, m_crack, false);
    }
    const size_t items = m_items.size(), crack = m_crack.size();
    if (items + crack == 0) return;
    glNamedBufferSubData(m_vbo, 0, GLsizeiptr(items * sizeof(Vertex)), m_items.data());
    if (crack)
        glNamedBufferSubData(m_vbo, GLintptr(items * sizeof(Vertex)), GLsizeiptr(crack * sizeof(Vertex)),
                             m_crack.data());
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
    if (crack) { // vanilla crumbling: multiply the block underneath
        glUniform1f(1, 0.01f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, GLint(items), GLsizei(crack));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glEnable(GL_CULL_FACE);
    m_items.clear();
}

} // namespace mc::gfx
