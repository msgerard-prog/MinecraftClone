// The chunk mesher is GL-free: geometry, culling and texture orientation are
// unit-tested here (GPU output is covered by screenshots).
#include "rendering/ChunkMesher.h"
#include "world/Blocks.h"
#include "world/FlatGenerator.h"
#include "world/SectionSnapshot.h"

#include <doctest/doctest.h>

#include <vector>

using namespace mc::world;
using mc::gfx::PackedVertex;
using mc::gfx::unpackVertex;

namespace {

// Models: every visible state uses sprite 7 (or 9 on east faces) with no rotation.
mc::gfx::BlockModels testModels() {
    mc::gfx::BlockModels m;
    const auto& r = blockRegistry();
    m.resize(r.stateCount());
    for (size_t s = 1; s < r.stateCount(); ++s) {
        auto& model = m.at(BlockStateId(s));
        model.visible = true;
        for (auto& f : model.variants[0].faces)
            f.sprite = 7;
        model.variants[0].faces[int(Direction::East)].sprite = 9;
    }
    return m;
}

std::vector<PackedVertex> meshOf(const World& world, SectionPos pos) {
    std::vector<BlockStateId> padded(kPaddedVolume);
    snapshotSection(world, pos, padded.data());
    mc::gfx::SectionMesh out;
    mc::gfx::meshSection(padded.data(), glm::ivec3(pos.x * 16, pos.y * 16, pos.z * 16),
                         blockRegistry(), testModels(), out);
    return out.opaque;
}

BlockStateId stone() { return blockRegistry().defaultState(blocks::Stone); }

// Vertex position in blocks.
glm::vec3 posOf(const mc::gfx::VertexAttribs& a) {
    return glm::vec3(float(a.x16), float(a.y16), float(a.z16)) / 16.0f;
}

// Full-face UV -> sprite corner: 0 (0,0) 1 (0,16) 2 (16,16) 3 (16,0).
uint32_t uvCorner(const mc::gfx::VertexAttribs& a) {
    return a.u ? (a.v ? 2u : 3u) : (a.v ? 1u : 0u);
}

} // namespace

TEST_CASE("a lone block has 6 faces; two touching blocks hide their shared faces") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({5, 5, 5}, stone());
    CHECK(meshOf(w, {0, 0, 0}).size() == 6 * 4);
    w.setBlock({6, 5, 5}, stone());
    CHECK(meshOf(w, {0, 0, 0}).size() == 10 * 4);
}

TEST_CASE("faces against a neighbouring chunk are culled across the border") {
    World w;
    w.createChunk({0, 0});
    w.createChunk({1, 0});
    w.setBlock({15, 0, 0}, stone()); // east edge of chunk 0
    w.setBlock({16, 0, 0}, stone()); // west edge of chunk 1
    CHECK(meshOf(w, {0, 0, 0}).size() == 5 * 4);
    CHECK(meshOf(w, {1, 0, 0}).size() == 5 * 4);
}

TEST_CASE("faces against the section above/below are culled") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({0, 15, 0}, stone()); // top of section y=0
    w.setBlock({0, 16, 0}, stone()); // bottom of section y=1
    CHECK(meshOf(w, {0, 0, 0}).size() == 5 * 4);
    CHECK(meshOf(w, {0, 1, 0}).size() == 5 * 4);
}

TEST_CASE("flat world section meshes only its top surface and outer walls") {
    World w;
    const auto gen = FlatGenerator::fromPreset(FlatGenerator::kClassicFlat);
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x)
            gen->generate(w.createChunk({x, z}));
    // Surrounded by loaded chunks: only the 256 grass tops are visible.
    // (The bedrock floor's down faces at y = -64 face the void and are kept.)
    const auto verts = meshOf(w, {0, -4, 0});
    CHECK(verts.size() == (256 + 256) * 4);
}

