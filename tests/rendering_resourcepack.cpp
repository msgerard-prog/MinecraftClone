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
