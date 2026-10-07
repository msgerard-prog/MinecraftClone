#pragma once

#include "rendering/BlockModels.h"
#include "rendering/Camera.h"
#include "rendering/ItemIcons.h"
#include "rendering/Shader.h"
#include "world/Chunk.h"
#include "world/Items.h"

#include <glm/glm.hpp>

#include <cstdint>
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
    // A falling block (M16) at `pos` (bottom centre): a full-size cube of its model.
    void addBlock(world::BlockStateId state, const glm::dvec3& pos, const glm::vec3& light,
                  const glm::dvec3& cameraPos);
    // An experience orb (M17.5): a glowing camera-facing quad, bigger for big orbs.
    void addOrb(const glm::dvec3& pos, int value, float time, const glm::dvec3& cameraPos);
    // An end crystal's healing beam (M20.2) from `from` to `to`: two crossed glowing
    // strips (vanilla: a textured beam).
    void addBeam(const glm::dvec3& from, const glm::dvec3& to, const glm::dvec3& cameraPos);
    // A cloud of dragon's breath: glowing purple puffs over its disc (vanilla: particles).
    void addCloud(const glm::dvec3& centre, float radius, float time, const glm::dvec3& cameraPos);
    // An arrow (M16.4) with its tip at `tip`, pointing along `dir`: two crossed quads.
    void addArrow(const glm::dvec3& tip, const glm::dvec3& dir, const glm::vec3& light, const glm::dvec3& cameraPos);
    // The crack on a block being broken: stage 0..9 (destroy_stage_N).
    void setCrack(const world::BlockPos& block, int stage);
    void clearCrack() { m_crackStage = -1; }

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
};

} // namespace mc::gfx