TEST_CASE("every quad is counter-clockwise seen from outside") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({3, 4, 5}, stone());
    const auto verts = meshOf(w, {0, 0, 0});
    REQUIRE(verts.size() == 24);
    const glm::vec3 center(3.5f, 4.5f, 5.5f);
    for (size_t q = 0; q < verts.size(); q += 4) {
        glm::vec3 p[4];
        for (int i = 0; i < 4; ++i) {
            const auto u = unpackVertex(verts[q + i]);
            p[i] = posOf(u);
        }
        // Triangles 0-1-2 and 0-2-3 (the shared index buffer's order).
        const glm::vec3 outward = (p[0] + p[2]) * 0.5f - center;
        CHECK(glm::dot(glm::cross(p[1] - p[0], p[2] - p[0]), outward) > 0.0f);
        CHECK(glm::dot(glm::cross(p[2] - p[0], p[3] - p[0]), outward) > 0.0f);
    }
}

TEST_CASE("texture orientation: top of sprite = +Y on sides, north on up, south on down") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({0, 0, 0}, stone());
    const auto verts = meshOf(w, {0, 0, 0});
    // uvCorner -> (u, v) in sprite units: 0 (0,0) 1 (0,1) 2 (1,1) 3 (1,0)
    constexpr float kU[4] = {0, 0, 1, 1};
    constexpr float kV[4] = {0, 1, 1, 0};
    for (size_t q = 0; q < verts.size(); q += 4) {
        const auto face = static_cast<Direction>(unpackVertex(verts[q]).face);
        glm::vec3 texUp(0, 1, 0);
        if (face == Direction::Up) texUp = {0, 0, -1};
        if (face == Direction::Down) texUp = {0, 0, 1};
        const glm::vec3 n(normal(face));
        const glm::vec3 texRight = glm::cross(-n, texUp); // viewer looks along -n
        for (int i = 0; i < 4; ++i) {
            const auto u = unpackVertex(verts[q + i]);
            const glm::vec3 p = posOf(u) - glm::vec3(0.5f);
            INFO("face ", int(face), " corner ", i);
            CHECK(kV[uvCorner(u)] == (glm::dot(p, texUp) > 0 ? 0.0f : 1.0f));    // v0 = top
            CHECK(kU[uvCorner(u)] == (glm::dot(p, texRight) > 0 ? 1.0f : 0.0f)); // u0 = left
            CHECK(u.sprite == (face == Direction::East ? 9u : 7u));
        }
    }
}

TEST_CASE("a tinted face carries its tint into the vertices (grass top)") {
    const auto& r = blockRegistry();
    mc::gfx::BlockModels models = testModels();
    auto& grass = models.at(r.defaultState(blocks::GrassBlock));
    grass.variants[0].faces[int(Direction::Up)].tint = mc::gfx::Tint::Grass;
    World w;
    w.createChunk({0, 0});
    w.setBlock({0, 0, 0}, r.defaultState(blocks::GrassBlock));
    std::vector<BlockStateId> padded(kPaddedVolume);
    snapshotSection(w, {0, 0, 0}, padded.data());
    mc::gfx::SectionMesh out;
    mc::gfx::meshSection(padded.data(), glm::ivec3(0), r, models, out);
    int tinted = 0;
    for (const auto& v : out.opaque) {
        const auto u = unpackVertex(v);
        if (u.tint == mc::gfx::Tint::Grass) {
            ++tinted;
            CHECK(u.face == uint32_t(Direction::Up));
        }
    }
    CHECK(tinted == 4);
}

