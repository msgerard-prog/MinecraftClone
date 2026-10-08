// Component models: redstone (M11), portals and frames (M12). Shapes follow the in-game look described on the
// wiki pages of each block; boxes are our own layout in 1/16 block. Each model is
// built in one orientation and turned to its state's facing.
#include "rendering/BlockModels.h"
#include "rendering/TextureAtlas.h"
#include "world/Biome.h"
#include "world/Blocks.h"

#include <glm/glm.hpp>

namespace mc::gfx {

namespace {

using world::Direction;
namespace B = world::blocks;
namespace P = world::properties;

// Quarter turns: first `x` about the X axis (north -> up), then `y` about the Y axis
// (north -> east).
struct Rot {
    int x = 0, y = 0;
};

glm::ivec3 turn(glm::ivec3 v, Rot r) {
    for (int i = 0; i < (r.x & 3); ++i)
        v = {v.x, -v.z, v.y};
    for (int i = 0; i < (r.y & 3); ++i)
        v = {-v.z, v.y, v.x};
    return v;
}

int dirIndex(glm::ivec3 n) {
    for (int i = 0; i < world::kDirectionCount; ++i)
        if (world::kDirectionNormals[i] == n) return i;
    return 0;
}

// The texture's "up" on each face when unrotated (see ChunkMesher kCorners).
glm::ivec3 defaultUp(int face) {
    if (face == int(Direction::Down)) return {0, 0, 1};
    if (face == int(Direction::Up)) return {0, 0, -1};
    return {0, 1, 0};
}

// Quarter turns clockwise (seen from outside) that bring the face's default texture
// up to `up`.
uint8_t rotationFor(int face, glm::ivec3 up) {
    const glm::ivec3 n = world::kDirectionNormals[face];
    glm::ivec3 u = defaultUp(face);
    for (uint8_t q = 0; q < 4; ++q) {
        if (u == up) return q;
        u = {u.y * n.z - u.z * n.y, u.z * n.x - u.x * n.z, u.x * n.y - u.y * n.x}; // u x n: one clockwise step
    }
    return 0;
}

struct FaceSpec {
    const char* sprite = nullptr; // nullptr: no face
    uint8_t uv[4] = {0, 0, 16, 16};
    glm::ivec3 up{0}; // texture up in model space; 0 = the face's default
    Tint tint = Tint::None;
};

struct BoxSpec {
    glm::ivec3 from, to;
    FaceSpec faces[world::kDirectionCount];
};

class Builder {
public:
    Builder(const TextureAtlas& atlas, BakedModel& m) : m_atlas(atlas), m_model(m) {
        m_model = {};
        m_model.visible = true;
    }

    void box(const BoxSpec& spec, Rot rot = {}) {
        if (m_model.boxCount >= BakedModel::kMaxBoxes) return;
        BakedBox& b = m_model.boxes[m_model.boxCount++];
        const glm::ivec3 a = turn(spec.from * 2 - 16, rot), c = turn(spec.to * 2 - 16, rot); // doubled: exact centre
        const glm::ivec3 lo = (glm::min(a, c) + 16) / 2, hi = (glm::max(a, c) + 16) / 2;
        for (int i = 0; i < 3; ++i) {
            b.from[i] = static_cast<uint8_t>(lo[i]);
            b.to[i] = static_cast<uint8_t>(hi[i]);
        }
        for (auto& f : b.faces)
            f.present = false;
        for (int i = 0; i < world::kDirectionCount; ++i) {
            const FaceSpec& fs = spec.faces[i];
            if (!fs.sprite) continue;
            const int to = dirIndex(turn(world::kDirectionNormals[i], rot));
            BakedBox::Face& f = b.faces[to];
            f.present = true;
            f.sprite = sprite(fs.sprite);
            for (int k = 0; k < 4; ++k)
                f.uv[k] = fs.uv[k];
            f.rotation = rotationFor(to, turn(fs.up == glm::ivec3(0) ? defaultUp(i) : fs.up, rot));
            f.tint = fs.tint;
        }
    }

