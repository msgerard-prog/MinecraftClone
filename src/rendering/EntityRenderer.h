#pragma once

#include "rendering/BlockModels.h"
#include "rendering/Camera.h"
#include "rendering/GuiBatch.h"
#include "rendering/ItemIcons.h"
#include "rendering/Shader.h"
#include "world/Chunk.h"
#include "world/Banners.h"
#include "world/Items.h"
#include "world/ParticleSprite.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace mc::gfx {

class TextureAtlas;

// Brightness 0..1 of a block's light as the block shader computes it (sky light
// minus the night darkening, block light warm), for entities lit per object.
// `ambient`: dimension ambient light (Nether 0.1); `forceBright`: the End's lightmap.
glm::vec3 lightColor(int sky, int block, float skyDarken, float ambient = 0.0f, bool forceBright = false);

// Draws dropped items and the block-breaking crack each frame: one dynamic buffer
// of camera-relative quads sampling the block atlas (GL-free building, one upload).
class EntityRenderer {
public:
    static constexpr int kMaxQuads = 32768;
    static constexpr int kMaxWeatherQuads = 1024; // one per column (21 x 21 within 10 blocks)

    EntityRenderer() = default;
    ~EntityRenderer();
    EntityRenderer(const EntityRenderer&) = delete;
    EntityRenderer& operator=(const EntityRenderer&) = delete;

    bool init(const TextureAtlas& atlas, const BlockModels& models, const ItemIcons& icons,
              const class PackStack& packs);
    // A mob at its render position (interpolated by the caller): cuboid model with
    // walk / head animation, red when hurt, falling over while dying.
    void addMob(const world::MobData& mob, const glm::dvec3& pos, float bodyYaw, float headYaw, float pitch,
                const glm::vec3& light, const glm::dvec3& cameraPos);

    // A dropped item at `pos` (feet of its 0.25 box): block items as 1/4-size cubes,
    // others as their sprite; spinning about Y and bobbing (wiki: Item (entity)).
    void addItem(const world::ItemStack& stack, const glm::dvec3& pos, float spin, float bob,
                 const glm::vec3& light, const glm::dvec3& cameraPos);
    // An item frame (M28.3a) centred at `centre` on a wall facing `facing`
    // (world::Direction): a 12x12 frame, its item flat inside turned `rotation` x 45 degrees.
    void addItemFrame(const glm::dvec3& centre, int facing, bool glow, const world::ItemStack& item, int rotation,
                      const glm::vec3& light, const glm::dvec3& cameraPos);
    // A painting (M28.3a): its canvas tiles (world::kPaintings[variant]) facing `facing`.
    void addPainting(int variant, const glm::dvec3& centre, int facing, const glm::vec3& light,
                     const glm::dvec3& cameraPos);
    // A banner (M28.3d) in the block at `cell` (its minimum corner): standing (`turn` =
    // rotation 0..15) or on a wall (`turn` = its facing, world::Direction); the flag in
    // `base` (dye 0..15) with its layers, front and back.
    void addBanner(const glm::dvec3& cell, bool wall, int turn, int base, const world::BannerLayers& layers,
                   const glm::vec3& light, const glm::dvec3& cameraPos);
    // A leash knot (M28.3c): a small wooden knot on its fence post, its bottom at `pos`.
    void addKnot(const glm::dvec3& pos, const glm::dvec3& cameraPos);
    // A falling block (M16) at `pos` (bottom centre): a full-size cube of its model.
    void addBlock(world::BlockStateId state, const glm::dvec3& pos, const glm::vec3& light,
                  const glm::dvec3& cameraPos);
    // An experience orb (M17.5): a glowing camera-facing quad, bigger for big orbs.
    void addOrb(const glm::dvec3& pos, int value, float time, const glm::dvec3& cameraPos);
    // An end crystal's healing beam (M20.2) from `from` to `to`: two crossed glowing
    // strips (vanilla: a textured beam).
    void addBeam(const glm::dvec3& from, const glm::dvec3& to, const glm::dvec3& cameraPos,
                 const glm::vec3& colour = {1.0f, 0.55f, 0.95f}, float halfWidth = 0.08f);
    // A cloud of dragon's breath: glowing purple puffs over its disc (vanilla: particles).
    void addCloud(const glm::dvec3& centre, float radius, float time, const glm::dvec3& cameraPos,
                  const glm::vec3& colour = {0.75f, 0.3f, 0.95f});
    // An arrow (M16.4) with its tip at `tip`, pointing along `dir`: two crossed quads.
    void addArrow(const glm::dvec3& tip, const glm::dvec3& dir, const glm::vec3& light, const glm::dvec3& cameraPos);
    // The crack on a block being broken: stage 0..9 (destroy_stage_N).
    void setCrack(const world::BlockPos& block, int stage);
    void clearCrack() { m_crackStage = -1; }
    // Weather (M22.1; vanilla's rain/snow layer): one camera-facing quad per column
    // (x, z) from y0 to y1, its repeating texture (environment/rain.png, snow.png)
    // scrolled down by `scroll` blocks (rain fast, snow slowly with a sideways drift),
    // blended over the scene.
    void addPrecipitation(int32_t x, int32_t z, int y0, int y1, bool snow, float scroll, float drift, float alpha,
                          const glm::vec3& light, const glm::dvec3& cameraPos);
    // A lightning bolt from the sky down to `ground`: jagged segments (from `seed`)
    // and a few branches, drawn additively.
    void addLightning(const glm::dvec3& ground, uint32_t seed, const glm::dvec3& cameraPos);
    // A line of text in the world (M23.3c: signs), centred on `centre`, in the plane of
    // `right` and `up` (unit vectors), `pixel` blocks per font pixel, colour 0xRRGGBB.
    void addText(std::string_view text, const glm::dvec3& centre, const glm::vec3& right, const glm::vec3& up,
                 float pixel, uint32_t rgb, const glm::dvec3& cameraPos);
    // A particle (M22.3): a camera-facing square of half-size `size`; Terrain particles
    // show the 4x4-texel piece (u, v) (quarters) of the block's texture.
    void addParticle(const glm::dvec3& pos, float size, ParticleSprite sprite, world::BlockStateId state, uint8_t u,
                     uint8_t v, const glm::vec3& color, const glm::dvec3& cameraPos, const glm::vec3& right,
                     const glm::vec3& up);