TEST_CASE("packVertex/unpackVertex round-trip the full field ranges") {
    mc::gfx::VertexAttribs a{};
    a.x16 = 256;
    a.y16 = 0;
    a.z16 = 255;
    a.face = 5;
    a.sprite = mc::gfx::kMaxSprites - 1;
    a.u = 16;
    a.v = 7;
    a.tint = mc::gfx::Tint::Water;
    a.fluidTop = true;
    a.ao = 3;
    a.sky4 = 60;
    a.block4 = 33;
    const auto u = unpackVertex(mc::gfx::packVertex(a));
    CHECK(u.x16 == 256);
    CHECK(u.y16 == 0);
    CHECK(u.z16 == 255);
    CHECK(u.face == 5);
    CHECK(u.sprite == mc::gfx::kMaxSprites - 1);
    CHECK(u.u == 16);
    CHECK(u.v == 7);
    CHECK(u.tint == mc::gfx::Tint::Water);
    CHECK(u.fluidTop);
    CHECK(u.ao == 3);
    CHECK(u.sky4 == 60);
    CHECK(u.block4 == 33);
}

TEST_CASE("face shade table matches vanilla") {
    CHECK(mc::gfx::kFaceShade[int(Direction::Up)] == 1.0f);
    CHECK(mc::gfx::kFaceShade[int(Direction::Down)] == 0.5f);
    CHECK(mc::gfx::kFaceShade[int(Direction::North)] == 0.8f);
    CHECK(mc::gfx::kFaceShade[int(Direction::South)] == 0.8f);
    CHECK(mc::gfx::kFaceShade[int(Direction::West)] == 0.6f);
    CHECK(mc::gfx::kFaceShade[int(Direction::East)] == 0.6f);
}

TEST_CASE("variant choice is deterministic per position and uses every variant") {
    int counts[4] = {};
    for (int x = -20; x < 20; ++x)
        for (int z = -20; z < 20; ++z) {
            const uint32_t v = mc::gfx::variantIndex(x, -61, z, 4);
            CHECK(v == mc::gfx::variantIndex(x, -61, z, 4));
            ++counts[v];
        }
    for (int c : counts)
        CHECK(c > 250); // 1600 samples, ~400 each
    CHECK(mc::gfx::variantIndex(5, 5, 5, 1) == 0);
}

TEST_CASE("rotation and mirroring move UV corners as documented") {
    World w;
    w.createChunk({0, 0});
    w.setBlock({0, 0, 0}, stone());
    mc::gfx::BlockModels models = testModels();
    auto& face = models.at(stone()).variants[0].faces[int(Direction::South)];
    std::vector<BlockStateId> padded(kPaddedVolume);
    snapshotSection(w, {0, 0, 0}, padded.data());
    auto southCorners = [&]() {
        mc::gfx::SectionMesh out;
        mc::gfx::meshSection(padded.data(), glm::ivec3(0), blockRegistry(), models, out);
        std::vector<uint32_t> uv;
        for (const auto& v : out.opaque)
            if (unpackVertex(v).face == uint32_t(Direction::South))
                uv.push_back(uvCorner(unpackVertex(v)));
        return uv;
    };
    CHECK(southCorners() == std::vector<uint32_t>{0, 1, 2, 3});
    face.rotation = 1; // 90 degrees clockwise: top-left corner shows the bottom-left texel
    CHECK(southCorners() == std::vector<uint32_t>{1, 2, 3, 0});
    face.rotation = 0;
    face.mirror = true; // left-right flip
    CHECK(southCorners() == std::vector<uint32_t>{3, 2, 1, 0});
}

TEST_CASE("rotateY turns the up face one way and the down face the other") {
    mc::gfx::BakedVariant v;
    const auto r = mc::gfx::rotateY(v, 1);
    CHECK(r.faces[int(Direction::Up)].rotation == 1);
    CHECK(r.faces[int(Direction::Down)].rotation == 3);
    CHECK(r.faces[int(Direction::North)].rotation == 0);
}

