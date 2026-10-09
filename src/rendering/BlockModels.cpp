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
    case BlockKind::Banner: // (M28.3d) drawn each frame by EntityRenderer::addBanner (its layers)
    case BlockKind::WallBanner:
        m.visible = false;
        return true;
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

// Models of the blocks M29 added (out of bake()'s name chain: MSVC limits how deeply
// an if-else chain may nest). True when `name` was one of them.
bool BlockModels::bakeLateModel(const world::BlockRegistry& registry, world::BlockStateId state, const std::string& name,
                                const TextureAtlas& atlas, BakedModel& m) const {
    using namespace world;
    auto sprite = [&](const char* n) { return static_cast<uint16_t>(atlas.spriteIndex(n)); };
    if (name == "lily_pad") { // (M29.4a) a pad lying on the water, 1/16 thick (ours)
        m.visible = true;
        addBox(m, 0, 0, 0, 16, 1, 16, sprite("lily_pad"));
        // vanilla tints it a fixed green (#208030); ours takes the biome's foliage
        for (auto& f : m.boxes[m.boxCount - 1].faces) f.tint = Tint::Foliage;
    } else if (name == "flower_pot" || name.starts_with("potted_")) {
        // (M29.4b; wiki: Flower Pot) a 6x6x6 pot, soil on top when planted; the plant
        // is its own cross (vanilla lifts it 4 pixels; ours stands in the pot), cacti
        // and bamboo a thin column, azaleas a small bush.
        m.visible = true;
        addBox(m, 5, 0, 5, 11, 6, 11, sprite("flower_pot"));
        const BlockId plant = plantInPot(registry.blockOf(state));
        if (plant) m.boxes[0].faces[int(Direction::Up)].sprite = sprite("dirt");
        const std::string plantName = plant ? registry.block(plant).id.substr(10) : std::string();
        if (plantName == "cactus") {
            addBox(m, 6, 6, 6, 10, 16, 10, sprite("cactus_side"));
            m.boxes[1].faces[int(Direction::Up)].sprite = sprite("cactus_top");
        } else if (plantName == "bamboo") {
            addBox(m, 7, 6, 7, 9, 16, 9, sprite("bamboo_stalk"));
        } else if (plantName == "azalea" || plantName == "flowering_azalea") {
            addBox(m, 4, 6, 4, 12, 14, 12, sprite((plantName + "_side").c_str()));
            m.boxes[1].faces[int(Direction::Up)].sprite = sprite((plantName + "_top").c_str());
        } else if (plantName == "crimson_roots" || plantName == "warped_roots") {
            m.cross = true, m.crossSprite = sprite((plantName + "_pot").c_str());
        } else if (plant) {
            const BakedModel& pm = m_models[registry.defaultState(plant)];
            m.cross = pm.cross, m.crossSprite = pm.crossSprite, m.crossTint = pm.crossTint;
        }
    } else if (name == "soul_fire") { // (M29.4c) as fire: a cross of its animated texture
        m.visible = true;
        m.cross = true;
        m.crossSprite = sprite("soul_fire_0");
    } else if (name == "bamboo_sapling") { // (M29.4c; vanilla: a cross of bamboo_stage0)
        m.visible = true;
        m.cross = true;
        m.crossSprite = sprite("bamboo_stage0");
    } else if (name.ends_with("_coral_wall_fan")) {
        // (M29.4c) vanilla: two planes tilted out of the wall; ours: one flat plane
        // standing 8 pixels out from the wall, seen from above and below.
        m.visible = true;
        std::string fan = name;
        fan.erase(fan.rfind("_wall"), 5);
        const uint16_t sp = sprite(fan.c_str());
        const auto f = registry.value(state, "facing").value_or("north"); // out of the wall
        if (f == "north") addBox(m, 0, 4, 8, 16, 4, 16, sp);
        else if (f == "south") addBox(m, 0, 4, 0, 16, 4, 8, sp);
        else if (f == "west") addBox(m, 8, 4, 0, 16, 4, 16, sp);
        else addBox(m, 0, 4, 0, 8, 4, 16, sp);
        for (int d = 0; d < 6; ++d)
            m.boxes[0].faces[d].present = d == int(Direction::Up) || d == int(Direction::Down);
    } else if (name == "tripwire") {
        // (M29.5) flat strips toward its connections (both ways when it has none),
        // 1 pixel up while attached, 2 when loose (vanilla: 1.5 / 2.5).
        m.visible = true;
        const uint16_t sp = sprite("tripwire");
        const int y = registry.value(state, "attached") == "true" ? 1 : 2;
        auto on = [&](const char* d) { return registry.value(state, d) == "true"; };
        const bool n = on("north"), so = on("south"), w = on("west"), e = on("east");
        const bool any = n || so || w || e;
        auto strip = [&](int x0, int z0, int x1, int z1, bool alongZ) {
            addBox(m, x0, y, z0, x1, y, z1, sp);
            for (int d = 0; d < 6; ++d) {
                auto& f = m.boxes[m.boxCount - 1].faces[d];
                f.present = d == int(Direction::Up) || d == int(Direction::Down);
                // the texture's line is its rows 7-8: along z, turn it a quarter
                if (alongZ) f.uv[0] = 0, f.uv[1] = 7, f.uv[2] = 16, f.uv[3] = 8, f.rotation = 1;
                else f.uv[1] = 7, f.uv[3] = 8;
            }
        };
        if (w || e) strip(w ? 0 : 8, 7, e ? 16 : 8, 8, false);
        if (n || so || !any) strip(7, n || !any ? 0 : 8, 8, so || !any ? 16 : 8, true);
    } else if (name == "tripwire_hook") {
        // (M29.5) a post on the wall and a ring reaching out (tilted down when not
        // attached, vanilla; ours stays level)
        m.visible = true;
        const uint16_t sp = sprite("tripwire_hook"), ring = sprite("iron_block");
        const auto fc = registry.value(state, "facing").value_or("north"); // out of the wall
        if (fc == "north") addBox(m, 7, 2, 14, 9, 12, 16, sp), addBox(m, 7, 6, 10, 9, 8, 14, ring);
        else if (fc == "south") addBox(m, 7, 2, 0, 9, 12, 2, sp), addBox(m, 7, 6, 2, 9, 8, 6, ring);
        else if (fc == "west") addBox(m, 14, 2, 7, 16, 12, 9, sp), addBox(m, 10, 6, 7, 14, 8, 9, ring);
        else addBox(m, 0, 2, 7, 2, 12, 9, sp), addBox(m, 2, 6, 7, 6, 8, 9, ring);
    } else if (name == "daylight_detector") { // (M29.5) a 6/16 slab, glass on top
        m.visible = true;
        addBox(m, 0, 0, 0, 16, 6, 16, sprite("daylight_detector_side"));
        m.boxes[0].faces[int(Direction::Up)].sprite =
            sprite(registry.value(state, "inverted") == "true" ? "daylight_detector_inverted_top" : "daylight_detector_top");
        m.boxes[0].faces[int(Direction::Up)].uv[1] = 0, m.boxes[0].faces[int(Direction::Up)].uv[3] = 16;
    } else if (name.ends_with("lightning_rod")) {
        // (M29.5) a 2x2 rod with a 4x4 tip pointing out of its face (vanilla: the same shape);
        // struck, it shows the bright "on" texture.
        m.visible = true;
        const std::string tex = registry.value(state, "powered") == "true" ? "lightning_rod_on" : name;
        const uint16_t sp = sprite(tex.c_str());
        const auto f = registry.value(state, "facing").value_or("up");
        if (f == "up") addBox(m, 7, 0, 7, 9, 12, 9, sp), addBox(m, 6, 12, 6, 10, 16, 10, sp);
        else if (f == "down") addBox(m, 7, 4, 7, 9, 16, 9, sp), addBox(m, 6, 0, 6, 10, 4, 10, sp);
        else if (f == "north") addBox(m, 7, 7, 4, 9, 9, 16, sp), addBox(m, 6, 6, 0, 10, 10, 4, sp);
        else if (f == "south") addBox(m, 7, 7, 0, 9, 9, 12, sp), addBox(m, 6, 6, 12, 10, 10, 16, sp);
        else if (f == "west") addBox(m, 4, 7, 7, 16, 9, 9, sp), addBox(m, 0, 6, 6, 4, 10, 10, sp);
        else addBox(m, 0, 7, 7, 12, 9, 9, sp), addBox(m, 12, 6, 6, 16, 10, 10, sp);
    } else if (name == "calibrated_sculk_sensor") { // (M29.5) a sensor base, amethyst on top
        m.visible = true;
        addBox(m, 0, 0, 0, 16, 8, 16, sprite("sculk_sensor_side"));
        m.boxes[0].faces[int(Direction::Up)].sprite = sprite("calibrated_sculk_sensor_top");
        m.boxes[0].faces[int(Direction::Down)].sprite = sprite("sculk_sensor_bottom");
        const auto f = registry.value(state, "facing").value_or("north"); // the input side is its back
        const Direction back = f == "north" ? Direction::South : f == "south" ? Direction::North
                               : f == "west" ? Direction::East : Direction::West;
        m.boxes[0].faces[int(back)].sprite = sprite("calibrated_sculk_sensor_input_side");
        m.cross = true;
        m.crossSprite = sprite("calibrated_sculk_sensor_amethyst");
    } else if (name == "chiseled_bookshelf") {
        // (M29.5) the block without its front, and the front as 6 slot quads (3 across - 6, 5
        // and 5 pixels - by 2 high), each showing books or an empty shelf.
        m.visible = true;
        addBox(m, 0, 0, 0, 16, 16, 16, sprite("chiseled_bookshelf_side"));
        m.boxes[0].faces[int(Direction::Up)].sprite = m.boxes[0].faces[int(Direction::Down)].sprite =
            sprite("chiseled_bookshelf_top");
        const auto fc = registry.value(state, "facing").value_or("north");
        const Direction front = fc == "north" ? Direction::North : fc == "south" ? Direction::South
                                : fc == "west" ? Direction::West : Direction::East;
        m.boxes[0].faces[int(front)].present = false;
        static constexpr int kU[4] = {0, 6, 11, 16};
        for (int slot = 0; slot < 6; ++slot) {
            const int col = slot % 3, row = slot / 3; // row 0 on top
            const int u0 = kU[col], u1 = kU[col + 1], y0 = row == 0 ? 8 : 0, y1 = y0 + 8;
            const bool full = registry.value(state, ("slot_" + std::to_string(slot) + "_occupied").c_str()) == "true";
            const uint16_t sp = sprite(full ? "chiseled_bookshelf_occupied" : "chiseled_bookshelf_empty");
            // the viewer's left is u = 0: +x seen from the north, -z from the west...
            if (front == Direction::North) addBox(m, 16 - u1, y0, 0, 16 - u0, y1, 0, sp);
            else if (front == Direction::South) addBox(m, u0, y0, 16, u1, y1, 16, sp);
            else if (front == Direction::West) addBox(m, 0, y0, u0, 0, y1, u1, sp);
            else addBox(m, 16, y0, 16 - u1, 16, y1, 16 - u0, sp);
            BakedBox& b = m.boxes[m.boxCount - 1];
            for (int d = 0; d < 6; ++d) {
                b.faces[d].present = d == int(front);
                b.faces[d].uv[0] = uint8_t(u0), b.faces[d].uv[2] = uint8_t(u1);
                // (our empty texture draws one row of shelves, in its top half: both rows use it)
                b.faces[d].uv[1] = uint8_t(full ? 16 - y1 : 0), b.faces[d].uv[3] = uint8_t(full ? 16 - y0 : 8);
            }
        }
    } else if (name == "barrier" || name == "light" || name == "structure_void") { // (M29.7) never drawn
        m = BakedModel{};
        m.visible = false;
    } else if (name == "structure_block") { // (M29.7) its mode's face all round
        m = single(cubeAll(sprite(("structure_block_" + std::string(registry.value(state, "mode").value_or("load"))).c_str())));
    } else if (name == "test_block") {
        m = single(cubeAll(sprite(("test_block_" + std::string(registry.value(state, "mode").value_or("start"))).c_str())));
    } else if (name == "jigsaw") { // (M29.7) its arrow face toward its front, the back opposite
        const std::string o(registry.value(state, "orientation").value_or("north_up"));
        static constexpr const char* kSides[6] = {"down", "up", "north", "south", "west", "east"};
        int front = 2;
        for (int k = 0; k < 6; ++k)
            if (o.starts_with(kSides[k])) front = k;
        BakedVariant v = cubeAll(sprite("jigsaw_side"));
        v.faces[front].sprite = sprite("jigsaw_top");
        v.faces[front ^ 1].sprite = sprite("jigsaw_bottom");
        m = single(v);
    } else if (name.ends_with("command_block")) { // (M29.7) the arrow front, its back, marked sides
        const auto f = registry.value(state, "facing").value_or("north");
        static constexpr const char* kSides[6] = {"down", "up", "north", "south", "west", "east"};
        int front = 2;
        for (int k = 0; k < 6; ++k)
            if (f == kSides[k]) front = k;
        const bool cond = registry.value(state, "conditional") == "true";
        BakedVariant v = cubeAll(sprite((name + (cond ? "_conditional" : "_side")).c_str()));
        v.faces[front].sprite = sprite((name + "_front").c_str());
        v.faces[front ^ 1].sprite = sprite((name + "_back").c_str());
        m = single(v);
    } else if (name.ends_with("copper_golem_statue")) {
        // (M29.6) a copper golem in its stage's copper (vanilla: the golem's model in a pose):
        // legs, body and head; sitting it is 4 pixels lower, with arms up for "star"; its
        // nose shows which way it faces.
        const std::string stage = name.substr(0, name.size() - std::string("copper_golem_statue").size());
        const uint16_t sp = sprite(stage.empty() ? "copper_block" : (stage + "copper").c_str());
        const std::string pose(registry.value(state, "copper_golem_pose").value_or("standing"));
        const int dy = pose == "sitting" ? -4 : 0;
        m.visible = true;
        if (dy == 0) addBox(m, 5, 0, 7, 7, 4, 9, sp), addBox(m, 9, 0, 7, 11, 4, 9, sp);
        addBox(m, 4, 4 + dy, 5, 12, 10 + dy, 11, sp);  // body
        addBox(m, 3, 10 + dy, 4, 13, 16 + dy, 12, sp); // head
        if (pose == "star") addBox(m, 1, 9, 7, 3, 15, 9, sp), addBox(m, 13, 9, 7, 15, 15, 9, sp);
        const auto fc = registry.value(state, "facing").value_or("north");
        if (fc == "north") addBox(m, 7, 11 + dy, 2, 9, 14 + dy, 4, sp);
        else if (fc == "south") addBox(m, 7, 11 + dy, 12, 9, 14 + dy, 14, sp);
        else if (fc == "west") addBox(m, 1, 11 + dy, 7, 3, 14 + dy, 9, sp);
        else addBox(m, 13, 11 + dy, 7, 15, 14 + dy, 9, sp);
    } else if (name.ends_with("_shelf")) { // (M29.6) the shelf on its front, planks round it
        const std::string wood = name.substr(0, name.size() - 6);
        BakedVariant v = cubeAll(sprite((wood + "_planks").c_str()));
        const auto fc = registry.value(state, "facing").value_or("north");
        const Direction front = fc == "north" ? Direction::North : fc == "south" ? Direction::South
                                : fc == "west" ? Direction::West : Direction::East;
        v.faces[int(front)].sprite = sprite(name.c_str());
        m = single(v);
    } else if (name == "copper_torch") { // (M29.6) the torch's stick with the copper flame
        m = m_models[registry.defaultState(blocks::Torch)];
        for (int b = 0; b < m.boxCount; ++b)
            for (auto& f : m.boxes[b].faces) f.sprite = sprite("copper_torch");
    } else if (name == "bubble_column") { // (M29.5) only its water is drawn (the mesher's waterlogged cell)
        m = BakedModel{};
        m.visible = false;
    } else if (name == "crafter") {
        // (M29.5) the crafting grid on top, the output face at its front ("north" texture),
        // the back and sides; lit up while triggered. Up/down crafters show the grid toward
        // the player (ours: the front texture on the face up or down).
        const std::string o(registry.value(state, "orientation").value_or("north_up"));
        const std::string trig = registry.value(state, "triggered") == "true" ? "_triggered" : "";
        static constexpr const char* kSides[6] = {"down", "up", "north", "south", "west", "east"};
        Direction front = Direction::North;
        for (int k = 0; k < 6; ++k)
            if (o.starts_with(kSides[k])) front = static_cast<Direction>(k);
        BakedVariant v = cubeAll(sprite(("crafter_east" + trig).c_str()));
        v.faces[int(Direction::Up)].sprite = sprite(("crafter_top" + trig).c_str());
        v.faces[int(Direction::Down)].sprite = sprite("crafter_bottom");
        v.faces[int(front)].sprite = sprite(("crafter_north" + trig).c_str());
        v.faces[int(front) ^ 1].sprite = sprite(("crafter_south" + trig).c_str());
        m = single(v);
    } else if (name == "scaffolding") { // (M29.5) an open frame (vanilla: a top, legs and side bars)
        BakedVariant v = cubeAll(sprite("scaffolding_side"));
        v.faces[int(Direction::Up)].sprite = sprite("scaffolding_top");
        v.faces[int(Direction::Down)].sprite = sprite("scaffolding_bottom");
        m = single(v);
    } else if (name == "respawn_anchor") { // (M29.5) sides glow by charge
        const std::string c(registry.value(state, "charges").value_or("0"));
        BakedVariant v = cubeAll(sprite(("respawn_anchor_side" + c).c_str()));
        v.faces[int(Direction::Up)].sprite = sprite(c == "0" ? "respawn_anchor_top_off" : "respawn_anchor_top");
        v.faces[int(Direction::Down)].sprite = sprite("respawn_anchor_bottom");
        m = single(v);
    } else if (name == "target") { // (M29.5) cube_column
        m = single(cubeColumn(sprite("target_side"), sprite("target_top"), "y"));
    } else if (name == "melon") { // vanilla: cube_column
        m = single(cubeColumn(sprite("melon_side"), sprite("melon_top"), "y"));
    } else if (name.ends_with("_stem") && (name.starts_with("melon") || name.starts_with("pumpkin") ||
                                           name.starts_with("attached_"))) {
        // (M29.4b) Stems: vanilla draws a cross (2 + 2*age)/16 tall, the attached one a
        // single plane bent toward its fruit; ours are planes on the block's middle
        // axes (a "+"), tinted green to yellow by age.
        m.visible = true;
        const bool attachedStem = name.starts_with("attached_");
        const int a = attachedStem ? 7 : std::stoi(std::string(registry.value(state, "age").value_or("0")));
        m.fixedTintSlot = uint8_t(world::kStemSlot0 + a);
        const uint16_t sp = sprite(name.c_str());
        // `toward`: the face's texture runs left to right as seen, so a face whose
        // viewer's right is the fruit's side keeps it; the back face mirrors it.
        auto plane = [&](bool alongX, std::string_view toward, int height) {
            if (alongX) addBox(m, 0, 0, 8, 16, height, 8, sp);
            else addBox(m, 8, 0, 0, 8, height, 16, sp);
            BakedBox& b = m.boxes[m.boxCount - 1];
            for (int d = 0; d < 6; ++d) {
                auto& f = b.faces[d];
                const auto dir = static_cast<Direction>(d);
                const bool shown = alongX ? (dir == Direction::North || dir == Direction::South)
                                          : (dir == Direction::West || dir == Direction::East);
                f.present = shown;
                f.tint = Tint::Foliage;
                const std::string_view right = dir == Direction::South   ? "east"
                                               : dir == Direction::North ? "west"
                                               : dir == Direction::East  ? "north"
                                                                         : "south";
                if (!toward.empty() && right != toward) std::swap(f.uv[0], f.uv[2]);
            }
        };
        if (attachedStem) { // the texture's arm is on its right: toward the fruit
            const auto fc = registry.value(state, "facing").value_or("north");
            plane(fc == "east" || fc == "west", fc, 16);
        } else {
            plane(true, {}, 2 + 2 * a);
            plane(false, {}, 2 + 2 * a);
        }
    } else if (name == "cocoa") {
        // (M29.4b; wiki: Cocoa Beans) a pod hanging from the log's side, 4/6/8 wide
        // and 3/5/7 tall by age (our texture's pod), its top 4/16 under the block's.
        m.visible = true;
        const int a = std::stoi(std::string(registry.value(state, "age").value_or("0")));
        const int w = 4 + 2 * a, h = 3 + 2 * a, lo = 8 - w / 2, hi = 8 + w / 2;
        const auto fc = registry.value(state, "facing").value_or("north"); // toward the log
        const uint16_t sp = sprite(("cocoa_stage" + std::to_string(a)).c_str());
        if (fc == "north") addBox(m, lo, 12 - h, 1, hi, 12, 1 + w, sp);
        else if (fc == "south") addBox(m, lo, 12 - h, 15 - w, hi, 12, 15, sp);
        else if (fc == "west") addBox(m, 1, 12 - h, lo, 1 + w, 12, hi, sp);
        else addBox(m, 15 - w, 12 - h, lo, 15, 12, hi, sp);
        for (auto& f : m.boxes[m.boxCount - 1].faces) { // every face shows the pod's side
            f.uv[0] = uint8_t(lo), f.uv[1] = 4, f.uv[2] = uint8_t(hi), f.uv[3] = uint8_t(4 + h);
        }
    } else {
        return false;
    }
    return true;
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
            if (bakeLateModel(registry, state, name, atlas, m)) break; // (M29)
            static constexpr std::string_view kPlants[] = {
                "short_grass", "fern", "dandelion", "golden_dandelion", "poppy", "cornflower", "azure_bluet",
                "oxeye_daisy", "dead_bush", "oak_sapling", "birch_sapling", "spruce_sapling", "acacia_sapling",
                "brown_mushroom", "red_mushroom", "jungle_sapling", "dark_oak_sapling", "cherry_sapling",
                "crimson_fungus", "warped_fungus", "crimson_roots", "warped_roots", "nether_sprouts",
                "weeping_vines", "weeping_vines_plant", "twisting_vines", "twisting_vines_plant",
                "mangrove_propagule", "pale_oak_sapling", "poplar_sapling",
                // (M29.4a) the rest of the flowers
                "allium", "blue_orchid", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip",
                "lily_of_the_valley", "wither_rose"};
            if (name == "wheat" || name == "carrots" || name == "potatoes" || name == "beetroots" ||
                name == "sweet_berry_bush") {
                // Crops by age (vanilla: carrots/potatoes 8 ages on 4 textures - 0-1,
                // 2-3, 4-6, 7). Drawn as a cross (vanilla's crop model is a # of 4 planes).
                const int a = std::stoi(std::string(registry.value(state, "age").value_or("0")));
                const int stage = name == "wheat" || name == "beetroots" || name == "sweet_berry_bush" ? a
                                  : a < 2                                                         ? 0
                                  : a < 4                                                         ? 1
                                  : a < 7                                                         ? 2
                                                                                                  : 3;
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
            } else if (name == "chorus_plant" || name == "iron_bars" || name.ends_with("copper_bars")) {
                // A middle piece plus an arm to each connected side (vanilla multipart):
                // the chorus plant a 8x8 core (vanilla 10x10 with fringes), iron bars a
                // 2-wide post with 2-wide arms (vanilla: flat panes).
                const bool bars = name != "chorus_plant"; // (M29.6: copper bars too)
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
            } else if (name.starts_with("infested_")) { // (M26.4a) the stone it hides in
                const std::string base = name.substr(9);
                m = base == "deepslate" ? single(cubeColumn(sprite("deepslate"), sprite("deepslate_top"),
                                                            registry.value(state, "axis").value_or("y")))
                                        : single(cubeAll(sprite(base.c_str())));
            } else if (name.ends_with("_skull") || name.ends_with("_head")) {
                // Mob heads (M26.4b): an 8x8x8 head of the mob's own skin (textures cut from
                // it by tools/textures/gen_heads.py); standing ones turned to the nearest
                // quarter of their 16 directions, wall ones against the wall behind them.
                const bool wall = name.find("_wall_") != std::string::npos;
                std::string kind = name.substr(0, name.rfind('_'));
                if (wall) kind = kind.substr(0, kind.rfind('_')); // ("zombie_wall_head" -> "zombie")
                auto tex = [&](const char* face) { return sprite(("clone_head_" + kind + "_" + face).c_str()); };
                int quarter = 0; // 0: facing south
                if (wall) {
                    const int f = registry.get(state, world::properties::facing); // north, south, west, east
                    quarter = f == 0 ? 2 : f == 1 ? 0 : f == 2 ? 1 : 3;
                } else {
                    quarter = ((registry.get(state, world::properties::rotation16) + 2) / 4) & 3;
                }
                // Faces of a head facing south, then turned: south -> west -> north -> east.
                static constexpr Direction kRing[4] = {Direction::South, Direction::West, Direction::North, Direction::East};
                auto turned = [&](Direction d) {
                    for (int i = 0; i < 4; ++i)
                        if (kRing[i] == d) return kRing[(i + quarter) & 3];
                    return d;
                };
                BakedVariant v = cubeAll(tex("side"));
                v.faces[int(turned(Direction::South))].sprite = tex("front");
                v.faces[int(turned(Direction::North))].sprite = tex("back");
                v.faces[int(turned(Direction::West))].sprite = tex("side");
                v.faces[int(turned(Direction::East))].sprite = tex("side");
                v.faces[int(Direction::Up)].sprite = tex("top");
                v.faces[int(Direction::Down)].sprite = tex("top");
                int a[3] = {4, wall ? 4 : 0, wall ? 0 : 4}, c[3] = {12, wall ? 12 : 8, wall ? 8 : 12};
                for (int q = 0; q < quarter; ++q) { // (x, z) -> (16 - z, x)
                    const int ax = a[0], az = a[2], cx = c[0], cz = c[2];
                    a[0] = 16 - az, a[2] = ax;
                    c[0] = 16 - cz, c[2] = cx;
                }
                m.visible = true;
                addBoxFrom(m, std::min(a[0], c[0]), a[1], std::min(a[2], c[2]), std::max(a[0], c[0]), c[1],
                           std::max(a[2], c[2]), v);
                for (auto& f : m.boxes[m.boxCount - 1].faces) { // (each face shows its whole texture)
                    f.uv[0] = f.uv[1] = 0;
                    f.uv[2] = f.uv[3] = 16;
                }
            } else if (name == "dried_ghast") { // (M26.5b) a shrivelled ghast, its face toward `facing`
                const auto f = registry.value(state, "facing").value_or("north");
                BakedVariant v = cubeAll(sprite("clone_dried_ghast_side"));
                v.faces[int(Direction::Up)].sprite = sprite("clone_dried_ghast_top");
                v.faces[int(Direction::Down)].sprite = sprite("clone_dried_ghast_top");
                const Direction front = f == "south" ? Direction::South
                                        : f == "west" ? Direction::West
                                        : f == "east" ? Direction::East
                                                      : Direction::North;
                v.faces[int(front)].sprite = sprite("clone_dried_ghast_front");
                m.visible = true;
                addBoxFrom(m, 3, 0, 3, 13, 10, 13, v);
                for (auto& bf : m.boxes[m.boxCount - 1].faces) {
                    bf.uv[0] = bf.uv[1] = 0;
                    bf.uv[2] = bf.uv[3] = 16;
                }
            } else if (name == "cobweb") {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite("cobweb");
            } else if (name == "ochre_froglight" || name == "verdant_froglight" || name == "pearlescent_froglight") {
                m = single(cubeColumn(sprite((name + "_side").c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name == "frogspawn") { // (M26.3c) a film of eggs on the water: one flat layer
                m.visible = true;
                m.boxCount = 1;
                BakedBox& b = m.boxes[0];
                b.from[0] = b.from[1] = b.from[2] = 0;
                b.to[0] = b.to[2] = 16;
                b.to[1] = 1;
                for (int f = 0; f < 6; ++f) {
                    b.faces[f].sprite = sprite("frogspawn");
                    b.faces[f].uv[0] = b.faces[f].uv[1] = 0;
                    b.faces[f].uv[2] = b.faces[f].uv[3] = 16;
                    b.faces[f].present = f == int(Direction::Up) || f == int(Direction::Down);
                }
            } else if (name == "basalt" || name == "bone_block") {
                m = single(cubeColumn(sprite((name + "_side").c_str()), sprite((name + "_top").c_str()),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name == "heavy_core") { // (M28.4d) a small dark core in the middle of the floor
                m.visible = true;
                addBox(m, 4, 0, 4, 12, 8, 12, sprite("heavy_core"));
            } else if (name == "lodestone") { // (M28.2a)
                BakedVariant v = cubeAll(sprite("lodestone_side"));
                v.faces[int(Direction::Up)].sprite = sprite("lodestone_top");
                v.faces[int(Direction::Down)].sprite = sprite("lodestone_top");
                m = single(v);
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
            } else if (name == "carved_pumpkin" || name == "jack_o_lantern") { // the face toward `facing`
                const auto facing = registry.value(state, "facing").value_or("north");
                BakedVariant v = cubeColumn(sprite("pumpkin_side"), sprite("pumpkin_top"), "y");
                const Direction front = facing == "south" ? Direction::South
                                        : facing == "west" ? Direction::West
                                        : facing == "east" ? Direction::East
                                                           : Direction::North;
                v.faces[int(front)].sprite = sprite(name.c_str()); // (M29.4a: or the lit face)
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
            } else if (name.ends_with("_bed")) { // (M29.4b: every colour)
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
                    face.sprite = d == int(Direction::Up) ? sprite((name + (head ? "_head_top" : "_foot_top")).c_str())
                                  : d == int(Direction::Down) ? sprite("oak_planks")
                                                              : sprite((name + "_side").c_str());
                    face.uv[0] = 0, face.uv[1] = 0, face.uv[2] = 16, face.uv[3] = 16;
                }
                // The top turns with the bed so the pillow lies at the head's far end.
                const auto f = registry.value(state, "facing").value_or("north");
                b.faces[int(Direction::Up)].rotation = f == "east" ? 1 : f == "south" ? 2 : f == "west" ? 3 : 0;
            } else if (name == "chest" || name == "trapped_chest" || name == "ender_chest" || name.ends_with("copper_chest")) {
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
                uint16_t sideS = 0, topS = 0, frontS = 0;
                if (name.ends_with("copper_chest")) { // (M26.5b) the copper block of its stage, all round
                    std::string stage = name.starts_with("waxed_") ? name.substr(6) : name;
                    stage = stage.substr(0, stage.size() - std::string("copper_chest").size());
                    sideS = topS = frontS = sprite(stage.empty() ? "copper_block" : (stage + "copper").c_str());
                } else {
                    sideS = sprite((tex + "_side").c_str()), topS = sprite((tex + "_top").c_str()),
                    frontS = sprite((tex + "_front").c_str());
                }
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
            } else if (name == "sunflower" || name == "lilac" || name == "rose_bush" || name == "peony" ||
                       name == "tall_grass" || name == "large_fern") {
                // Two-block plants (M27.1): a cross per half; the sunflower's upper half also
                // shows its flower head, a plane facing east (vanilla: tilted, also east).
                const bool upper = registry.value(state, "half") == "upper";
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite((name + (upper ? "_top" : "_bottom")).c_str());
                m.crossTint = name == "tall_grass" || name == "large_fern" ? Tint::Grass : Tint::None;
                if (name == "sunflower" && upper) {
                    addBox(m, 9, 0, 0, 9, 16, 16, sprite("sunflower_front"));
                    m.boxes[0].faces[int(Direction::West)].sprite = sprite("sunflower_back");
                }
            } else if (name == "creaking_heart") { // (M27.1c) its glowing faces when awake
                const bool awake = registry.value(state, "creaking_heart_state") == "awake";
                m = single(cubeColumn(sprite(awake ? "creaking_heart_active" : "creaking_heart"),
                                      sprite(awake ? "creaking_heart_top_active" : "creaking_heart_top"),
                                      registry.value(state, "axis").value_or("y")));
            } else if (name == "open_eyeblossom" || name == "closed_eyeblossom") {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(name.c_str());
            } else if (name == "resin_clump") { // a 1-pixel layer on the face it sits on
                const std::string_view f = registry.value(state, "facing").value_or("down");
                const uint16_t sp = sprite("resin_clump");
                m.visible = true;
                if (f == "down") addBox(m, 0, 0, 0, 16, 1, 16, sp);
                else if (f == "up") addBox(m, 0, 15, 0, 16, 16, 16, sp);
                else if (f == "north") addBox(m, 0, 0, 0, 16, 16, 1, sp);
                else if (f == "south") addBox(m, 0, 0, 15, 16, 16, 16, sp);
                else if (f == "west") addBox(m, 0, 0, 0, 1, 16, 16, sp);
                else addBox(m, 15, 0, 0, 16, 16, 16, sp);
            } else if (name.ends_with("amethyst_bud") || name == "amethyst_cluster") { // (M27.4a)
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(name.c_str());
            } else if (name == "torchflower_crop" || name == "torchflower") { // (M27.5c)
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(name == "torchflower" ? "torchflower"
                                       : registry.value(state, "age") == "1" ? "torchflower_crop_stage1"
                                                                            : "torchflower_crop_stage0");
            } else if (name == "pitcher_crop") {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(("pitcher_crop_bottom_stage_" + std::string(registry.value(state, "age").value_or("0"))).c_str());
            } else if (name == "pitcher_plant") {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(registry.value(state, "half") == "upper" ? "pitcher_plant_top" : "pitcher_plant_bottom");
            } else if (name == "sniffer_egg") { // a big egg, cracking as it nears hatching
                const std::string_view h = registry.value(state, "hatch").value_or("0");
                const std::string k = h == "0" ? "not_cracked" : h == "1" ? "slightly_cracked" : "very_cracked";
                m.visible = true;
                addBox(m, 1, 0, 2, 15, 16, 14, sprite(("sniffer_egg_" + k + "_north").c_str()));
                m.boxes[0].faces[int(Direction::Up)].sprite = sprite(("sniffer_egg_" + k + "_top").c_str());
                m.boxes[0].faces[int(Direction::Down)].sprite = sprite(("sniffer_egg_" + k + "_bottom").c_str());
                m.boxes[0].faces[int(Direction::East)].sprite = sprite(("sniffer_egg_" + k + "_east").c_str());
                m.boxes[0].faces[int(Direction::West)].sprite = sprite(("sniffer_egg_" + k + "_west").c_str());
            } else if (name == "suspicious_sand" || name == "suspicious_gravel") { // (M27.5) by how dusted
                m = single(cubeAll(sprite((name + "_" + std::string(registry.value(state, "dusted").value_or("0"))).c_str())));
            } else if (name == "decorated_pot") { // (M27.5) a terracotta pot with a neck (no sherd faces)
                m.visible = true;
                addBox(m, 1, 0, 1, 15, 13, 15, sprite("terracotta"));
                addBox(m, 4, 13, 4, 12, 16, 12, sprite("terracotta"));
            } else if (name == "trial_spawner") { // (M27.4d) a cage by its state
                const std::string_view st = registry.value(state, "trial_spawner_state").value_or("inactive");
                const bool active = st == "active" || st == "waiting_for_reward_ejection";
                const bool ejecting = st == "ejecting_reward";
                const std::string om = registry.value(state, "ominous") == "true" ? "_ominous" : ""; // (M28.4d)
                BakedVariant v = cubeAll(sprite((std::string(ejecting ? "trial_spawner_side_ejecting_reward"
                                                             : active ? "trial_spawner_side_active"
                                                                      : "trial_spawner_side_inactive") +
                                                 (ejecting ? "" : om))
                                                    .c_str()));
                v.faces[int(Direction::Up)].sprite = sprite((std::string(ejecting ? "trial_spawner_top_ejecting_reward"
                                                                         : active ? "trial_spawner_top_active"
                                                                                  : "trial_spawner_top_inactive") +
                                                             om)
                                                                .c_str());
                v.faces[int(Direction::Down)].sprite = sprite("trial_spawner_bottom");
                m = single(v);
            } else if (name == "vault") { // (M27.4d) its keyhole on the front, lit while it can open
                const bool on = registry.value(state, "vault_state") != "inactive";
                const std::string om = registry.value(state, "ominous") == "true" ? "_ominous" : ""; // (M28.4d)
                const std::string_view f = registry.value(state, "facing").value_or("north");
                const Direction front = f == "south" ? Direction::South : f == "west" ? Direction::West
                                        : f == "east" ? Direction::East : Direction::North;
                BakedVariant v = cubeAll(sprite((std::string(on ? "vault_side_on" : "vault_side_off") + om).c_str()));
                v.faces[int(front)].sprite = sprite((std::string(on ? "vault_front_on" : "vault_front_off") + om).c_str());
                v.faces[int(Direction::Up)].sprite = sprite(("vault_top" + om).c_str());
                v.faces[int(Direction::Down)].sprite = sprite(("vault_bottom" + om).c_str());
                m = single(v);
            } else if (name == "sculk_vein" || name == "glow_lichen") { // (M27.3; M29.4c) a layer on its face
                const std::string_view f = registry.value(state, "facing").value_or("down");
                const uint16_t sp = sprite(name.c_str());
                m.visible = true;
                if (f == "down") addBox(m, 0, 0, 0, 16, 1, 16, sp);
                else if (f == "up") addBox(m, 0, 15, 0, 16, 16, 16, sp);
                else if (f == "north") addBox(m, 0, 0, 0, 16, 16, 1, sp);
                else if (f == "south") addBox(m, 0, 0, 15, 16, 16, 16, sp);
                else if (f == "west") addBox(m, 0, 0, 0, 1, 16, 16, sp);
                else addBox(m, 15, 0, 0, 16, 16, 16, sp);
            } else if (name == "sculk_catalyst") {
                const bool b = registry.value(state, "bloom") == "true";
                BakedVariant v = cubeAll(sprite(b ? "sculk_catalyst_side_bloom" : "sculk_catalyst_side"));
                v.faces[int(Direction::Up)].sprite = sprite(b ? "sculk_catalyst_top_bloom" : "sculk_catalyst_top");
                v.faces[int(Direction::Down)].sprite = sprite("sculk_catalyst_bottom");
                m = single(v);
            } else if (name == "reinforced_deepslate") {
                BakedVariant v = cubeAll(sprite("reinforced_deepslate_side"));
                v.faces[int(Direction::Up)].sprite = sprite("reinforced_deepslate_top");
                v.faces[int(Direction::Down)].sprite = sprite("reinforced_deepslate_bottom");
                m = single(v);
            } else if (name == "sculk_sensor" || name == "sculk_shrieker") { // a half-block base
                const bool sensor = name == "sculk_sensor";
                m.visible = true;
                addBox(m, 0, 0, 0, 16, 8, 16, sprite((name + "_side").c_str()));
                m.boxes[0].faces[int(Direction::Up)].sprite = sprite((name + "_top").c_str());
                m.boxes[0].faces[int(Direction::Down)].sprite = sprite((name + "_bottom").c_str());
                if (sensor) { // tendrils over it, lit while active
                    m.cross = true;
                    m.crossSprite = sprite(registry.value(state, "sculk_sensor_phase") == "active"
                                               ? "sculk_sensor_tendril_active"
                                               : "sculk_sensor_tendril_inactive");
                }
            } else if (name == "pointed_dripstone" || name == "sulfur_spike") { // (M27.2b; M33.2) a cross by direction and thickness
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite((name + "_" + std::string(registry.value(state, "vertical_direction").value_or("up")) +
                                        "_" + std::string(registry.value(state, "thickness").value_or("tip")))
                                           .c_str());
            } else if (name == "cave_vines" || name == "cave_vines_plant") { // (M27.2) lit with berries
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite((name + (registry.value(state, "berries") == "true" ? "_lit" : "")).c_str());
            } else if (name == "spore_blossom") { // a flower hanging under its pad
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite("spore_blossom");
                addBox(m, 1, 15, 1, 15, 16, 15, sprite("spore_blossom_base"));
            } else if (name == "hanging_roots") {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite("hanging_roots");
            } else if (name == "azalea" || name == "flowering_azalea") { // a leafy top on a twiggy stem
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite("azalea_plant");
                addBox(m, 0, 8, 0, 16, 16, 16, sprite((name + "_side").c_str()));
                m.boxes[0].faces[int(Direction::Up)].sprite = sprite((name + "_top").c_str());
                m.boxes[0].faces[int(Direction::Down)].sprite = sprite((name + "_top").c_str());
            } else if (name == "small_dripleaf") {
                const bool upper = registry.value(state, "half") == "upper";
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(upper ? "small_dripleaf_stem_top" : "small_dripleaf_stem_bottom");
                if (upper) addBox(m, 2, 13, 2, 14, 14, 14, sprite("small_dripleaf_top"));
            } else if (name == "big_dripleaf" || name == "big_dripleaf_stem") {
                // The leaf a plate at the top, lower as it tips (vanilla turns it).
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite("big_dripleaf_stem");
                if (name == "big_dripleaf") {
                    const std::string_view t = registry.value(state, "tilt").value_or("none");
                    const int y = t == "full" ? 11 : t == "partial" ? 13 : 15;
                    addBox(m, 0, y - 1, 0, 16, y, 16, sprite("big_dripleaf_side"));
                    m.boxes[0].faces[int(Direction::Up)].sprite = sprite("big_dripleaf_top");
                    m.boxes[0].faces[int(Direction::Down)].sprite = sprite("big_dripleaf_top");
                }
            } else if (name == "pale_hanging_moss") {
                m.visible = true;
                m.cross = true;
                m.crossSprite = sprite(registry.value(state, "tip") == "true" ? "pale_hanging_moss_tip" : "pale_hanging_moss");
            } else if (name == "muddy_mangrove_roots") {
                m = single(cubeColumn(sprite("muddy_mangrove_roots_side"), sprite("muddy_mangrove_roots_top"),
                                      registry.value(state, "axis").value_or("y")));
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
            } else if (name == "turtle_egg") {
                // 1-4 eggs (4x5x4, a little apart), cracking as they near hatching.
                static constexpr int kAt[4][4][2] = {{{6, 6}}, {{3, 4}, {9, 9}}, {{3, 3}, {9, 4}, {6, 10}},
                                                     {{3, 3}, {10, 3}, {3, 10}, {10, 10}}};
                const int n = std::stoi(std::string(registry.value(state, "eggs").value_or("1")));
                const std::string_view h = registry.value(state, "hatch").value_or("0");
                const uint16_t egg = sprite(h == "0" ? "turtle_egg" : h == "1" ? "turtle_egg_slightly_cracked" : "turtle_egg_very_cracked");
                m.visible = true;
                for (int k = 0; k < n; ++k)
                    addBox(m, kAt[n - 1][k][0], 0, kAt[n - 1][k][1], kAt[n - 1][k][0] + 4, 5, kAt[n - 1][k][1] + 4, egg);
            } else if (name == "dried_kelp_block") {
                BakedVariant v = cubeAll(sprite("dried_kelp_side"));
                v.faces[int(Direction::Up)].sprite = sprite("dried_kelp_top");
                v.faces[int(Direction::Down)].sprite = sprite("dried_kelp_bottom");
                m = single(v);
            } else if (name == "cake" || registry.likeOf(registry.blockOf(state)) == world::blocks::CandleCake) {
                // (M28.5a) the cake (bites eaten from its west side) and a candle cake's candle
                m.visible = true;
                const int bitten = name == "cake" ? std::stoi(std::string(registry.value(state, "bites").value_or("0"))) : 0;
                addBox(m, 1 + 2 * bitten, 0, 1, 15, 8, 15, sprite("cake_side"));
                BakedBox& c = m.boxes[m.boxCount - 1];
                c.faces[int(Direction::Up)].sprite = sprite("cake_top");
                c.faces[int(Direction::Down)].sprite = sprite("cake_bottom");
                if (bitten > 0) c.faces[int(Direction::West)].sprite = sprite("cake_inner");
                if (name != "cake") {
                    const std::string candle = name.substr(0, name.rfind("_cake")) +
                                               (registry.value(state, "lit") == "true" ? "_lit" : "");
                    addBox(m, 7, 8, 7, 9, 14, 9, sprite(candle.c_str()));
                }
            } else if (registry.likeOf(registry.blockOf(state)) == world::blocks::Candle) {
                // (M28.5a) 1-4 candles, 2x6x2 (vanilla's places, a little apart)
                static constexpr int kAt[4][4][2] = {{{7, 7}}, {{5, 7}, {9, 8}}, {{5, 8}, {8, 6}, {9, 9}},
                                                     {{5, 5}, {9, 5}, {5, 9}, {9, 8}}};
                const int n = std::stoi(std::string(registry.value(state, "candles").value_or("1")));
                const std::string tex = name + (registry.value(state, "lit") == "true" ? "_lit" : "");
                m.visible = true;
                for (int k = 0; k < n; ++k)
                    addBox(m, kAt[n - 1][k][0], 0, kAt[n - 1][k][1], kAt[n - 1][k][0] + 2, 6 - (k % 2),
                           kAt[n - 1][k][1] + 2, sprite(tex.c_str()));
            } else if (name == "pink_petals" || name == "wildflowers" || name == "leaf_litter") {
                // (M28.5a) 1-4 quarters of the floor, turned by the facing
                const int n = std::stoi(std::string(registry.value(state, name == "leaf_litter" ? "segment_amount" : "flower_amount")
                                                        .value_or("1")));
                const std::string_view f = registry.value(state, "facing").value_or("north");
                const int turn = f == "east" ? 1 : f == "south" ? 2 : f == "west" ? 3 : 0;
                static constexpr int kQuarter[4][2] = {{0, 0}, {8, 0}, {8, 8}, {0, 8}}; // NW, NE, SE, SW
                m.visible = true;
                for (int k = 0; k < n; ++k) {
                    const int q = (k + turn) % 4;
                    addBox(m, kQuarter[q][0], 0, kQuarter[q][1], kQuarter[q][0] + 8, 1, kQuarter[q][1] + 8,
                           sprite(name.c_str()));
                    BakedBox& b = m.boxes[m.boxCount - 1];
                    for (int d = 0; d < 6; ++d) b.faces[d].present = d == int(Direction::Up);
                }
            } else if (name == "firefly_bush" || name == "bush" || name == "short_dry_grass" || name == "tall_dry_grass" ||
                       name == "cactus_flower") {
                m.visible = true; // (M28.5a) crossed plants
                m.cross = true;
                m.crossSprite = sprite(name.c_str());
                m.crossTint = name == "bush" ? Tint::Grass : Tint::None;
            } else if (name == "vine") { // (M28.5a) a thin panel on each face it clings to, foliage green
                m.visible = true;
                auto side = [&](const char* prop) { return registry.value(state, prop) == "true"; };
                const auto add = [&](int x0, int y0, int z0, int x1, int y1, int z1) {
                    addBox(m, x0, y0, z0, x1, y1, z1, sprite("vine"));
                    for (auto& f : m.boxes[m.boxCount - 1].faces) f.tint = Tint::Foliage;
                };
                if (side("north")) add(0, 0, 0, 16, 16, 1);
                if (side("south")) add(0, 0, 15, 16, 16, 16);
                if (side("west")) add(0, 0, 0, 1, 16, 16);
                if (side("east")) add(15, 0, 0, 16, 16, 16);
                if (side("up")) add(0, 15, 0, 16, 16, 16);
                if (m.boxCount == 0) m.visible = false;
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
                    f.tint = name == "cherry_leaves" || name == "pale_oak_leaves" || name.ends_with("poplar_leaves") ||
                             name == "azalea_leaves" ||
                                     name == "flowering_azalea_leaves"
                                 ? Tint::None
                                 : Tint::Foliage;
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
            } else if (name == "bee_nest" || name == "beehive") {
                // (M26.3b) the front faces `facing`; full of honey it shows dripping combs
                const bool nest = name == "bee_nest";
                const bool full = registry.value(state, "honey_level") == "5";
                const auto facing = registry.value(state, "facing").value_or("north");
                BakedVariant v = cubeAll(sprite(nest ? "bee_nest_side" : "beehive_side"));
                v.faces[int(Direction::Up)].sprite = sprite(nest ? "bee_nest_top" : "beehive_end");
                v.faces[int(Direction::Down)].sprite = sprite(nest ? "bee_nest_bottom" : "beehive_end");
                const Direction front = facing == "south" ? Direction::South
                                        : facing == "west" ? Direction::West
                                        : facing == "east" ? Direction::East
                                                           : Direction::North;
                v.faces[int(front)].sprite = sprite(nest ? (full ? "bee_nest_front_honey" : "bee_nest_front")
                                                         : (full ? "beehive_front_honey" : "beehive_front"));
                m = single(v);
            } else if (name == "honey_block") {
                BakedVariant v = cubeAll(sprite("honey_block_side"));
                v.faces[int(Direction::Up)].sprite = sprite("honey_block_top");
                v.faces[int(Direction::Down)].sprite = sprite("honey_block_bottom");
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
            } else if (name == "frosted_ice") { // (M29.2b) its crack stage by age
                m = single(cubeAll(sprite(("frosted_ice_" + std::string(registry.value(state, "age").value_or("0"))).c_str())));
                m.translucent = true;
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