    // Uploads and draws everything added this frame, then clears.
    void draw(const Camera& camera, float aspect);

private:
    struct Vertex {
        float x, y, z;
        float u, v; // atlas texels
        uint32_t color;
    };
    void quad(const glm::vec3 (&p)[4], float u0, float v0, float u1, float v1, uint32_t color,
              std::vector<Vertex>& out);
    void cube(const glm::vec3& min, const glm::vec3& max, const uint16_t (&sprites)[6],
              const glm::vec3& light, const uint32_t (&tints)[6], std::vector<Vertex>& out, bool shade);

    Shader m_shader;
    uint32_t m_vao = 0, m_vbo = 0;
    uint32_t m_atlasTexture = 0;
    int m_columns = 1, m_cell = 16;
    const BlockModels* m_models = nullptr;
    const ItemIcons* m_icons = nullptr;
    uint16_t m_crackSprites[10] = {};
    int m_crackStage = -1;
    uint16_t m_orbSprite = 0;
    world::BlockPos m_crackBlock{};
    std::vector<Vertex> m_items; // reserved once
    std::vector<Vertex> m_mobs;  // mob atlas pass
    uint32_t m_mobTexture = 0;
    std::vector<Vertex> m_crack;
    std::vector<Vertex> m_weather; // blended, after everything else
    std::vector<Vertex> m_bolts;   // additive
    uint16_t m_boltSprite = 0;
    uint32_t m_weatherTexture = 0; // rain | snow side by side (16 + 16 wide), repeating in v
    uint32_t m_fontTexture = 0;    // sign text (font/ascii.png)
    FontMetrics m_font;
    std::vector<Vertex> m_text;    // reserved; drawn with the font texture
    uint16_t m_particleSprites[size_t(ParticleSprite::Count)] = {};
    // (M28.3a) frame and painting sprites; painting tiles per variant, row by row from the top
    uint16_t m_frameSprite = 0, m_glowFrameSprite = 0, m_frameWood = 0, m_paintingBack = 0;
    std::vector<std::vector<uint16_t>> m_paintingTiles;
    // (M28.3d) banner masks: top and bottom halves per pattern (index kBannerPatterns), then base
    std::vector<std::array<uint16_t, 2>> m_bannerMasks;
    uint16_t m_bannerWood = 0;
};

} // namespace mc::gfx
