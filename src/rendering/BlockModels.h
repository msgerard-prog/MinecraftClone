#pragma once

#include "rendering/PackedVertex.h"
#include "world/BlockRegistry.h"
#include "world/Direction.h"

#include <cstdint>
#include <vector>

namespace mc::gfx {

class TextureAtlas;

// One face of a baked full-cube model: everything the mesher needs, no lookups.
struct BakedFace {
    uint16_t sprite = 0;  // atlas grid index
    uint8_t rotation = 0; // quarter turns of the texture on the face (0..3)
    Tint tint = Tint::None;
};

struct BakedModel {
    bool visible = false; // false for air / invisible blocks
    BakedFace faces[world::kDirectionCount];
};

// Per-state models, resolved once at startup (vanilla "model baking").
// Until the JSON loader exists, block -> model mapping lives in BlockModels.cpp,
// mirroring vanilla's block/cube_all, block/cube_column(_horizontal), grass_block.
class BlockModels {
public:
    void bake(const world::BlockRegistry& registry, const TextureAtlas& atlas);
    const BakedModel& operator[](world::BlockStateId state) const { return m_models[state]; }
    size_t size() const { return m_models.size(); }

    // Test/helper constructor path: set a model directly.
    void resize(size_t states) { m_models.assign(states, {}); }
    BakedModel& at(world::BlockStateId state) { return m_models[state]; }

private:
    std::vector<BakedModel> m_models;
};

} // namespace mc::gfx
