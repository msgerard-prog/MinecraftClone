// Resource packs: zip reading, pack priority, sprite helpers. Uses the fixtures from
// tools/make_test_pack.py (our own content).
#include "rendering/ResourcePack.h"
#include "rendering/SpriteImage.h"
#include "rendering/ZipArchive.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace mc::gfx;
namespace fs = std::filesystem;

namespace {

const fs::path kData = MC_TEST_DATA_DIR;

std::string text(const std::vector<uint8_t>& bytes) { return {bytes.begin(), bytes.end()}; }

// A throwaway folder pack holding one file.
fs::path makeFolderPack(const std::string& name, const std::string& file, const std::string& body) {
    const fs::path root = fs::temp_directory_path() / "mc_tests" / name;
    fs::create_directories((root / file).parent_path());
    std::ofstream(root / file, std::ios::binary) << body;
    return root;
}

} // namespace

TEST_CASE("zip: reads stored and deflated entries") {
    ZipArchive zip;
    REQUIRE(zip.open((kData / "testpack.zip").string()));
    const auto meta = zip.read("pack.mcmeta"); // stored
    REQUIRE(meta.has_value());
    CHECK(text(*meta).find("pack_format") != std::string::npos);
    const auto hello = zip.read("hello.txt"); // deflated
    REQUIRE(hello.has_value());
    std::string expected;
    for (int i = 0; i < 20; ++i)
        expected += "hello resource pack\n";
    CHECK(text(*hello) == expected);
    CHECK_FALSE(zip.read("nope.txt").has_value());
}

TEST_CASE("zip: rejects garbage") {
    ZipArchive zip;
    CHECK_FALSE(zip.openMemory(std::vector<uint8_t>(100, 0x42)));
}

TEST_CASE("folder and zip packs list the same textures") {
    const auto folder = ResourcePack::open(kData / "testpack");
    const auto zip = ResourcePack::open(kData / "testpack.zip");
    REQUIRE((folder && zip));
    const std::string dir = "assets/minecraft/textures/block/";
    const std::vector<std::string> expected = {"dirt.png", "sand.png", "stone.png"};
    CHECK(folder->list(dir, ".png") == expected);
    CHECK(zip->list(dir, ".png") == expected);
    CHECK(folder->read(dir + "dirt.png") == zip->read(dir + "dirt.png"));
}

TEST_CASE("pack stack: later packs override earlier ones file by file") {
    PackStack stack;
    stack.add(ResourcePack::open(makeFolderPack("low", "a/x.txt", "low")));
    stack.add(ResourcePack::open(makeFolderPack("high", "a/x.txt", "high")));
    stack.add(ResourcePack::open(makeFolderPack("other", "a/y.txt", "other")));
    CHECK(text(*stack.read("a/x.txt")) == "high");
    CHECK(text(*stack.read("a/y.txt")) == "other");
    CHECK(stack.list("a/", ".txt") == std::vector<std::string>{"x.txt", "y.txt"});
    CHECK_FALSE(stack.read("a/z.txt").has_value());
}

TEST_CASE("animation meta: frametime and defaults") {
    CHECK(parseAnimationMeta(R"({"animation": {"frametime": 5}})")->frametime == 5);
    CHECK(parseAnimationMeta(R"({"animation": {}})")->frametime == 1);
    CHECK_FALSE(parseAnimationMeta(R"({"villager": {}})").has_value());
}

TEST_CASE("sprite helpers: strip frames, nearest upscale, mean mipmap") {
    Image strip{2, 4, {}};
    for (int i = 0; i < 8; ++i)
        strip.pixels.insert(strip.pixels.end(), {uint8_t(i), 0, 0, 255});
    const Image f1 = stripFrame(strip, 1);
    CHECK(f1.width == 2);
    CHECK(f1.at(0, 0)[0] == 4);
    const Image up = upscaleNearest(f1, 2);
    CHECK(up.width == 4);
    CHECK(up.at(3, 3)[0] == 7);
    CHECK(up.at(1, 1)[0] == 4);
    const Image half = halve(f1);
    CHECK(half.width == 1);
    CHECK(half.at(0, 0)[0] == (4 + 5 + 6 + 7 + 2) / 4);
}

