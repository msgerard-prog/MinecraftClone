#include "rendering/BlockModels.h"

#include "rendering/TextureAtlas.h"
#include "world/Blocks.h"

namespace mc::gfx {

namespace {

using world::Direction;

BakedModel cubeAll(uint16_t sprite) {
    BakedModel m;
    m.visible = true;
    for (auto& f : m.faces)
        f.sprite = sprite;
    return m;
}

// Vanilla cube_column: `end` on the two faces along the axis, `side` elsewhere.
// Horizontal logs rotate the side texture so the bark runs along the axis.
BakedModel cubeColumn(uint16_t side, uint16_t end, std::string_view axis) {
    BakedModel m = cubeAll(side);
    auto setEnd = [&](Direction a, Direction b) {
        m.faces[int(a)].sprite = end;
        m.faces[int(b)].sprite = end;
    };
    auto rotate = [&](std::initializer_list<Direction> faces) {
        for (Direction d : faces)
            m.faces[int(d)].rotation = 1;
    };
    if (axis == "x") {
        setEnd(Direction::West, Direction::East);
        rotate({Direction::Up, Direction::Down, Direction::North, Direction::South});
    } else if (axis == "z") {
        setEnd(Direction::North, Direction::South);
        rotate({Direction::West, Direction::East});
    } else {
        setEnd(Direction::Down, Direction::Up);
    }
    return m;
}

} // namespace

void BlockModels::bake(const world::BlockRegistry& registry, const TextureAtlas& atlas) {
    using namespace world;
    m_models.assign(registry.stateCount(), {});
    auto sprite = [&](const char* name) { return static_cast<uint16_t>(atlas.spriteIndex(name)); };

    for (size_t s = 0; s < registry.stateCount(); ++s) {
        const auto state = static_cast<BlockStateId>(s);
        BakedModel& m = m_models[s];
        switch (registry.blockOf(state)) {
        case blocks::Air:
            break;
        case blocks::Stone:
            m = cubeAll(sprite("stone"));
            break;
        case blocks::Dirt:
            m = cubeAll(sprite("dirt"));
            break;
        case blocks::Cobblestone:
            m = cubeAll(sprite("cobblestone"));
            break;
        case blocks::OakPlanks:
            m = cubeAll(sprite("oak_planks"));
            break;
        case blocks::Bedrock:
            m = cubeAll(sprite("bedrock"));
            break;
        case blocks::Sand:
            m = cubeAll(sprite("sand"));
            break;
        case blocks::OakLog:
            m = cubeColumn(sprite("oak_log"), sprite("oak_log_top"),
                           registry.value(state, "axis").value_or("y"));
            break;
        case blocks::GrassBlock:
            // Greyscale top tinted by the biome colour; dirt bottom. (snowy=true uses
            // the snowy side texture in vanilla; not modelled until snow exists.)
            m = cubeAll(sprite("grass_block_side"));
            m.faces[int(Direction::Up)] = {sprite("grass_block_top"), 0, Tint::Grass};
            m.faces[int(Direction::Down)].sprite = sprite("dirt");
            break;
        default:
            // A registered block without a model: vanilla shows the missing model.
            m = cubeAll(sprite(std::string(TextureAtlas::kMissing).c_str()));
            break;
        }
    }
}

} // namespace mc::gfx
