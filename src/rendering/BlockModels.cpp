#include "rendering/BlockModels.h"
#include "world/Biome.h"

#include <algorithm>

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
        if (bakeRedstoneModel(registry, state, atlas, m)) continue;
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
        case blocks::Glowstone:
            m = single(cubeAll(sprite("glowstone")));
            break;
        case blocks::Glass:
            m = single(cubeAll(sprite("glass")));
            m.cullSame = true; // glass hides its faces against other glass
            break;
        case blocks::Torch: {
            // Vanilla torch model: a 2x10x2 stick in the middle of the block; sides use
            // the texture's stick column, the top shows the flame (texels 7..9, 6..8).
            m.visible = true;
            m.boxCount = 1;
            BakedBox& b = m.boxes[0];
            b.from[0] = 7;
            b.from[1] = 0;
            b.from[2] = 7;
            b.to[0] = 9;
            b.to[1] = 10;
            b.to[2] = 9;
            const uint16_t t = sprite("torch");
            for (auto& f : b.faces) {
                f.sprite = t;
                f.uv[0] = 7;
                f.uv[1] = 6;
                f.uv[2] = 9;
                f.uv[3] = 16;
            }
            auto& top = b.faces[int(Direction::Up)];
            top.uv[0] = 7;
            top.uv[1] = 6;
            top.uv[2] = 9;
            top.uv[3] = 8;
            auto& bottom = b.faces[int(Direction::Down)];
            bottom.uv[0] = 7;
            bottom.uv[1] = 14;
            bottom.uv[2] = 9;
            bottom.uv[3] = 16;
            break;
        }
        case blocks::Water: {
            // Every level uses the still texture for now (flowing texture with
            // direction comes with fluid flow).
            BakedVariant v = cubeAll(sprite("water_still"));
            for (auto& f : v.faces)
                f.tint = Tint::Water;
            m = single(v);
            m.translucent = true;
            m.fluid = true;
            m.cullSame = true;
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
        default: {
            // Name-driven models (vanilla's common templates) for the many simple
            // blocks: cube_all by name, logs as cube_column, leaves tinted, plants as
            // cross, sandstones bottom/top. Unknown textures show the missing sprite.
            std::string name = registry.block(registry.blockOf(state)).id;
            if (name.starts_with("minecraft:")) name.erase(0, 10);
            const auto ends = [&](std::string_view s) { return name.ends_with(s); };
            static constexpr std::string_view kPlants[] = {
                "short_grass", "fern", "dandelion", "poppy", "cornflower", "azure_bluet",
                "oxeye_daisy", "dead_bush", "oak_sapling", "birch_sapling", "spruce_sapling", "acacia_sapling",
                "brown_mushroom", "red_mushroom", "jungle_sapling", "dark_oak_sapling", "cherry_sapling",
                "crimson_fungus", "warped_fungus", "crimson_roots", "warped_roots", "nether_sprouts",
                "weeping_vines", "weeping_vines_plant", "twisting_vines", "twisting_vines_plant"};
            if (name == "wheat" || name == "carrots" || name == "potatoes" || name == "beetroots") {
                // Crops by age (vanilla: carrots/potatoes 8 ages on 4 textures - 0-1,
                // 2-3, 4-6, 7). Drawn as a cross (vanilla's crop model is a # of 4 planes).
                const int a = std::stoi(std::string(registry.value(state, "age").value_or("0")));
                const int stage = name == "wheat" || name == "beetroots" ? a : a < 2 ? 0 : a < 4 ? 1 : a < 7 ? 2 : 3;
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite((name + "_stage" + std::to_string(stage)).c_str());
            } else if (name == "bookshelf") { // books on the sides, planks top and bottom
                BakedVariant v = cubeAll(sprite("bookshelf"));
                v.faces[int(Direction::Up)].sprite = sprite("oak_planks");
                v.faces[int(Direction::Down)].sprite = sprite("oak_planks");
                m = single(v);
            } else if (name == "chiseled_sandstone" || name == "cut_sandstone" || name == "smooth_sandstone") {
                // Vanilla: the plain sandstone top on top and bottom (smooth: everywhere).
                BakedVariant v = cubeAll(sprite(name == "smooth_sandstone" ? "sandstone_top" : name.c_str()));
                v.faces[int(Direction::Up)].sprite = sprite("sandstone_top");
                v.faces[int(Direction::Down)].sprite = sprite("sandstone_top");
                m = single(v);
            } else if (name == "nether_wart") { // a cross of its stage (0, 1-2, 3)
                const int a = std::stoi(std::string(registry.value(state, "age").value_or("0")));
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(a == 0 ? "nether_wart_stage0" : a < 3 ? "nether_wart_stage1" : "nether_wart_stage2");
            } else if (name == "nether_brick_fence") { // a post (vanilla: posts with arms to neighbours)
                m.visible = true;
                m.boxCount = 1;
                BakedBox& b = m.boxes[0];
                b.from[0] = 6, b.from[1] = 0, b.from[2] = 6;
                b.to[0] = 10, b.to[1] = 16, b.to[2] = 10;
                for (int d = 0; d < 6; ++d) {
                    auto& face = b.faces[d];
                    face.sprite = sprite("nether_bricks");
                    face.uv[0] = 6, face.uv[1] = 0, face.uv[2] = 10, face.uv[3] = 16;
                }
            } else if (name == "chorus_plant" || name == "iron_bars") {
                // A middle piece plus an arm to each connected side (vanilla multipart):
                // the chorus plant a 8x8 core (vanilla 10x10 with fringes), iron bars a
                // 2-wide post with 2-wide arms (vanilla: flat panes).
                const bool bars = name == "iron_bars";
                const uint8_t lo = bars ? 7 : 4, hi = bars ? 9 : 12;
                const uint16_t sp = sprite(name.c_str());
                m.visible = true;
                auto addBox = [&](uint8_t x0, uint8_t y0, uint8_t z0, uint8_t x1, uint8_t y1, uint8_t z1) {
                    BakedBox& b = m.boxes[m.boxCount++];
                    b.from[0] = x0, b.from[1] = y0, b.from[2] = z0;
                    b.to[0] = x1, b.to[1] = y1, b.to[2] = z1;
                    // Each face shows the part of the texture it covers (vanilla's
                    // default UVs from the element's position).
                    for (int d = 0; d < 6; ++d) {
                        auto& f = b.faces[d];
                        f.sprite = sp;
                        const auto dir = static_cast<world::Direction>(d);
                        const bool vertical = dir == world::Direction::Up || dir == world::Direction::Down;
                        const bool alongZ = dir == world::Direction::West || dir == world::Direction::East;
                        const uint8_t u0 = alongZ ? z0 : x0, u1 = alongZ ? z1 : x1;
                        f.uv[0] = u0, f.uv[2] = u1;
                        f.uv[1] = vertical ? z0 : uint8_t(16 - y1);
                        f.uv[3] = vertical ? z1 : uint8_t(16 - y0);
                    }
                };
                addBox(lo, bars ? 0 : lo, lo, hi, bars ? 16 : hi, hi);
                auto on = [&](const char* prop) { return registry.value(state, prop).value_or("false") == "true"; };
                const uint8_t y0 = bars ? 0 : lo, y1 = bars ? 16 : hi;
                if (on("north")) addBox(lo, y0, 0, hi, y1, lo);
                if (on("south")) addBox(lo, y0, hi, hi, y1, 16);
                if (on("west")) addBox(0, y0, lo, lo, y1, hi);
                if (on("east")) addBox(hi, y0, lo, 16, y1, hi);
                if (!bars && on("down")) addBox(lo, 0, lo, hi, lo, hi);
                if (!bars && on("up")) addBox(lo, hi, lo, hi, 16, hi);
            } else if (name == "chorus_flower") {
                const int a = std::stoi(std::string(registry.value(state, "age").value_or("0")));
                m = single(cubeAll(sprite(a >= 5 ? "chorus_flower_dead" : "chorus_flower")));
            } else if (name == "purpur_pillar") {
                m = single(cubeColumn(sprite("purpur_pillar"), sprite("purpur_pillar_top"),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name == "brewing_stand") { // a blaze rod on a stone base (vanilla: 3 feet)
                m.visible = true;
                m.boxCount = 2;
                const uint8_t from[2][3] = {{2, 0, 2}, {7, 2, 7}}, to[2][3] = {{14, 2, 14}, {9, 14, 9}};
                for (int k = 0; k < 2; ++k) {
                    BakedBox& b = m.boxes[k];
                    for (int a = 0; a < 3; ++a)
                        b.from[a] = from[k][a], b.to[a] = to[k][a];
                    for (int d = 0; d < 6; ++d) {
                        auto& face = b.faces[d];
                        face.sprite = sprite(k == 0 ? "brewing_stand_base" : "brewing_stand");
                        face.uv[0] = k == 0 ? 2 : 7, face.uv[1] = k == 0 ? 2 : 2, face.uv[2] = k == 0 ? 14 : 9;
                        face.uv[3] = k == 0 ? 14 : 14;
                    }
                }
            } else if (name == "polished_basalt") {
                m = single(cubeColumn(sprite("polished_basalt_side"), sprite("polished_basalt_top"),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name == "crimson_nylium" || name == "warped_nylium") { // side, top, netherrack below
                BakedVariant v = cubeAll(sprite((name + "_side").c_str()));
                v.faces[int(Direction::Up)].sprite = sprite(name.c_str());
                v.faces[int(Direction::Down)].sprite = sprite("netherrack");
                m = single(v);
            } else if (name == "crimson_stem" || name == "warped_stem") {
                m = single(cubeColumn(sprite(name.c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name == "basalt" || name == "bone_block") {
                m = single(cubeColumn(sprite((name + "_side").c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name == "blackstone") {
                BakedVariant v = cubeAll(sprite("blackstone"));
                v.faces[int(Direction::Up)].sprite = sprite("blackstone_top");
                v.faces[int(Direction::Down)].sprite = sprite("blackstone_top");
                m = single(v);
            } else if (name == "dirt_path") {
                BakedVariant v = cubeAll(sprite("dirt_path_side"));
                v.faces[int(Direction::Up)].sprite = sprite("dirt_path_top");
                v.faces[int(Direction::Down)].sprite = sprite("dirt");
                m = single(v);
            } else if (name == "tnt") {
                BakedVariant v = cubeAll(sprite("tnt_side"));
                v.faces[int(Direction::Up)].sprite = sprite("tnt_top");
                v.faces[int(Direction::Down)].sprite = sprite("tnt_bottom");
                m = single(v);
            } else if (name == "podzol" || name == "mycelium") { // side, own top, dirt bottom
                BakedVariant v = cubeAll(sprite((name + "_side").c_str()));
                v.faces[int(Direction::Up)].sprite = sprite((name + "_top").c_str());
                v.faces[int(Direction::Down)].sprite = sprite("dirt");
                m = single(v);
            } else if (name.ends_with("mushroom_block") || name == "mushroom_stem") {
                // Each face shows the cap (or stem) where its property is true, the pale
                // inside where false (vanilla multipart).
                BakedVariant v = cubeAll(sprite(name.c_str()));
                static constexpr std::pair<Direction, const char*> kFaces[] = {
                    {Direction::Down, "down"},   {Direction::Up, "up"},     {Direction::North, "north"},
                    {Direction::South, "south"}, {Direction::West, "west"}, {Direction::East, "east"}};
                for (const auto& [d, prop] : kFaces)
                    if (registry.value(state, prop).value_or("true") == "false")
                        v.faces[int(d)].sprite = sprite("mushroom_block_inside");
                m = single(v);
            } else if (name == "pumpkin") { // vanilla: cube_column, the stem end on top and bottom
                m = single(cubeColumn(sprite("pumpkin_side"), sprite("pumpkin_top"), "y"));
            } else if (name == "cactus") {
                // Vanilla: sides inset 1/16 (the spikes stick out of the texture), full
                // top and bottom.
                m.visible = true;
                m.boxCount = 1;
                BakedBox& b = m.boxes[0];
                b.from[0] = 1, b.from[1] = 0, b.from[2] = 1;
                b.to[0] = 15, b.to[1] = 16, b.to[2] = 15;
                for (int d = 0; d < 6; ++d) {
                    auto& face = b.faces[d];
                    face.sprite = sprite(d == int(Direction::Up)     ? "cactus_top"
                                         : d == int(Direction::Down) ? "cactus_bottom"
                                                                     : "cactus_side");
                    face.uv[0] = 0, face.uv[1] = 0, face.uv[2] = 16, face.uv[3] = 16;
                }
            } else if (name == "sugar_cane") { // a cross, tinted like grass (vanilla)
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite("sugar_cane");
                m.crossTint = Tint::Grass;
            } else if (name == "enchanting_table" || name.ends_with("anvil")) {
                // Enchanting table: a 12/16-tall box; anvils: a full box here (vanilla:
                // a stepped model), their top worn with damage.
                const bool table = name == "enchanting_table";
                m.visible = true;
                m.boxCount = 1;
                BakedBox& b = m.boxes[0];
                b.from[0] = 0, b.from[1] = 0, b.from[2] = 0;
                b.to[0] = 16, b.to[1] = table ? 12 : 16, b.to[2] = 16;
                const uint16_t side = sprite(table ? "enchanting_table_side" : "anvil");
                const uint16_t top = sprite(table ? "enchanting_table_top" : (name + "_top").c_str());
                const uint16_t bottom = sprite(table ? "enchanting_table_bottom" : "anvil");
                for (int d = 0; d < 6; ++d) {
                    auto& face = b.faces[d];
                    face.sprite = d == int(Direction::Up) ? top : d == int(Direction::Down) ? bottom : side;
                    face.uv[0] = 0, face.uv[1] = table && d > 1 ? 4 : 0, face.uv[2] = 16, face.uv[3] = 16;
                }
                if (!table) {
                    const auto f = registry.value(state, "facing").value_or("north");
                    b.faces[int(Direction::Up)].rotation = f == "east" || f == "west" ? 1 : 0;
                }
            } else if (name == "red_bed") {
                // A 9/16-tall slab (vanilla's bed model, simplified): blanket on top, the
                // pillow on the head half; the item icon shows the side.
                const bool head = registry.value(state, "part") == "head";
                m.visible = true;
                m.boxCount = 1;
                BakedBox& b = m.boxes[0];
                b.from[0] = 0, b.from[1] = 0, b.from[2] = 0;
                b.to[0] = 16, b.to[1] = 9, b.to[2] = 16;
                for (int d = 0; d < 6; ++d) {
                    auto& face = b.faces[d];
                    face.sprite = d == int(Direction::Up) ? sprite(head ? "red_bed_head_top" : "red_bed_foot_top")
                                  : d == int(Direction::Down) ? sprite("oak_planks")
                                                              : sprite("red_bed_side");
                    face.uv[0] = 0, face.uv[1] = 0, face.uv[2] = 16, face.uv[3] = 16;
                }
                // The top turns with the bed so the pillow lies at the head's far end.
                const auto f = registry.value(state, "facing").value_or("north");
                b.faces[int(Direction::Up)].rotation = f == "east" ? 1 : f == "south" ? 2 : f == "west" ? 3 : 0;
            } else if (name == "chest") {
                // A 14/16 box (vanilla's chest model), front toward `facing`; the halves
                // of a double chest reach across to their partner (type left: the
                // partner is counter-clockwise of facing, right: clockwise).
                const auto f = registry.value(state, "facing").value_or("north");
                const auto type = registry.value(state, "type").value_or("single");
                const Direction front = f == "south" ? Direction::South
                                        : f == "west" ? Direction::West
                                        : f == "east" ? Direction::East
                                                      : Direction::North;
                m.visible = true;
                m.boxCount = 1;
                BakedBox& b = m.boxes[0];
                b.from[0] = 1, b.from[1] = 0, b.from[2] = 1;
                b.to[0] = 15, b.to[1] = 14, b.to[2] = 15;
                if (type != "single") {
                    // Clockwise of N is E, of E is S, of S is W, of W is N.
                    static constexpr Direction kCw[6] = {Direction::Down, Direction::Up, Direction::East,
                                                         Direction::West, Direction::North, Direction::South};
                    const Direction cw = kCw[int(front)];
                    const Direction side = type == "right" ? cw : static_cast<Direction>(int(cw) ^ 1);
                    const glm::ivec3 n = world::normal(side);
                    for (int a = 0; a < 3; a += 2) {
                        if (n[a] > 0) b.to[a] = 16;
                        if (n[a] < 0) b.from[a] = 0;
                    }
                }
                const uint16_t sideS = sprite("chest_side"), topS = sprite("chest_top"), frontS = sprite("chest_front");
                for (int d = 0; d < 6; ++d) {
                    auto& face = b.faces[d];
                    face.sprite = d == int(Direction::Up) || d == int(Direction::Down) ? topS
                                  : d == int(front)                                   ? frontS
                                                                                      : sideS;
                    face.uv[0] = 0, face.uv[1] = 0, face.uv[2] = 16, face.uv[3] = 16;
                }
            } else if (name == "farmland") {
                // Dirt sides; the top darkens when fully wet (moisture 7).
                BakedVariant v = cubeAll(sprite("dirt"));
                v.faces[int(Direction::Up)].sprite =
                    sprite(registry.value(state, "moisture") == "7" ? "farmland_moist" : "farmland");
                m = single(v);
            } else if (name == "fire") {
                // Placeholder: vanilla's fire is 4 inward-leaning planes (floor) or planes
                // on the burning sides; a cross of the animated fire_0 for now.
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite("fire_0");
            } else if (std::find(std::begin(kPlants), std::end(kPlants), name) != std::end(kPlants)) {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(name.c_str());
                m.crossTint = (name == "short_grass" || name == "fern") ? Tint::Grass : Tint::None;
            } else if (ends("_log")) {
                m = single(cubeColumn(sprite(name.c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (ends("_leaves")) {
                BakedVariant v = cubeAll(sprite(name.c_str()));
                for (auto& f : v.faces) // cherry leaves are pink in their texture: no biome tint
                    f.tint = name == "cherry_leaves" ? Tint::None : Tint::Foliage;
                m = single(v); // all faces drawn (fancy leaves): no cullSame
                if (name == "birch_leaves") m.fixedTintSlot = world::kBirchFoliageSlot;
                if (name == "spruce_leaves") m.fixedTintSlot = world::kSpruceFoliageSlot;

            } else if (ends("sandstone")) {
                BakedVariant v = cubeAll(sprite(name.c_str()));
                v.faces[int(Direction::Up)].sprite = sprite((name + "_top").c_str());
                v.faces[int(Direction::Down)].sprite = sprite((name + "_bottom").c_str());
                m = single(v);
            } else if (name == "crafting_table") {
                BakedVariant v = cubeAll(sprite("crafting_table_side"));
                v.faces[int(Direction::Up)].sprite = sprite("crafting_table_top");
                v.faces[int(Direction::Down)].sprite = sprite("oak_planks");
                v.faces[int(Direction::North)].sprite = sprite("crafting_table_front");
                v.faces[int(Direction::West)].sprite = sprite("crafting_table_front");
                m = single(v);
            } else if (name == "furnace") {
                // Orientable: the front faces `facing`, lit shows the burning front.
                const bool lit = registry.value(state, "lit") == "true";
                const auto facing = registry.value(state, "facing").value_or("north");
                BakedVariant v = cubeAll(sprite("furnace_side"));
                v.faces[int(Direction::Up)].sprite = sprite("furnace_top");
                v.faces[int(Direction::Down)].sprite = sprite("furnace_top");
                const Direction front = facing == "south" ? Direction::South
                                        : facing == "west" ? Direction::West
                                        : facing == "east" ? Direction::East
                                                           : Direction::North;
                v.faces[int(front)].sprite = sprite(lit ? "furnace_front_on" : "furnace_front");
                m = single(v);
            } else if (name == "snow") {
                // Layers: a box 2 texels per layer high (vanilla snow_height* models).
                const int layers = std::stoi(std::string(registry.value(state, "layers").value_or("1")));
                m.visible = true;
                m.boxCount = 1;
                BakedBox& b = m.boxes[0];
                b.from[0] = b.from[1] = b.from[2] = 0;
                b.to[0] = b.to[2] = 16;
                b.to[1] = static_cast<uint8_t>(2 * layers);
                for (int f = 0; f < 6; ++f) {
                    b.faces[f].sprite = sprite("snow");
                    const bool side = f >= 2;
                    b.faces[f].uv[0] = 0;
                    b.faces[f].uv[1] = side ? static_cast<uint8_t>(16 - 2 * layers) : 0;
                    b.faces[f].uv[2] = 16;
                    b.faces[f].uv[3] = 16;
                }
                b.faces[int(Direction::Down)].present = false; // on the ground: never seen
            } else if (name == "snow_block") {
                m = single(cubeAll(sprite("snow")));
            } else if (name == "lava") {
                m = single(cubeAll(sprite("lava_still")));
                m.fluid = true;
                m.cullSame = true;
            } else {
                m = single(cubeAll(sprite(name.c_str())));
                if (name == "ice") {
                    m.translucent = true;
                    m.cullSame = true;
                }
            }
            break;
        }
        }
    }
}

} // namespace mc::gfx