namespace {

// Minimal zip with one stored entry; fields can be corrupted by the caller.
std::vector<uint8_t> tinyZip(uint32_t claimedSize, uint32_t localOffset, uint16_t method = 0) {
    std::vector<uint8_t> z;
    auto u16 = [&](uint16_t v) {
        z.push_back(v & 0xFF);
        z.push_back(v >> 8);
    };
    auto u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i)
            z.push_back((v >> (8 * i)) & 0xFF);
    };
    const std::string name = "a.txt";
    const std::string body = "hi";
    // local header
    u32(0x04034b50);
    u16(20);
    u16(0);
    u16(method);
    u16(0);
    u16(0);
    u32(0);
    u32(static_cast<uint32_t>(body.size()));
    u32(claimedSize);
    u16(static_cast<uint16_t>(name.size()));
    u16(0);
    z.insert(z.end(), name.begin(), name.end());
    z.insert(z.end(), body.begin(), body.end());
    const auto cd = static_cast<uint32_t>(z.size());
    // central directory
    u32(0x02014b50);
    u16(20);
    u16(20);
    u16(0);
    u16(method);
    u16(0);
    u16(0);
    u32(0);
    u32(static_cast<uint32_t>(body.size()));
    u32(claimedSize);
    u16(static_cast<uint16_t>(name.size()));
    u16(0);
    u16(0);
    u16(0);
    u16(0);
    u32(0);
    u32(localOffset);
    z.insert(z.end(), name.begin(), name.end());
    const auto cdSize = static_cast<uint32_t>(z.size()) - cd;
    // end of central directory
    u32(0x06054b50);
    u16(0);
    u16(0);
    u16(1);
    u16(1);
    u32(cdSize);
    u32(cd);
    u16(0);
    return z;
}

} // namespace

TEST_CASE("zip: malformed entries are rejected, never allocated") {
    ZipArchive good;
    REQUIRE(good.openMemory(tinyZip(2, 0)));
    CHECK(text(*good.read("a.txt")) == "hi");

    ZipArchive huge; // claims 4 GiB uncompressed
    REQUIRE(huge.openMemory(tinyZip(0xFFFFFFF0u, 0, 8)));
    CHECK_FALSE(huge.read("a.txt").has_value());

    ZipArchive mismatch; // stored entry whose sizes disagree
    REQUIRE(mismatch.openMemory(tinyZip(5, 0)));
    CHECK_FALSE(mismatch.read("a.txt").has_value());

    ZipArchive badOffset; // local header offset past the end
    REQUIRE(badOffset.openMemory(tinyZip(2, 100000)));
    CHECK_FALSE(badOffset.read("a.txt").has_value());

    auto truncated = tinyZip(2, 0);
    truncated.resize(truncated.size() - 30); // cut into the central directory
    ZipArchive cut;
    CHECK_FALSE(cut.openMemory(truncated));
}

TEST_CASE("pack stack: a client .jar always sits below user packs") {
    const fs::path dir = fs::temp_directory_path() / "mc_tests" / "order";
    fs::remove_all(dir);
    fs::create_directories(dir / "A pack" / "a");
    std::ofstream(dir / "A pack" / "a" / "x.txt", std::ios::binary) << "user";
    fs::copy_file(kData / "testpack.zip", dir / "zz.jar"); // a "jar" sorted after "A pack"
    PackStack stack;
    stack.addAllIn(dir);
    REQUIRE(stack.size() == 2);
    CHECK(stack.pack(0).name() == "zz.jar"); // bottom
    CHECK(stack.pack(1).name() == "A pack");
}