TEST_CASE("log textures: ends on the axis faces, side rotations as vanilla horizontal logs") {
    using mc::gfx::cubeColumn;
    auto rot = [](const mc::gfx::BakedVariant& v, Direction d) {
        return int(v.faces[int(d)].rotation);
    };
    const auto y = cubeColumn(1, 2, "y");
    CHECK(y.faces[int(Direction::Up)].sprite == 2);
    CHECK(y.faces[int(Direction::Down)].sprite == 2);
    CHECK(y.faces[int(Direction::North)].sprite == 1);
    const auto z = cubeColumn(1, 2, "z");
    CHECK(z.faces[int(Direction::North)].sprite == 2);
    CHECK(z.faces[int(Direction::South)].sprite == 2);
    CHECK(rot(z, Direction::Up) == 0);
    CHECK(rot(z, Direction::Down) == 2);
    CHECK(rot(z, Direction::West) == 3);
    CHECK(rot(z, Direction::East) == 1);
    const auto x = cubeColumn(1, 2, "x");
    CHECK(x.faces[int(Direction::West)].sprite == 2);
    CHECK(x.faces[int(Direction::East)].sprite == 2);
    CHECK(rot(x, Direction::Up) == 1);
    CHECK(rot(x, Direction::Down) == 1);
    CHECK(rot(x, Direction::North) == 3);
    CHECK(rot(x, Direction::South) == 1);
}

TEST_CASE("water: translucent pass, hidden against water, surface lowered under air") {
    const auto& r = blockRegistry();
    mc::gfx::BlockModels models = testModels();
    const BlockStateId water = r.defaultState(blocks::Water);
    auto& wm = models.at(water);
    wm.translucent = true;
    wm.fluid = true;
    wm.cullSame = true; // as the real water model
    World w;
    w.createChunk({0, 0});
    // A 2x1x1 pool of water with a stone floor under it.
    w.setBlock({4, 5, 4}, water);
    w.setBlock({5, 5, 4}, water);
    w.setBlock({4, 4, 4}, stone());
    w.setBlock({5, 4, 4}, stone());
    std::vector<BlockStateId> padded(kPaddedVolume);
    snapshotSection(w, {0, 0, 0}, padded.data());
    mc::gfx::SectionMesh out;
    mc::gfx::meshSection(padded.data(), glm::ivec3(0), r, models, out);
    // Water: 2 blocks x 6 faces - 2 shared - 2 on the stone floor = 8 faces (2 tops,
    // 6 sides), each emitted twice (front + reversed back face).
    CHECK(out.translucent.size() == 8 * 2 * 4);
    int lowered = 0;
    for (const auto& v : out.translucent) {
        const auto u = mc::gfx::unpackVertex(v);
        if (u.fluidTop) {
            ++lowered;
            CHECK(u.y16 == 6 * 16); // only top-edge vertices (y + 1) are lowered
        }
    }
    CHECK(lowered == 2 * (2 * 4 + 6 * 2)); // (2 tops x 4 + 6 sides x 2 top corners), both sides
    // Water isn't opaque, so the stone floor's top faces stay visible through it:
    // 2 stones x 6 faces - 2 shared = 10 opaque faces.
    CHECK(out.opaque.size() == 10 * 4);
}

TEST_CASE("water under a solid block keeps its lowered surface; bottom has no back face") {
    const auto& r = blockRegistry();
    mc::gfx::BlockModels models = testModels();
    const BlockStateId water = r.defaultState(blocks::Water);
    models.at(water).translucent = true;
    models.at(water).fluid = true;
    World w;
    w.createChunk({0, 0});
    w.setBlock({4, 5, 4}, water);
    w.setBlock({4, 6, 4}, stone()); // solid block directly on top
    std::vector<BlockStateId> padded(kPaddedVolume);
    snapshotSection(w, {0, 0, 0}, padded.data());
    mc::gfx::SectionMesh out;
    mc::gfx::meshSection(padded.data(), glm::ivec3(0), r, models, out);
    int upQuads = 0, downQuads = 0;
    for (size_t q = 0; q < out.translucent.size(); q += 4) {
        const auto face = static_cast<Direction>(mc::gfx::unpackVertex(out.translucent[q]).face);
        upQuads += face == Direction::Up;
        downQuads += face == Direction::Down;
    }
    CHECK(upQuads == 2);   // lowered top under stone: front + back
    CHECK(downQuads == 1); // bottom: front only (invisible from above)
}
