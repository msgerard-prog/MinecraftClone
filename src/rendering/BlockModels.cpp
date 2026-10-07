#include "rendering/BlockModels.h"

#include "rendering/TextureAtlas.h"
#include "world/Blocks.h"

#include <string>

namespace mc::gfx {

namespace {

using world::Direction;

BakedVariant cubeAll(uint16_t sprite) {
    BakedVariant v;
    for (auto& f : v.faces)
        f.sprite = sprite;
    return v;
}

BakedVariant mirrored(BakedVariant v) {
    for (auto& f : v.faces)
        f.mirror = true;
    return v;
}

BakedModel single(const BakedVariant& v) {
    BakedModel m;
    m.visible = true;
    m.variants[0] = v;
    return m;
}

// Vanilla dirt/sand/grass blockstates: y = 0, 90, 180, 270, equal weight.
BakedModel fourYRotations(const BakedVariant& v) {
    BakedModel m;
    m.visible = true;
    m.variantCount = 4;
    for (int i = 0; i < 4; ++i)
        m.variants[i] = rotateY(v, i);
    return m;
}

// Vanilla stone/bedrock blockstates: normal, mirrored, each with y = 0 or 180.
BakedModel mirroredAndRotated(const BakedVariant& v) {
    BakedModel m;
    m.visible = true;
    m.variantCount = 4;
    m.variants[0] = v;
    m.variants[1] = mirrored(v);
    m.variants[2] = rotateY(v, 2);
    m.variants[3] = rotateY(mirrored(v), 2);
    return m;
}

} // namespace

uint32_t variantIndex(int32_t x, int32_t y, int32_t z, uint32_t variantCount) {
    if (variantCount <= 1) return 0;
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^
                 static_cast<uint32_t>(z) * 83492791u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h % variantCount;
}

// Vanilla cube_column: `end` on the two faces along the axis, `side` elsewhere.
// Horizontal logs (cube_column_horizontal rotated by the blockstate) turn the side
// textures so the bark runs along the axis and the end rings stay upright.
// Rotations are quarter turns clockwise per face.
BakedVariant cubeColumn(uint16_t side, uint16_t end, std::string_view axis) {
    BakedVariant v = cubeAll(side);
    auto set = [&](Direction d, uint16_t sprite, uint8_t rotation) {
        v.faces[int(d)].sprite = sprite;
        v.faces[int(d)].rotation = rotation;
    };
    if (axis == "x") {
        set(Direction::West, end, 0);
        set(Direction::East, end, 0);
        set(Direction::Up, side, 1);
        set(Direction::Down, side, 1);
        set(Direction::North, side, 3);
        set(Direction::South, side, 1);
    } else if (axis == "z") {
        set(Direction::North, end, 0);
        set(Direction::South, end, 0);
        set(Direction::Down, side, 2);
        set(Direction::West, side, 3);
        set(Direction::East, side, 1);
    } else {
        set(Direction::Down, end, 0);
        set(Direction::Up, end, 0);
    }
    return v;
}

BakedVariant rotateY(BakedVariant v, int quarters) {
    const int q = quarters & 3;
    auto& up = v.faces[int(Direction::Up)];
    auto& down = v.faces[int(Direction::Down)];
    up.rotation = static_cast<uint8_t>((up.rotation + q) & 3);
    down.rotation = static_cast<uint8_t>((down.rotation + 4 - q) & 3);
    return v;
}

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
            m = mirroredAndRotated(cubeAll(sprite("stone")));
            break;
        case blocks::Bedrock:
            m = mirroredAndRotated(cubeAll(sprite("bedrock")));
            break;
        case blocks::Dirt:
            m = fourYRotations(cubeAll(sprite("dirt")));
            break;
        case blocks::Sand:
            m = fourYRotations(cubeAll(sprite("sand")));
            break;
        case blocks::Cobblestone:
            m = single(cubeAll(sprite("cobblestone")));
            break;
        case blocks::OakPlanks:
            m = single(cubeAll(sprite("oak_planks")));
            break;
        case blocks::Deepslate:
            m = single(cubeColumn(sprite("deepslate"), sprite("deepslate_top"),
                                  registry.value(state, "axis").value_or("y")));
            break;
        case blocks::Gravel:
            m = single(cubeAll(sprite("gravel")));
            break;
        case blocks::Water: {
            // Every level uses the still texture for now (flowing texture with
            // direction comes with fluid flow).
            BakedVariant v = cubeAll(sprite("water_still"));
            for (auto& f : v.faces)
                f.tint = Tint::Water;
            m = single(v);
            m.translucent = true;
            m.fluid = true;
            break;
        }
        case blocks::OakLog:
            m = single(cubeColumn(sprite("oak_log"), sprite("oak_log_top"),
                                  registry.value(state, "axis").value_or("y")));
            break;
        case blocks::GrassBlock: {
            // Greyscale top tinted by the biome colour; dirt bottom. snowy=true should
            // use the snowy side and an untinted top (known deviation until snow).
            BakedVariant v = cubeAll(sprite("grass_block_side"));
            v.faces[int(Direction::Up)] = {sprite("grass_block_top"), 0, false, Tint::Grass};
            v.faces[int(Direction::Down)].sprite = sprite("dirt");
            m = fourYRotations(v);
            break;
        }
        default:
            // A registered block without a model: vanilla shows the missing model.
            m = single(cubeAll(sprite(std::string(TextureAtlas::kMissing).c_str())));
            break;
        }
    }
}

} // namespace mc::gfx
