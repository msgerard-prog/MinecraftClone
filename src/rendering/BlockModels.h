#pragma once

#include "rendering/PackedVertex.h"
#include "world/BlockRegistry.h"
#include "world/Direction.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace mc::gfx {

class TextureAtlas;

// One face of a baked full-cube model: everything the mesher needs, no lookups.
struct BakedFace {
    uint16_t sprite = 0;  // atlas grid index
    uint8_t rotation = 0; // quarter turns clockwise of the texture on the face (0..3)
    bool mirror = false;  // texture flipped left-right (vanilla *_mirrored models)
    Tint tint = Tint::None;
};

struct BakedVariant {
    BakedFace faces[world::kDirectionCount];
};

// Vanilla blockstates can list several models for one state; the game picks one per
// block position (e.g. grass tops randomly rotated so the ground doesn't tile).
// A box element of a non-cube model (vanilla model "elements"), in 1/16 block.
// Each face has a sprite and a texel rectangle (u0, v0, u1, v1 in 0..16).
struct BakedBox {
    uint8_t from[3];
    uint8_t to[3];
    struct Face {
        uint16_t sprite = 0;
        uint8_t uv[4] = {0, 0, 16, 16};
        bool present = true;
    } faces[world::kDirectionCount];
};

struct BakedModel {
    static constexpr int kMaxVariants = 4;
    static constexpr int kMaxBoxes = 2;
    bool visible = false;     // false for air / invisible blocks
    bool translucent = false; // drawn in the blended pass (water, ice, stained glass)
    bool fluid = false;       // surface at 8/9 (source fluid)
    bool cullSame = false;    // faces against the same block are hidden (fluids, glass)
    uint8_t variantCount = 1;
    BakedVariant variants[kMaxVariants];
    // Non-cube models (torch...): boxes instead of the 6 full-cube faces.
    uint8_t boxCount = 0;
    BakedBox boxes[kMaxBoxes];
    // Plants (vanilla "cross" model): two diagonal planes, seen from both sides.
    bool cross = false;
    uint16_t crossSprite = 0;
    Tint crossTint = Tint::None;
};

// Per-state models, resolved once at startup (vanilla "model baking").
// Until the JSON loader exists, block -> model mapping lives in BlockModels.cpp,
// mirroring vanilla's cube_all, cube_column(_horizontal), grass_block and the
// random-variant blockstates of stone, bedrock, dirt, sand and grass.
class BlockModels {
public:
    void bake(const world::BlockRegistry& registry, const TextureAtlas& atlas);
    const BakedModel& operator[](world::BlockStateId state) const { return m_models[state]; }
    size_t size() const { return m_models.size(); }

    // Test/helper path: set models directly.
    void resize(size_t states) { m_models.assign(states, {}); }
    BakedModel& at(world::BlockStateId state) { return m_models[state]; }

private:
    std::vector<BakedModel> m_models;
};

// Which variant a block at this world position uses. Deterministic per position.
// Our own hash: vanilla's per-position seed isn't documented on the wiki, so the
// variant chosen at a given position differs from vanilla (known deviation).
uint32_t variantIndex(int32_t x, int32_t y, int32_t z, uint32_t variantCount);

// Rotation helpers (exposed for tests and for the future JSON loader).
// Vanilla blockstate "y" rotation applied to a cube whose side faces share one
// texture: the up face turns by `quarters`, the down face the other way.
BakedVariant rotateY(BakedVariant v, int quarters);

// Vanilla cube_column for a log-like block on `axis` ("x" | "y" | "z").
BakedVariant cubeColumn(uint16_t side, uint16_t end, std::string_view axis);

} // namespace mc::gfx
