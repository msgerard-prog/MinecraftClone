#include "rendering/BlockModels.h"

#include "world/BlockShapes.h"
#include "world/Rails.h"
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

// A box element whose faces show the part of the texture they cover (vanilla's
// default UVs from the element's position).
void addBox(BakedModel& m, int x0, int y0, int z0, int x1, int y1, int z1, uint16_t sprite) {
    if (m.boxCount >= BakedModel::kMaxBoxes) return;
    BakedBox& b = m.boxes[m.boxCount++];
    b.from[0] = uint8_t(x0), b.from[1] = uint8_t(y0), b.from[2] = uint8_t(z0);
    b.to[0] = uint8_t(x1), b.to[1] = uint8_t(y1), b.to[2] = uint8_t(z1);
    for (int d = 0; d < 6; ++d) {
        auto& f = b.faces[d];
        f.sprite = sprite;
        const auto dir = static_cast<world::Direction>(d);
        const bool vertical = dir == world::Direction::Up || dir == world::Direction::Down;
        const bool alongZ = dir == world::Direction::West || dir == world::Direction::East;
        f.uv[0] = uint8_t(alongZ ? z0 : x0), f.uv[2] = uint8_t(alongZ ? z1 : x1);
        f.uv[1] = uint8_t(vertical ? z0 : std::max(0, 16 - y1));
        f.uv[3] = uint8_t(vertical ? z1 : std::max(0, 16 - y0));
    }
}

// A box with the base block's face sprites and tints (slabs, stairs, walls - M23.1);
// UVs follow the box's position (vanilla's default element UVs).
void addBoxFrom(BakedModel& m, int x0, int y0, int z0, int x1, int y1, int z1, const BakedVariant& base,
                bool sidesOnly = false) {
    if (m.boxCount >= BakedModel::kMaxBoxes) return;
    addBox(m, x0, y0, z0, x1, y1, z1, 0);
    BakedBox& b = m.boxes[m.boxCount - 1];
    for (int d = 0; d < 6; ++d) {
        const BakedFace& f = base.faces[sidesOnly ? int(world::Direction::North) : d];
        b.faces[d].sprite = f.sprite;
        b.faces[d].tint = f.tint;
    }
}