    void cube(const FaceSpec (&faces)[world::kDirectionCount], Rot rot = {}) {
        BakedVariant v;
        for (int i = 0; i < world::kDirectionCount; ++i) {
            const int to = dirIndex(turn(world::kDirectionNormals[i], rot));
            v.faces[to].sprite = sprite(faces[i].sprite);
            v.faces[to].rotation = rotationFor(to, turn(faces[i].up == glm::ivec3(0) ? defaultUp(i) : faces[i].up, rot));
            v.faces[to].tint = faces[i].tint;
        }
        m_model.variants[0] = v;
    }

    uint16_t sprite(const char* name) const { return static_cast<uint16_t>(m_atlas.spriteIndex(name)); }

private:
    const TextureAtlas& m_atlas;
    BakedModel& m_model;
};

// The same face on all six sides.
BoxSpec allFaces(glm::ivec3 from, glm::ivec3 to, const char* sprite, const uint8_t (&uv)[4] = {0, 0, 16, 16}) {
    BoxSpec b{from, to, {}};
    for (auto& f : b.faces) {
        f.sprite = sprite;
        for (int k = 0; k < 4; ++k)
            f.uv[k] = uv[k];
    }
    return b;
}

// A torch stick (2x2): sides show the stick column, the top shows the flame.
BoxSpec torchStick(glm::ivec3 from, int height, const char* sprite) {
    BoxSpec b = allFaces(from, from + glm::ivec3(2, height, 2), sprite,
                         {7, 6, 9, static_cast<uint8_t>(6 + height)});
    b.faces[int(Direction::Up)].uv[3] = 8;
    b.faces[int(Direction::Down)] = {sprite, {7, 14, 9, 16}};
    return b;
}

int yTurns(Direction d) { // north 0, east 1, south 2, west 3
    switch (d) {
    case Direction::East: return 1;
    case Direction::South: return 2;
    case Direction::West: return 3;
    default: return 0;
    }
}

Direction hFacing(const world::BlockRegistry& r, world::BlockStateId s) {
    return static_cast<Direction>(r.get(s, P::facing) + 2);
}

// Levers and buttons are built lying on the floor facing north.
Rot attachRot(const world::BlockRegistry& r, world::BlockStateId s) {
    const Direction f = hFacing(r, s);
    switch (r.get(s, P::face)) {
    case 0: return {0, yTurns(f)};                       // floor
    case 2: return {2, yTurns(f) + 2};                   // ceiling
    default: return {1, yTurns(static_cast<Direction>(int(f) ^ 1))}; // wall behind it
    }
}

} // namespace

bool bakeRedstoneModel(const world::BlockRegistry& r, world::BlockStateId s, const TextureAtlas& atlas,
                       BakedModel& out) {
    const world::BlockId block = r.likeOf(r.blockOf(s)); // (wood sets: the oak geometry)
    const std::string_view realName = std::string_view(r.block(r.blockOf(s)).id).substr(10);
    switch (block) {
    case B::RedstoneWire: {
        // Flat pieces 1/16 above the ground (vanilla 1/64: our vertices are in 1/16),
        // tinted by power: a centre, an arm per connected side, a climb per "up" side.
        Builder b(atlas, out);
        out.fixedTintSlot = static_cast<uint8_t>(world::kRedstoneSlot0 + r.get(s, P::power));
        const world::Property* sides[4] = {&P::north, &P::east, &P::south, &P::west};
        int side[4];
        for (int i = 0; i < 4; ++i)
            side[i] = r.get(s, *sides[i]); // 0 up, 1 side, 2 none
        const bool n = side[0] != 2, e = side[1] != 2, so = side[2] != 2, w = side[3] != 2;
        const char* centre = (n || so) && !e && !w ? "redstone_dust_line0"
                             : (e || w) && !n && !so ? "redstone_dust_line1"
                                                    : "redstone_dust_dot";
        auto flat = [&](glm::ivec3 from, glm::ivec3 to, const char* sprite) {
            BoxSpec spec{from, to, {}};
            spec.faces[int(Direction::Up)] = {sprite,
                                              {uint8_t(from.x), uint8_t(from.z), uint8_t(to.x), uint8_t(to.z)},
                                              glm::ivec3(0),
                                              Tint::Foliage};
            b.box(spec);
        };
        flat({5, 0, 5}, {11, 1, 11}, centre);
        if (n) flat({5, 0, 0}, {11, 1, 5}, "redstone_dust_line0");
        if (so) flat({5, 0, 11}, {11, 1, 16}, "redstone_dust_line0");
        if (w) flat({0, 0, 5}, {5, 1, 11}, "redstone_dust_line1");
        if (e) flat({11, 0, 5}, {16, 1, 11}, "redstone_dust_line1");
        for (int i = 0; i < 4; ++i) {
            if (side[i] != 0) continue;
            BoxSpec climb{{5, 0, 0}, {11, 16, 1}, {}}; // up the north block's face
            climb.faces[int(Direction::South)] = {"redstone_dust_line0", {5, 0, 11, 16}, glm::ivec3(0), Tint::Foliage};
            b.box(climb, {0, i});
        }
        return true;
    }
    // Small blocks of M23.2 (they share the torch stick and rotation helpers).
    case B::SoulTorch: {
        Builder b(atlas, out);
        b.box(torchStick({7, 0, 7}, 10, "soul_torch"));
        return true;
    }
    case B::WallTorch:
    case B::SoulWallTorch: { // upright against the wall (vanilla tilts it: known deviation)
        Builder b(atlas, out);
        b.box(torchStick({7, 3, 12}, 10, block == B::WallTorch ? "torch" : "soul_torch"), {0, yTurns(hFacing(r, s))});
        return true;
    }
    case B::Lantern:
    case B::SoulLantern: {
        // A 6x7x6 body and a small cap (wiki: Lantern), one pixel lower when standing.
        Builder b(atlas, out);
        const char* tex = block == B::Lantern ? "lantern" : "soul_lantern";
        const int y0 = r.get(s, P::hanging) == 0 ? 1 : 0;
        // (our lantern art: the glowing body at texels 5..11 x 4..11, the cap above it)
        BoxSpec body = allFaces({5, y0, 5}, {11, y0 + 7, 11}, tex, {5, 4, 11, 11});
        body.faces[int(Direction::Up)] = {tex, {5, 4, 11, 10}};
        body.faces[int(Direction::Down)] = {tex, {5, 4, 11, 10}};
        b.box(body);
        BoxSpec cap = allFaces({6, y0 + 7, 6}, {10, y0 + 9, 10}, tex, {6, 2, 10, 4});
        cap.faces[int(Direction::Up)] = {tex, {6, 2, 10, 4}};
        b.box(cap);
        return true;
    }
    case B::Chain: { // a thin bar along the axis showing the chain links
        Builder b(atlas, out);
        const int a = r.get(s, P::axis);
        BoxSpec bar = allFaces({7, 0, 7}, {9, 16, 9}, "chain", {6, 0, 10, 16}); // (our chain art: columns 6..9)
        bar.faces[int(Direction::Up)] = {nullptr};
        bar.faces[int(Direction::Down)] = {nullptr};
        b.box(bar, a == 0 ? Rot{1, 1} : a == 2 ? Rot{1, 0} : Rot{});
        return true;
    }
    case B::Ladder: { // a flat panel on the block behind it, the ladder texture facing out
        Builder b(atlas, out);
        BoxSpec panel{{0, 0, 15}, {16, 16, 16}, {}};
        panel.faces[int(Direction::North)] = {"ladder"};
        panel.faces[int(Direction::South)] = {"ladder"};
        b.box(panel, {0, yTurns(hFacing(r, s))});
        return true;
    }
    case B::RedstoneTorch:
    case B::RedstoneWallTorch: {
        Builder b(atlas, out);
        const char* sprite = r.get(s, P::lit) == 0 ? "redstone_torch" : "redstone_torch_off";
        if (block == B::RedstoneTorch) {
            b.box(torchStick({7, 0, 7}, 10, sprite));
        } else {
            // Upright against the wall (vanilla tilts it 22.5 degrees: known deviation).
            b.box(torchStick({7, 3, 12}, 10, sprite), {0, yTurns(hFacing(r, s))});
        }
        return true;
    }
    case B::Repeater: {
        // Built facing north (input north, output south).
        Builder b(atlas, out);
        const bool on = r.get(s, P::powered) == 0;
        const char* torch = on ? "redstone_torch" : "redstone_torch_off";
        const Rot rot{0, yTurns(hFacing(r, s))};
        BoxSpec base = allFaces({0, 0, 0}, {16, 2, 16}, "smooth_stone", {0, 14, 16, 16});
        base.faces[int(Direction::Up)] = {on ? "repeater_on" : "repeater"};
        base.faces[int(Direction::Down)] = {"smooth_stone"};
        b.box(base, rot);
        b.box(torchStick({7, 2, 12}, 5, torch), rot); // output side
        const int z = 2 + 2 * (r.get(s, P::delay) + 1);
        if (r.get(s, P::locked) == 0) {
            b.box(allFaces({2, 2, z}, {14, 4, z + 2}, "bedrock", {2, 2, 14, 4}), rot); // the lock bar
        } else {
            b.box(torchStick({7, 2, z}, 5, torch), rot);
        }
        return true;
    }
    case B::Comparator: {
        // Like the repeater (input north, output south): two torches at the back lit
        // while it outputs, one at the front lit in subtract mode (wiki: Comparator).
        Builder b(atlas, out);
        const bool on = r.get(s, P::powered) == 0;
        const bool subtract = r.get(s, P::comparatorMode) == 1;
        const Rot rot{0, yTurns(hFacing(r, s))};
        BoxSpec base = allFaces({0, 0, 0}, {16, 2, 16}, "smooth_stone", {0, 14, 16, 16});
        base.faces[int(Direction::Up)] = {on ? "comparator_on" : "comparator"};
        base.faces[int(Direction::Down)] = {"smooth_stone"};
        b.box(base, rot);
        const char* back = on ? "redstone_torch" : "redstone_torch_off";
        b.box(torchStick({4, 2, 2}, 5, back), rot);
        b.box(torchStick({10, 2, 2}, 5, back), rot);
        b.box(torchStick({7, 2, 11}, subtract ? 5 : 4, subtract ? "redstone_torch" : "redstone_torch_off"), rot);
        return true;
    }
    case B::Observer: {
        // Built looking north: the face at -Z, the back (red when on) at +Z, the arrow
        // on top pointing from face to back.
        Builder b(atlas, out);
        const auto facing = static_cast<Direction>(r.get(s, P::facing6));
        static constexpr Rot kFacingRot[6] = {{3, 0}, {1, 0}, {0, 0}, {0, 2}, {0, 3}, {0, 1}}; // by Direction
        FaceSpec faces[6];
        const glm::ivec3 north{0, 0, -1};
        faces[int(Direction::North)] = {"observer_front"};
        faces[int(Direction::South)] = {r.get(s, P::powered) == 0 ? "observer_back_on" : "observer_back"};
        faces[int(Direction::Up)] = {"observer_top", {0, 0, 16, 16}, north};
        faces[int(Direction::Down)] = {"observer_top", {0, 0, 16, 16}, north};
        faces[int(Direction::East)] = {"observer_side", {0, 0, 16, 16}, north};
        faces[int(Direction::West)] = {"observer_side", {0, 0, 16, 16}, north};
        b.cube(faces, kFacingRot[int(facing)]);
        return true;
    }
    case B::Hopper: {
        // A wide bowl on top, a narrower middle, and a spout down or to its side
        // (vanilla's shape, our boxes).
        Builder b(atlas, out);
        BoxSpec bowl = allFaces({0, 10, 0}, {16, 16, 16}, "hopper_outside", {0, 0, 16, 6});
        bowl.faces[int(Direction::Up)] = {"hopper_top"};
        bowl.faces[int(Direction::Down)] = {"hopper_outside"};
        b.box(bowl);
        b.box(allFaces({4, 4, 4}, {12, 10, 12}, "hopper_outside", {4, 6, 12, 12}));
        const int f = r.get(s, P::hopperFacing); // down, north, south, west, east
        if (f == 0) {
            b.box(allFaces({6, 0, 6}, {10, 4, 10}, "hopper_outside", {6, 12, 10, 16}));
        } else {
            static constexpr int kTurns[5] = {0, 0, 2, 3, 1}; // north as built, then turned
            b.box(allFaces({6, 4, 0}, {10, 8, 4}, "hopper_outside", {6, 8, 10, 12}), {0, kTurns[f]});
        }
        return true;
    }
    case B::Dispenser:
    case B::Dropper: {
        // Built facing north: the front (its mouth) at -Z, furnace-like sides and top.
        Builder b(atlas, out);
        const auto facing = static_cast<Direction>(r.get(s, P::facing6));
        const bool vertical = facing == Direction::Up || facing == Direction::Down;
        const bool dropper = block == B::Dropper;
        const char* front = dropper ? (vertical ? "dropper_front_vertical" : "dropper_front")
                                    : (vertical ? "dispenser_front_vertical" : "dispenser_front");
        static constexpr Rot kFacingRot[6] = {{3, 0}, {1, 0}, {0, 0}, {0, 2}, {0, 3}, {0, 1}}; // by Direction
        FaceSpec faces[6];
        faces[int(Direction::North)] = {front};
        faces[int(Direction::South)] = {"furnace_side"};
        faces[int(Direction::Up)] = {vertical ? "furnace_side" : "furnace_top"};
        faces[int(Direction::Down)] = {vertical ? "furnace_side" : "furnace_top"};
        faces[int(Direction::East)] = {"furnace_side"};
        faces[int(Direction::West)] = {"furnace_side"};
        b.cube(faces, kFacingRot[int(facing)]);
        return true;
    }
    case B::Beacon: {
        // Vanilla's model: an obsidian base and the glowing core inside a glass shell.
        Builder b(atlas, out);
        b.box(allFaces({2, 0, 2}, {14, 3, 14}, "obsidian", {2, 2, 14, 14}));
        b.box(allFaces({3, 3, 3}, {13, 14, 13}, "beacon", {3, 3, 13, 13}));
        b.box(allFaces({0, 0, 0}, {16, 16, 16}, "glass"));
        return true;
    }
    case B::Conduit: { // (placeholder: vanilla draws a cage and an eye as an entity)
        Builder b(atlas, out);
        b.box(allFaces({5, 5, 5}, {11, 11, 11}, "conduit", {5, 5, 11, 11}));
        return true;
    }
    case B::Stonecutter: {
        // A 9-high base and the saw blade standing across it (animated, cutout).
        Builder b(atlas, out);
        BoxSpec base = allFaces({0, 0, 0}, {16, 9, 16}, "stonecutter_side", {0, 7, 16, 16});
        base.faces[int(Direction::Up)] = {"stonecutter_top"};
        base.faces[int(Direction::Down)] = {"stonecutter_bottom"};
        b.box(base);
        // (our saw texture draws the blade in rows 6-9: 4 pixels above the base)
        BoxSpec saw{{1, 9, 8}, {15, 13, 8}, {}};
        saw.faces[int(Direction::North)] = {"stonecutter_saw", {1, 6, 15, 10}};
        saw.faces[int(Direction::South)] = {"stonecutter_saw", {1, 6, 15, 10}};
        b.box(saw, {0, yTurns(hFacing(r, s))});
        return true;
    }
    case B::Grindstone: {
        // Built on the floor facing north: the stone wheel between two pivots on dark
        // oak legs; turned like a lever for walls and ceilings.
        Builder b(atlas, out);
        const Rot rot = attachRot(r, s);
        BoxSpec wheel = allFaces({4, 4, 2}, {12, 16, 14}, "grindstone_round", {0, 0, 8, 12});
        wheel.faces[int(Direction::East)] = {"grindstone_side", {2, 0, 14, 12}};
        wheel.faces[int(Direction::West)] = {"grindstone_side", {2, 0, 14, 12}};
        b.box(wheel, rot);
        for (const int x : {2, 12}) {
            b.box(allFaces({x, 7, 5}, {x + 2, 13, 11}, "grindstone_pivot", {0, 0, 6, 6}), rot);
            b.box(allFaces({x, 0, 6}, {x + 2, 7, 10}, "dark_oak_log", {6, 9, 10, 16}), rot);
        }
        return true;
    }
    case B::Composter:
    case B::Cauldron:
    case B::WaterCauldron:
    case B::LavaCauldron:
    case B::PowderSnowCauldron: {
        // An open box: four 2-thick walls (outside and inside faces, the rim on top), a
        // floor (composter 2 high; cauldron at 3-4 over legs cut out of the side
        // texture), and the contents as a top face at their level.
        Builder b(atlas, out);
        const bool composter = block == B::Composter;
        const char* side = composter ? "composter_side" : "cauldron_side";
        const char* top = composter ? "composter_top" : "cauldron_top";
        const char* inner = composter ? "composter_side" : "cauldron_inner";
        auto wall = [&](glm::ivec3 from, glm::ivec3 to) {
            BoxSpec w = allFaces(from, to, side, {0, 0, 16, 16});
            w.faces[int(Direction::Up)] = {top, {uint8_t(from.x), uint8_t(from.z), uint8_t(to.x), uint8_t(to.z)}};
            w.faces[int(Direction::Down)] = {composter ? "composter_bottom" : "cauldron_bottom",
                                             {uint8_t(from.x), uint8_t(from.z), uint8_t(to.x), uint8_t(to.z)}};
            // Inside faces show the inner texture.
            if (from.z == 0 && to.z == 2) w.faces[int(Direction::South)] = {inner};
            if (from.z == 14) w.faces[int(Direction::North)] = {inner};
            if (from.x == 0 && to.x == 2) w.faces[int(Direction::East)] = {inner};
            if (from.x == 14) w.faces[int(Direction::West)] = {inner};
            b.box(w);
        };
        wall({0, 0, 0}, {16, 16, 2});
        wall({0, 0, 14}, {16, 16, 16});
        wall({0, 0, 2}, {2, 16, 14});
        wall({14, 0, 2}, {16, 16, 14});
        const int floorLo = composter ? 0 : 3, floorHi = composter ? 2 : 4;
        BoxSpec floor{{2, floorLo, 2}, {14, floorHi, 14}, {}};
        floor.faces[int(Direction::Up)] = {composter ? "composter_bottom" : "cauldron_inner", {2, 2, 14, 14}};
        floor.faces[int(Direction::Down)] = {composter ? "composter_bottom" : "cauldron_bottom", {2, 2, 14, 14}};
        b.box(floor);
        int level = 0; // contents' top in pixels (vanilla: water 6 + 3 x level, compost 1 + 2 x level)
        const char* contents = nullptr;
        Tint tint = Tint::None;
        if (composter) {
            const int l = r.get(s, P::composterLevel);
            if (l > 0) {
                level = l == 8 ? 15 : 1 + 2 * l;
                contents = l == 8 ? "composter_ready" : "composter_compost";
            }
        } else if (block == B::WaterCauldron || block == B::PowderSnowCauldron) {
            level = 6 + 3 * (r.get(s, P::cauldronLevel) + 1);
            contents = block == B::WaterCauldron ? "water_still" : "powder_snow";
            if (block == B::WaterCauldron) tint = Tint::Water;
        } else if (block == B::LavaCauldron) {
            level = 15;
            contents = "lava_still";
        }
        if (contents) {
            BoxSpec c{{2, level - 1, 2}, {14, level, 14}, {}};
            c.faces[int(Direction::Up)] = {contents, {2, 2, 14, 14}, glm::ivec3(0), tint};
            b.box(c);
        }
        return true;
    }
    case B::Barrel: {
        // Built facing up: the lid on top (open: the dark inside), turned like a dispenser.
        Builder b(atlas, out);
        const auto facing = static_cast<Direction>(r.get(s, P::facing6));
        static constexpr Rot kUpRot[6] = {{2, 0}, {0, 0}, {3, 0}, {1, 0}, {1, 1}, {1, 3}}; // by Direction
        FaceSpec faces[6];
        faces[int(Direction::Up)] = {r.get(s, P::open) == 0 ? "barrel_top_open" : "barrel_top"};
        faces[int(Direction::Down)] = {"barrel_bottom"};
        for (Direction d : {Direction::North, Direction::South, Direction::East, Direction::West})
            faces[int(d)] = {"barrel_side"};
        b.cube(faces, kUpRot[int(facing)]);
        return true;
    }
    case B::Lever: {
        // A cobblestone base and a handle leaning to the off or on side.
        Builder b(atlas, out);
        const Rot rot = attachRot(r, s);
        b.box(allFaces({5, 0, 4}, {11, 3, 12}, "cobblestone", {5, 4, 11, 12}), rot);
        const int z = r.get(s, P::powered) == 0 ? 9 : 5;
        BoxSpec handle = allFaces({7, 3, z}, {9, 12, z + 2}, "lever", {7, 6, 9, 15});
        handle.faces[int(Direction::Up)] = {"lever", {7, 6, 9, 8}};
        b.box(handle, rot);
        return true;
    }
    case B::StoneButton:
    case B::OakButton: {
        Builder b(atlas, out);
        // Its material: planks for wooden buttons, else stone / polished blackstone.
        const std::string prefix(realName.substr(0, realName.size() - 7));
        const std::string tex = prefix == "stone" || prefix == "polished_blackstone" ? prefix : prefix + "_planks";
        const char* sprite = tex.c_str();
        const int h = r.get(s, P::powered) == 0 ? 1 : 2; // pressed buttons sink in
        b.box(allFaces({5, 0, 6}, {11, h, 10}, sprite, {5, 6, 11, 10}), attachRot(r, s));
        return true;
    }
    case B::RedstoneLamp: {
        Builder b(atlas, out);
        const char* sprite = r.get(s, P::lit) == 0 ? "redstone_lamp_on" : "redstone_lamp";
        FaceSpec faces[6];
        for (auto& f : faces)
            f.sprite = sprite;
        b.cube(faces);
        return true;
    }
    case B::Piston:
    case B::StickyPiston:
    case B::PistonHead: {
        // Built facing north: the front (head) at -Z; side textures point to the front.
        Builder b(atlas, out);
        const auto facing = static_cast<Direction>(r.get(s, P::facing6));
        static constexpr Rot kFacingRot[6] = {{3, 0}, {1, 0}, {0, 0}, {0, 2}, {0, 3}, {0, 1}}; // by Direction
        const Rot rot = kFacingRot[int(facing)];
        const bool sticky = block == B::StickyPiston || (block == B::PistonHead && r.get(s, P::pistonType) == 1);
        const char* front = sticky ? "piston_top_sticky" : "piston_top";
        const glm::ivec3 north{0, 0, -1};
        auto side = [&](uint8_t v0, uint8_t v1) { return FaceSpec{"piston_side", {0, v0, 16, v1}, north}; };
        if (block == B::PistonHead) {
            BoxSpec plate{{0, 0, 0}, {16, 16, 4}, {}};
            plate.faces[int(Direction::North)] = {front};
            plate.faces[int(Direction::South)] = {front};
            for (Direction d : {Direction::East, Direction::West, Direction::Up, Direction::Down})
                plate.faces[int(d)] = side(0, 4);
            b.box(plate, rot);
            BoxSpec arm{{6, 6, 4}, {10, 10, 16}, {}};
            for (Direction d : {Direction::East, Direction::West, Direction::Up, Direction::Down})
                arm.faces[int(d)] = {"piston_side", {6, 4, 10, 16}, north};
            b.box(arm, rot);
            return true;
        }
        if (r.get(s, P::extended) == 0) {
            BoxSpec base{{0, 0, 4}, {16, 16, 16}, {}};
            base.faces[int(Direction::North)] = {"piston_inner"};
            base.faces[int(Direction::South)] = {"piston_bottom"};
            for (Direction d : {Direction::East, Direction::West, Direction::Up, Direction::Down})
                base.faces[int(d)] = side(4, 16);
            b.box(base, rot);
            BoxSpec stub{{6, 6, 0}, {10, 10, 4}, {}}; // the arm's end inside the base cell
            for (Direction d : {Direction::East, Direction::West, Direction::Up, Direction::Down})
                stub.faces[int(d)] = {"piston_side", {6, 12, 10, 16}, north};
            b.box(stub, rot);
            return true;
        }
        FaceSpec faces[6];
        faces[int(Direction::North)] = {front};
        faces[int(Direction::South)] = {"piston_bottom"};
        for (Direction d : {Direction::East, Direction::West, Direction::Up, Direction::Down})
            faces[int(d)] = side(0, 16);
        b.cube(faces, rot);
        return true;
    }
    // --- Dimensions (M12) ---
    case B::NetherPortal: {
        // A pane in the middle of the block, across its axis (both sides drawn).
        Builder b(atlas, out);
        BoxSpec pane{{0, 0, 6}, {16, 16, 10}, {}};
        pane.faces[int(Direction::North)] = {"nether_portal"};
        pane.faces[int(Direction::South)] = {"nether_portal"};
        b.box(pane, {0, r.get(s, P::haxis) == 0 ? 0 : 1});
        out.translucent = true;
        return true;
    }
    case B::EndPortal: {
        Builder b(atlas, out);
        BoxSpec surface{{0, 0, 0}, {16, 12, 16}, {}}; // the top at 12/16 (vanilla)
        surface.faces[int(Direction::Up)] = {"end_portal"};
        b.box(surface);
        return true;
    }
    case B::EndPortalFrame: {
        Builder b(atlas, out);
        const Rot rot{0, yTurns(hFacing(r, s))};
        BoxSpec frame = allFaces({0, 0, 0}, {16, 13, 16}, "end_portal_frame_side", {0, 3, 16, 16});
        frame.faces[int(Direction::Up)] = {"end_portal_frame_top"};
        frame.faces[int(Direction::Down)] = {"end_stone"};
        b.box(frame, rot);
        if (r.get(s, P::eye) == 0) {
            BoxSpec eye = allFaces({4, 13, 4}, {12, 16, 12}, "end_portal_frame_eye", {4, 0, 12, 3});
            eye.faces[int(Direction::Up)] = {"end_portal_frame_eye", {4, 4, 12, 12}};
            b.box(eye, rot);
        }
        return true;
    }
    case B::MagmaBlock: {
        Builder b(atlas, out);
        FaceSpec faces[6];
        for (auto& f : faces)
            f.sprite = "magma";
        b.cube(faces);
        return true;
    }
    default: return false;
    }
}

} // namespace mc::gfx
