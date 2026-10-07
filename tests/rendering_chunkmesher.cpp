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
        for (auto& f : model.faces)
            f.sprite = 7;
        model.faces[int(Direction::East)].sprite = 9;
    }
    return m;
}

std::vector<PackedVertex> meshOf(const World& world, SectionPos pos) {
    std::vector<BlockStateId> padded(kPaddedVolume);
    snapshotSection(world, pos, padded.data());
    std::vector<PackedVertex> out;
    mc::gfx::meshSection(padded.data(), blockRegistry(), testModels(), out);
    return out;
}

BlockStateId stone() { return blockRegistry().defaultState(blocks::Stone); }

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
            p[i] = {float(u.x), float(u.y), float(u.z)};
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
            const glm::vec3 p = glm::vec3(float(u.x), float(u.y), float(u.z)) - glm::vec3(0.5f);
            INFO("face ", int(face), " corner ", i);
            CHECK(kV[u.uvCorner] == (glm::dot(p, texUp) > 0 ? 0.0f : 1.0f));    // v0 = top
            CHECK(kU[u.uvCorner] == (glm::dot(p, texRight) > 0 ? 1.0f : 0.0f)); // u0 = left
            CHECK(u.sprite == (face == Direction::East ? 9u : 7u));
        }
    }
}

TEST_CASE("a tinted face carries its tint into the vertices (grass top)") {
    const auto& r = blockRegistry();
    mc::gfx::BlockModels models = testModels();
    auto& grass = models.at(r.defaultState(blocks::GrassBlock));
    grass.faces[int(Direction::Up)].tint = mc::gfx::Tint::Grass;
    World w;
    w.createChunk({0, 0});
    w.setBlock({0, 0, 0}, r.defaultState(blocks::GrassBlock));
    std::vector<BlockStateId> padded(kPaddedVolume);
    snapshotSection(w, {0, 0, 0}, padded.data());
    std::vector<PackedVertex> out;
    mc::gfx::meshSection(padded.data(), r, models, out);
    int tinted = 0;
    for (const auto& v : out) {
        const auto u = unpackVertex(v);
        if (u.tint == mc::gfx::Tint::Grass) {
            ++tinted;
            CHECK(u.face == uint32_t(Direction::Up));
        }
    }
    CHECK(tinted == 4);
}

TEST_CASE("packVertex/unpackVertex round-trip the full field ranges") {
    const auto v =
        mc::gfx::packVertex(16, 0, 16, 5, 3, mc::gfx::kMaxSprites - 1, mc::gfx::Tint::Grass);
    const auto u = unpackVertex(v);
    CHECK(u.x == 16);
    CHECK(u.y == 0);
    CHECK(u.z == 16);
    CHECK(u.face == 5);
    CHECK(u.uvCorner == 3);
    CHECK(u.sprite == mc::gfx::kMaxSprites - 1);
    CHECK(u.tint == mc::gfx::Tint::Grass);
}

TEST_CASE("face shade table matches vanilla") {
    CHECK(mc::gfx::kFaceShade[int(Direction::Up)] == 1.0f);
    CHECK(mc::gfx::kFaceShade[int(Direction::Down)] == 0.5f);
    CHECK(mc::gfx::kFaceShade[int(Direction::North)] == 0.8f);
    CHECK(mc::gfx::kFaceShade[int(Direction::South)] == 0.8f);
    CHECK(mc::gfx::kFaceShade[int(Direction::West)] == 0.6f);
    CHECK(mc::gfx::kFaceShade[int(Direction::East)] == 0.6f);
}