// Models of the shaped families from their base block's (already baked) model.
bool bakeFamilyModel(const world::BlockRegistry& registry, world::BlockStateId state, const BakedModel& baseModel,
                     BakedModel& m, const TextureAtlas& atlas) {
    using namespace world;
    const BlockId b = registry.blockOf(state);
    const BakedVariant& base = baseModel.variants[0];
    switch (registry.kind(b)) {
    case BlockKind::Slab: {
        const int t = registry.get(state, properties::slabType);
        if (t == 2) { // double: the full block
            m = baseModel;
            return true;
        }
        m.visible = true;
        addBoxFrom(m, 0, t == 0 ? 8 : 0, 0, 16, t == 0 ? 16 : 8, 16, base);
        return true;
    }
    case BlockKind::Stairs: {
        m.visible = true;
        const BlockShape sh = stairShapeOf(state);
        for (int i = 0; i < sh.count; ++i) {
            const ShapeBox& x = sh.boxes[size_t(i)];
            addBoxFrom(m, x.from[0], x.from[1], x.from[2], x.to[0], x.to[1], x.to[2], base);
        }
        return true;
    }
    case BlockKind::Wall: {
        // The post 16 tall, arms 14 (low) or 16 (tall) high, 6 wide (wiki: Wall); the
        // base's side texture on every face, as vanilla's wall models.
        m.visible = true;
        auto armHeight = [&](const Property& p) {
            const int v = registry.get(state, p);
            return v == 0 ? 0 : v == 1 ? 14 : 16;
        };
        if (registry.get(state, properties::fireUp) == 0) addBoxFrom(m, 4, 0, 4, 12, 16, 12, base, true);
        if (const int h = armHeight(properties::wallNorth)) addBoxFrom(m, 5, 0, 0, 11, h, 8, base, true);
        if (const int h = armHeight(properties::wallSouth)) addBoxFrom(m, 5, 0, 8, 11, h, 16, base, true);
        if (const int h = armHeight(properties::wallWest)) addBoxFrom(m, 0, 0, 5, 8, h, 11, base, true);
        if (const int h = armHeight(properties::wallEast)) addBoxFrom(m, 8, 0, 5, 16, h, 11, base, true);
        if (m.boxCount == 0) addBoxFrom(m, 4, 0, 4, 12, 16, 12, base, true);
        return true;
    }
    case BlockKind::Pane: {
        // A 2-wide post and arms with the glass texture; the edges show the pane's
        // "_top" strip (wiki: Glass Pane).
        m.visible = true;
        m.translucent = baseModel.translucent;
        std::string name = registry.block(b).id.substr(10);
        const uint16_t edge = static_cast<uint16_t>(atlas.spriteIndex(name + "_top"));
        auto arm = [&](int x0, int z0, int x1, int z1, bool alongX) {
            addBoxFrom(m, x0, 0, z0, x1, 16, z1, base);
            BakedBox& bx = m.boxes[m.boxCount - 1];
            for (const Direction d : {Direction::Up, Direction::Down}) {
                auto& f = bx.faces[int(d)];
                f.sprite = edge;
                f.uv[0] = 7, f.uv[2] = 9; // the strip runs along the arm
                f.uv[1] = uint8_t(alongX ? x0 : z0), f.uv[3] = uint8_t(alongX ? x1 : z1);
                f.rotation = alongX ? 1 : 0;
            }
        };
        arm(7, 7, 9, 9, false);
        if (registry.get(state, properties::fireNorth) == 0) arm(7, 0, 9, 7, false);
        if (registry.get(state, properties::fireSouth) == 0) arm(7, 9, 9, 16, false);
        if (registry.get(state, properties::fireWest) == 0) arm(0, 7, 7, 9, true);
        if (registry.get(state, properties::fireEast) == 0) arm(9, 7, 16, 9, true);
        return true;
    }
    case BlockKind::Carpet: // a 1-pixel slab of its wool (wiki: Carpet)
        m.visible = true;
        addBoxFrom(m, 0, 0, 0, 16, 1, 16, base);
        return true;
    case BlockKind::Sign:
    case BlockKind::WallSign:
    case BlockKind::HangingSign:
    case BlockKind::WallHangingSign: {
        // Boards of the wood's planks (hanging signs: its stripped log) - standing
        // signs on a post, turned to the nearest quarter of their 16 directions (box
        // models can't turn 22.5 degrees: known deviation); wall signs flat on the
        // wall; hanging signs under two chains (wiki: Sign, Hanging Sign).
        m.visible = true;
        const BlockKind k = registry.kind(b);
        std::string name = registry.block(b).id.substr(10);
        std::string woodName = name.substr(0, name.find(k == BlockKind::Sign || k == BlockKind::WallSign
                                                            ? (k == BlockKind::WallSign ? "_wall_sign" : "_sign")
                                                            : (k == BlockKind::HangingSign ? "_hanging_sign" : "_wall_hanging_sign")));
        const bool nether = woodName == "crimson" || woodName == "warped";
        const std::string logTex = woodName == "bamboo" ? "stripped_bamboo_block"
                                                        : "stripped_" + woodName + (nether ? "_stem" : "_log");
        BakedVariant hang = base;
        for (auto& f : hang.faces)
            f.sprite = static_cast<uint16_t>(atlas.spriteIndex(logTex));
        // Facing as quarter turns from north: wall kinds by facing, standing by rotation.
        int quarter = 0; // 0: board faces south (built so), 1 west, 2 north, 3 east
        if (k == BlockKind::WallSign || k == BlockKind::WallHangingSign) {
            const int f = registry.get(state, properties::facing); // north, south, west, east
            quarter = f == 0 ? 2 : f == 1 ? 0 : f == 2 ? 1 : 3;
        } else {
            quarter = ((registry.get(state, properties::rotation16) + 2) / 4) & 3;
        }
        // Boxes built for a board facing south, then turned.
        auto put = [&](int x0, int y0, int z0, int x1, int y1, int z1, const BakedVariant& tex) {
            int a[3] = {x0, y0, z0}, c[3] = {x1, y1, z1};
            for (int q = 0; q < quarter; ++q) { // turn 90 degrees about y: (x, z) -> (16 - z, x)
                const int ax = a[0], az = a[2], cx = c[0], cz = c[2];
                a[0] = 16 - az, a[2] = ax;
                c[0] = 16 - cz, c[2] = cx;
            }
            addBoxFrom(m, std::min(a[0], c[0]), a[1], std::min(a[2], c[2]), std::max(a[0], c[0]), c[1],
                       std::max(a[2], c[2]), tex);
        };
        switch (k) {
        case BlockKind::Sign:
            put(0, 8, 7, 16, 16, 9, base); // the board
            put(7, 0, 7, 9, 8, 9, base);   // the post
            break;
        case BlockKind::WallSign: put(0, 4, 0, 16, 12, 2, base); break; // against the north wall
        case BlockKind::HangingSign:
            put(1, 0, 7, 15, 10, 9, hang);
            put(3, 10, 7, 4, 16, 9, hang); // the two chains
            put(12, 10, 7, 13, 16, 9, hang);
            break;
        default: // wall hanging: the board under a bar into the wall
            put(1, 0, 7, 15, 10, 9, hang);
            put(0, 14, 6, 16, 16, 10, hang);
            put(3, 10, 7, 4, 14, 9, hang);
            put(12, 10, 7, 13, 14, 9, hang);
            break;
        }
        return true;
    }
    case BlockKind::Plain: break;
    }
    return false;
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
        if (const BlockId fb = registry.blockOf(state); registry.kind(fb) != world::BlockKind::Plain &&
                                                        bakeFamilyModel(registry, state,
                                                                        m_models[registry.defaultState(registry.block(fb).settings.base)], m,
                                                                        atlas))
            continue;
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
            if (name.starts_with("waxed_")) name.erase(0, 6); // (waxed copper looks the same)
            const auto ends = [&](std::string_view s) { return name.ends_with(s); };
            static constexpr std::string_view kPlants[] = {
                "short_grass", "fern", "dandelion", "poppy", "cornflower", "azure_bluet",
                "oxeye_daisy", "dead_bush", "oak_sapling", "birch_sapling", "spruce_sapling", "acacia_sapling",
                "brown_mushroom", "red_mushroom", "jungle_sapling", "dark_oak_sapling", "cherry_sapling",
                "crimson_fungus", "warped_fungus", "crimson_roots", "warped_roots", "nether_sprouts",
                "weeping_vines", "weeping_vines_plant", "twisting_vines", "twisting_vines_plant",
                "mangrove_propagule", "pale_oak_sapling"};
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
            } else if (name == "chiseled_sandstone" || name == "cut_sandstone" || name == "smooth_sandstone" ||
                       name == "chiseled_red_sandstone" || name == "cut_red_sandstone" ||
                       name == "smooth_red_sandstone") {
                // Vanilla: the plain sandstone top on top and bottom (smooth: everywhere).
                const std::string topName = name.find("red_") != std::string::npos ? "red_sandstone_top" : "sandstone_top";
                BakedVariant v = cubeAll(sprite(name.starts_with("smooth_") ? topName.c_str() : name.c_str()));
                v.faces[int(Direction::Up)].sprite = sprite(topName.c_str());
                v.faces[int(Direction::Down)].sprite = sprite(topName.c_str());
                m = single(v);
            } else if (name == "campfire" || name == "soul_campfire") {
                // Two logs one way under two the other way, embers between, and the
                // flames as a cross when lit (wiki: Campfire).
                const bool on = registry.value(state, "lit") == "true";
                const bool soul = name == "soul_campfire";
                m.visible = true;
                const uint16_t log = sprite("campfire_log");
                const uint16_t burning = sprite(soul ? "soul_campfire_log_lit" : "campfire_log_lit");
                addBox(m, 1, 0, 0, 5, 4, 16, log);
                addBox(m, 11, 0, 0, 15, 4, 16, log);
                addBox(m, 0, 3, 1, 16, 7, 5, on ? burning : log);
                addBox(m, 0, 3, 11, 16, 7, 15, on ? burning : log);
                addBox(m, 5, 0, 5, 11, 1, 11, on ? burning : log); // the embers
                if (on) {
                    m.cross = true;
                    m.crossSprite = sprite(soul ? "soul_campfire_fire" : "campfire_fire");
                }
            } else if (ends("copper_bulb")) { // lit and powered textures (wiki: Copper Bulb)
                const bool on = registry.value(state, "lit") == "true", pow = registry.value(state, "powered") == "true";
                m = single(cubeAll(sprite((name + (on ? "_lit" : "") + (pow ? "_powered" : "")).c_str())));
            } else if (ends("_glazed_terracotta")) { // the pattern turns with its facing (vanilla)
                const std::string_view f = registry.value(state, "facing").value_or("north");
                const int q = f == "east" ? 1 : f == "south" ? 2 : f == "west" ? 3 : 0;
                m = single(rotateY(cubeAll(sprite(name.c_str())), q));
            } else if (ends("_stained_glass")) { // translucent, faces hidden against itself (as glass)
                m = single(cubeAll(sprite(name.c_str())));
                m.translucent = true;
                m.cullSame = true;
            } else if (name == "smooth_quartz") { // vanilla: the quartz block's bottom on every face
                m = single(cubeAll(sprite("quartz_block_bottom")));
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
            } else if (name.ends_with("_door") || name.ends_with("_trapdoor")) {
                // The same panel as its collision shape (world/BlockShapes).
                const bool door = name.ends_with("_door");
                const bool upper = door && registry.value(state, "half").value_or("lower") == "upper";
                const uint16_t sp = sprite((door ? name + (upper ? "_top" : "_bottom") : name).c_str());
                m.visible = true;
                const world::BlockShape& sh = world::collisionShape(state);
                for (int i = 0; i < sh.count; ++i) {
                    const auto& b = sh.boxes[size_t(i)];
                    addBox(m, b.from[0], b.from[1], b.from[2], b.to[0], b.to[1], b.to[2], sp);
                }
            } else if (ends("_fence") && name != "nether_brick_fence") { // a post with two rails to each neighbour
                const uint16_t sp = sprite((name.substr(0, name.size() - 6) + "_planks").c_str());
                m.visible = true;
                addBox(m, 6, 0, 6, 10, 16, 10, sp);
                auto on = [&](const char* p) { return registry.value(state, p).value_or("false") == "true"; };
                for (const int y : {6, 12}) {
                    if (on("north")) addBox(m, 7, y, 0, 9, y + 3, 6, sp);
                    if (on("south")) addBox(m, 7, y, 10, 9, y + 3, 16, sp);
                    if (on("west")) addBox(m, 0, y, 7, 6, y + 3, 9, sp);
                    if (on("east")) addBox(m, 10, y, 7, 16, y + 3, 9, sp);
                }
            } else if (ends("_fence_gate")) {
                // Two posts and two rails across; open, the rails fold back to the posts.
                const uint16_t sp = sprite((name.substr(0, name.size() - 11) + "_planks").c_str());
                const std::string_view f = registry.value(state, "facing").value_or("north");
                const bool alongX = f == "north" || f == "south"; // (the gate spans x)
                const bool isOpen = registry.value(state, "open").value_or("false") == "true";
                m.visible = true;
                auto put = [&](int a0, int y0, int b0, int a1, int y1, int b1) { // a: across, b: through
                    if (alongX) addBox(m, a0, y0, b0, a1, y1, b1, sp);
                    else addBox(m, b0, y0, a0, b1, y1, a1, sp);
                };
                put(0, 5, 7, 2, 16, 9);
                put(14, 5, 7, 16, 16, 9);
                for (const int y : {6, 12}) {
                    if (!isOpen) {
                        put(2, y, 7, 14, y + 3, 9);
                    } else {
                        put(0, y, 9, 2, y + 3, 15);
                        put(14, y, 9, 16, y + 3, 15);
                    }
                }
            } else if (name == "rail" || name.ends_with("_rail")) {
                // A flat strip 1/16 thick (vanilla: a plane); ascending rails as four
                // thin steps (vanilla: a sloped plane). Curves turn the corner texture
                // (unturned: south-east, as vanilla's model).
                const int shape = world::railShapeOf(state);
                const bool on = registry.value(state, "powered").value_or("false") == "true";
                std::string tex = name == "rail" ? (shape >= 6 ? "rail_corner" : "rail") : (on ? name + "_on" : name);
                const uint16_t sp = sprite(tex.c_str());
                m.visible = true;
                static constexpr uint8_t kTurns[10] = {0, 1, 1, 1, 0, 0, 0, 1, 2, 3};
                auto top = [&](int i) {
                    BakedBox& b = m.boxes[size_t(i)];
                    for (auto& f : b.faces)
                        f.present = false;
                    auto& t = b.faces[int(world::Direction::Up)];
                    t.present = true;
                    t.rotation = kTurns[shape];
                };
                if (shape < 2 || shape >= 6) {
                    addBox(m, 0, 0, 0, 16, 1, 16, sp);
                    top(0);
                    m.boxes[0].faces[int(world::Direction::Up)].uv[0] = 0, m.boxes[0].faces[int(world::Direction::Up)].uv[1] = 0;
                    m.boxes[0].faces[int(world::Direction::Up)].uv[2] = 16, m.boxes[0].faces[int(world::Direction::Up)].uv[3] = 16;
                } else {
                    for (int k = 0; k < 4; ++k) { // step k rises 4 more toward the high side
                        const int lo = 4 * k, hi = lo + 4, y = 4 * k;
                        switch (shape) {
                        case 2: addBox(m, lo, y, 0, hi, y + 1, 16, sp); break;           // up to the east
                        case 3: addBox(m, 16 - hi, y, 0, 16 - lo, y + 1, 16, sp); break; // up to the west
                        case 4: addBox(m, 0, y, 16 - hi, 16, y + 1, 16 - lo, sp); break; // up to the north
                        default: addBox(m, 0, y, lo, 16, y + 1, hi, sp); break;          // up to the south
                        }
                        top(k);
                    }
                }
            } else if (name.ends_with("_pressure_plate")) {
                // The material it's made of (wiki: Pressure Plate): planks, stone, gold, iron.
                const std::string prefix = name.substr(0, name.size() - 15);
                const std::string tex = prefix == "stone" || prefix == "polished_blackstone" ? prefix
                                        : prefix == "light_weighted"                     ? "gold_block"
                                        : prefix == "heavy_weighted"                     ? "iron_block"
                                                                                         : prefix + "_planks";
                m.visible = true;
                addBox(m, 1, 0, 1, 15, 1, 15, sprite(tex.c_str()));
            } else if (name == "end_rod") {
                // A 2x15 rod on a 4x1 base, built pointing up, then turned to its facing.
                const std::string_view f = registry.value(state, "facing").value_or("up");
                auto map = [&](int x, int y, int z, int out[3]) {
                    if (f == "down") out[0] = x, out[1] = 16 - y, out[2] = 16 - z;
                    else if (f == "north") out[0] = x, out[1] = z, out[2] = 16 - y;
                    else if (f == "south") out[0] = x, out[1] = z, out[2] = y;
                    else if (f == "west") out[0] = 16 - y, out[1] = z, out[2] = x;
                    else if (f == "east") out[0] = y, out[1] = z, out[2] = x;
                    else out[0] = x, out[1] = y, out[2] = z;
                };
                static constexpr int kParts[2][6] = {{6, 0, 6, 10, 1, 10}, {7, 1, 7, 9, 16, 9}};
                const uint16_t sp = sprite("end_rod");
                m.visible = true;
                for (int k = 0; k < 2; ++k) {
                    int a[3], b[3];
                    map(kParts[k][0], kParts[k][1], kParts[k][2], a);
                    map(kParts[k][3], kParts[k][4], kParts[k][5], b);
                    BakedBox& box = m.boxes[m.boxCount++];
                    for (int i = 0; i < 3; ++i) {
                        box.from[i] = uint8_t(std::min(a[i], b[i]));
                        box.to[i] = uint8_t(std::max(a[i], b[i]));
                    }
                    for (int d = 0; d < 6; ++d) { // (the rod's column, or the base row)
                        auto& face = box.faces[d];
                        face.sprite = sp;
                        if (k == 0) face.uv[0] = 6, face.uv[1] = 15, face.uv[2] = 10, face.uv[3] = 16;
                        else face.uv[0] = 7, face.uv[1] = 1, face.uv[2] = 9, face.uv[3] = 15;
                    }
                }
            } else if (name == "moving_piston") { // (its block is drawn moving by the entity renderer)
                m.visible = false;
            } else if (name == "end_gateway") { // (vanilla: the end portal's starfield on every side)
                m = single(cubeAll(sprite("end_portal")));
            } else if (name == "dragon_egg") {
                // An egg of stacked layers, widest low down (vanilla: eight layers).
                static constexpr uint8_t kLayers[6][3] = {{4, 0, 1}, {3, 1, 6}, {4, 6, 10}, {5, 10, 13}, {6, 13, 15}, {7, 15, 16}};
                m.visible = true;
                const uint16_t sp = sprite("dragon_egg");
                for (const auto& l : kLayers) {
                    BakedBox& b = m.boxes[m.boxCount++];
                    const uint8_t lo = l[0], hi = uint8_t(16 - l[0]);
                    b.from[0] = lo, b.from[1] = l[1], b.from[2] = lo;
                    b.to[0] = hi, b.to[1] = l[2], b.to[2] = hi;
                    for (int d = 0; d < 6; ++d) {
                        auto& f = b.faces[d];
                        f.sprite = sp;
                        const bool vertical = d < 2;
                        f.uv[0] = lo, f.uv[2] = hi;
                        f.uv[1] = vertical ? lo : uint8_t(16 - l[2]);
                        f.uv[3] = vertical ? hi : uint8_t(16 - l[1]);
                    }
                }
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
            } else if (name == "carved_pumpkin") { // the face toward `facing`
                const auto facing = registry.value(state, "facing").value_or("north");
                BakedVariant v = cubeColumn(sprite("pumpkin_side"), sprite("pumpkin_top"), "y");
                const Direction front = facing == "south" ? Direction::South
                                        : facing == "west" ? Direction::West
                                        : facing == "east" ? Direction::East
                                                           : Direction::North;
                v.faces[int(front)].sprite = sprite("carved_pumpkin");
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
            } else if (name == "chest" || name == "ender_chest") {
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
                const std::string tex = name; // (ender chests: the same box, their own faces)
                const uint16_t sideS = sprite((tex + "_side").c_str()), topS = sprite((tex + "_top").c_str()),
                               frontS = sprite((tex + "_front").c_str());
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
            } else if (name == "kelp" || name == "kelp_plant" || name == "seagrass" || ends("_coral") ||
                       ends("_coral_fan")) {
                // Ocean plants (M25.1): crosses, drawn over the water they stand in. Coral
                // fans are a cross here (vanilla: four tilted planes).
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(name.c_str());
            } else if (name == "tall_seagrass") {
                m.visible = true;
                m.cross = true;
                m.crossSprite =
                    sprite(registry.value(state, "half") == "upper" ? "tall_seagrass_top" : "tall_seagrass_bottom");
            } else if (name == "sea_pickle") {
                // 1-4 small pickles (4x6x4, vanilla's size) on the floor.
                static constexpr int kAt[4][4][2] = {{{6, 6}}, {{3, 4}, {9, 9}}, {{3, 3}, {9, 4}, {6, 10}},
                                                     {{3, 3}, {10, 3}, {3, 10}, {10, 10}}};
                const int n = std::stoi(std::string(registry.value(state, "pickles").value_or("1")));
                m.visible = true;
                for (int k = 0; k < n; ++k)
                    addBox(m, kAt[n - 1][k][0], 0, kAt[n - 1][k][1], kAt[n - 1][k][0] + 4, 6,
                           kAt[n - 1][k][1] + 4, sprite("sea_pickle"));
            } else if (name == "dried_kelp_block") {
                BakedVariant v = cubeAll(sprite("dried_kelp_side"));
                v.faces[int(Direction::Up)].sprite = sprite("dried_kelp_top");
                v.faces[int(Direction::Down)].sprite = sprite("dried_kelp_bottom");
                m = single(v);
            } else if (std::find(std::begin(kPlants), std::end(kPlants), name) != std::end(kPlants)) {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(name.c_str());
                m.crossTint = (name == "short_grass" || name == "fern") ? Tint::Grass : Tint::None;
            } else if (ends("_log") || name == "bamboo_block" || name == "stripped_bamboo_block") {
                m = single(cubeColumn(sprite(name.c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (ends("_wood") || ends("_hyphae")) {
                // Bark on every face (vanilla: the log's side texture all round).
                const std::string side = ends("_wood") ? name.substr(0, name.size() - 5) + "_log"
                                                       : name.substr(0, name.size() - 7) + "_stem";
                m = single(cubeColumn(sprite(side.c_str()), sprite(side.c_str()), registry.value(state, "axis").value_or("y")));
            } else if (name == "bamboo") {
                // A 3-wide stalk (vanilla: 3x16x3, offset per position; leaves on top as
                // crossed planes, which ours doesn't draw yet).
                m.visible = true;
                addBox(m, 6, 0, 6, 9, 16, 9, sprite("bamboo_stalk"));
                for (auto& f : m.boxes[0].faces) {
                    f.uv[0] = 2, f.uv[2] = 5; // (our art: a stalk at texels 2..5)
                }
            } else if (ends("_leaves")) {
                BakedVariant v = cubeAll(sprite(name.c_str()));
                for (auto& f : v.faces) // cherry leaves are pink in their texture: no biome tint
                    f.tint = name == "cherry_leaves" || name == "pale_oak_leaves" ? Tint::None : Tint::Foliage;
                m = single(v); // all faces drawn (fancy leaves): no cullSame
                if (name == "birch_leaves") m.fixedTintSlot = world::kBirchFoliageSlot;
                if (name == "spruce_leaves") m.fixedTintSlot = world::kSpruceFoliageSlot;

            } else if (ends("sandstone")) {
                BakedVariant v = cubeAll(sprite(name.c_str()));
                v.faces[int(Direction::Up)].sprite = sprite((name + "_top").c_str());
                v.faces[int(Direction::Down)].sprite = sprite((name + "_bottom").c_str());
                m = single(v);
            } else if (name == "fletching_table") { // (fronts north and south)
                BakedVariant v = cubeAll(sprite("fletching_table_side"));
                v.faces[int(Direction::Up)].sprite = sprite("fletching_table_top");
                v.faces[int(Direction::Down)].sprite = sprite("birch_planks");
                v.faces[int(Direction::North)].sprite = sprite("fletching_table_front");
                v.faces[int(Direction::South)].sprite = sprite("fletching_table_front");
                m = single(v);
            } else if (name == "jukebox") {
                BakedVariant v = cubeAll(sprite("jukebox_side"));
                v.faces[int(Direction::Up)].sprite = sprite("jukebox_top");
                m = single(v);
            } else if (name == "ancient_debris") {
                m = single(cubeColumn(sprite("ancient_debris_side"), sprite("ancient_debris_top"), "y"));
            } else if (name == "smithing_table") { // (fronts on the north and south sides)
                BakedVariant v = cubeAll(sprite("smithing_table_side"));
                v.faces[int(Direction::Up)].sprite = sprite("smithing_table_top");
                v.faces[int(Direction::Down)].sprite = sprite("smithing_table_bottom");
                v.faces[int(Direction::North)].sprite = sprite("smithing_table_front");
                v.faces[int(Direction::South)].sprite = sprite("smithing_table_front");
                m = single(v);
            } else if (name == "cartography_table") { // (three different sides)
                BakedVariant v = cubeAll(sprite("cartography_table_side3"));
                v.faces[int(Direction::Up)].sprite = sprite("cartography_table_top");
                v.faces[int(Direction::Down)].sprite = sprite("dark_oak_planks");
                v.faces[int(Direction::South)].sprite = sprite("cartography_table_side1");
                v.faces[int(Direction::West)].sprite = sprite("cartography_table_side2");
                m = single(v);
            } else if (name == "loom") { // the front toward `facing`
                const auto facing = registry.value(state, "facing").value_or("north");
                BakedVariant v = cubeAll(sprite("loom_side"));
                v.faces[int(Direction::Up)].sprite = sprite("loom_top");
                v.faces[int(Direction::Down)].sprite = sprite("loom_bottom");
                const Direction front = facing == "south" ? Direction::South
                                        : facing == "west" ? Direction::West
                                        : facing == "east" ? Direction::East
                                                           : Direction::North;
                v.faces[int(front)].sprite = sprite("loom_front");
                m = single(v);
            } else if (name == "crafting_table") {
                BakedVariant v = cubeAll(sprite("crafting_table_side"));
                v.faces[int(Direction::Up)].sprite = sprite("crafting_table_top");
                v.faces[int(Direction::Down)].sprite = sprite("oak_planks");
                v.faces[int(Direction::North)].sprite = sprite("crafting_table_front");
                v.faces[int(Direction::West)].sprite = sprite("crafting_table_front");
                m = single(v);
            } else if (name == "furnace" || name == "smoker" || name == "blast_furnace") {
                // Orientable: the front faces `facing`, lit shows the burning front.
                const bool lit = registry.value(state, "lit") == "true";
                const auto facing = registry.value(state, "facing").value_or("north");
                const std::string n(name);
                BakedVariant v = cubeAll(sprite((n + "_side").c_str()));
                v.faces[int(Direction::Up)].sprite = sprite((n + "_top").c_str());
                v.faces[int(Direction::Down)].sprite = sprite(name == "smoker" ? "smoker_bottom" : (n + "_top").c_str());
                const Direction front = facing == "south" ? Direction::South
                                        : facing == "west" ? Direction::West
                                        : facing == "east" ? Direction::East
                                                           : Direction::North;
                v.faces[int(front)].sprite = sprite((n + (lit ? "_front_on" : "_front")).c_str());
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
            } else if (!atlas.has(name) && atlas.has(name + "_side") && registry.value(state, "axis") &&
                       atlas.has(name + "_top")) {
                // A pillar with "_side"/"_top" textures (hay bale).
                m = single(cubeColumn(sprite((name + "_side").c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (!atlas.has(name) && atlas.has(name + "_side")) {
                // <name>_side / _top / _bottom textures (quartz block...)
                BakedVariant v = cubeAll(sprite((name + "_side").c_str()));
                const uint16_t top = atlas.has(name + "_top") ? sprite((name + "_top").c_str()) : uint16_t(0);
                const uint16_t bottom = atlas.has(name + "_bottom") ? sprite((name + "_bottom").c_str()) : uint16_t(0);
                v.faces[int(Direction::Up)].sprite = top ? top : v.faces[0].sprite;
                v.faces[int(Direction::Down)].sprite = bottom ? bottom : (top ? top : v.faces[0].sprite);
                m = single(v);
            } else if (registry.value(state, "axis") && atlas.has(name + "_top")) {
                // A pillar (vanilla cube_column): quartz pillar...
                m = single(cubeColumn(sprite(name.c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name.starts_with("chiseled_") && atlas.has(name + "_top")) {
                // Chiseled blocks with their own top (chiseled tuff, chiseled quartz).
                BakedVariant v = cubeAll(sprite(name.c_str()));
                v.faces[int(Direction::Up)].sprite = sprite((name + "_top").c_str());
                v.faces[int(Direction::Down)].sprite = sprite((name + "_top").c_str());
                m = single(v);
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
    // Box models whose item looks like the block (vanilla's block-shaped GUI models).
    for (size_t s = 0; s < registry.stateCount(); ++s) {
        BakedModel& m = m_models[s];
        if (m.boxCount == 0 || m.cross) continue;
        const BlockId b = registry.blockOf(static_cast<BlockStateId>(s));
        const BlockKind k = registry.kind(b);
        const BlockId like = registry.likeOf(b);
        m.icon3d = k == BlockKind::Slab || k == BlockKind::Stairs || k == BlockKind::Wall || k == BlockKind::Carpet ||
                   like == blocks::OakFence || like == blocks::NetherBrickFence || like == blocks::OakFenceGate ||
                   like == blocks::Composter || like == blocks::Stonecutter || like == blocks::Grindstone ||
                   like == blocks::Chest || like == blocks::EnderChest || like == blocks::EnchantingTable || like == blocks::OakTrapdoor ||
                   like == blocks::IronTrapdoor || like == blocks::Snow || like == blocks::OakPressurePlate ||
                   like == blocks::StonePressurePlate || like == blocks::LightWeightedPressurePlate ||
                   like == blocks::HeavyWeightedPressurePlate || like == blocks::Campfire || like == blocks::SoulCampfire;
    }
}

} // namespace mc::gfx
