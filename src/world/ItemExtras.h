#pragma once

#include "world/Banners.h"
#include "world/Chunk.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mc::world {

// Data some items carry that doesn't fit ItemStack's small fields (M28.2), kept in a
// table like world/ItemContainers: the stack holds an id (`ItemStack::extra`), entries
// never change once added and are never freed (copies of a stack share one).
// Thread-safe: chunk loading and saving add and read entries on worker threads.

// A lodestone compass's target (vanilla minecraft:lodestone_tracker): the lodestone and
// its dimension; `hasTarget` false once the lodestone is gone (the needle spins).
struct LodestoneTarget {
    BlockPos pos{};
    uint8_t dimension = 0; // world::Dimension
    bool hasTarget = true;
    bool tracked = true; // (vanilla: false for /give-made compasses that never lose it)
    bool operator==(const LodestoneTarget&) const = default;
};
uint32_t addLodestoneTarget(const LodestoneTarget& t);
std::optional<LodestoneTarget> lodestoneTarget(uint32_t id);

// A book and quill's pages, or a written book's (vanilla minecraft:writable_book_content /
// written_book_content: title, author, generation 0 original .. 3 tattered).
struct BookContent {
    std::string title, author;
    int generation = 0;
    std::vector<std::string> pages;
    bool operator==(const BookContent&) const = default;
};
uint32_t addBook(BookContent book);
std::optional<BookContent> bookContent(uint32_t id);

// Fireworks (M28.4c; wiki: Firework Rocket, Firework Star; vanilla minecraft:fireworks /
// firework_explosion): a rocket's flight duration (1-3) and explosions (a star has one).
// Colours are dye bit masks (bit = dye index; saved as vanilla's RGB ints).
struct FireworkExplosion {
    uint8_t shape = 0; // 0 small ball, 1 large ball, 2 star, 3 creeper, 4 burst
    uint16_t colours = 0, fades = 0;
    bool trail = false, twinkle = false;
    bool operator==(const FireworkExplosion&) const = default;
};
struct Fireworks {
    static constexpr int kMax = 7;
    uint8_t flight = 1;
    uint8_t count = 0;
    std::array<FireworkExplosion, kMax> explosions{};
    bool operator==(const Fireworks&) const = default;
};
inline constexpr std::string_view kFireworkShapes[5] = {"small_ball", "large_ball", "star",
                                                        "creeper", "burst"};
// The colours fireworks use for each dye (wiki: Firework Star › Colors), 0xRRGGBB.
inline constexpr uint32_t kFireworkColours[16] = {
    0xF0F0F0, 0xEB8844, 0xC354CD, 0x6689D3, 0xDECF2A, 0x41CD34, 0xD88198, 0x434343,
    0xABABAB, 0x287697, 0x7B2FBE, 0x253192, 0x51301A, 0x3B511A, 0xB3312C, 0x1E1B1B};
// Equal fireworks share an entry (they stack).
uint32_t addFireworks(const Fireworks& f);
std::optional<Fireworks> fireworks(uint32_t id);

// A banner item's layers (M28.3d; vanilla minecraft:banner_patterns). Identical layers
// share one entry, so such banners stack.
uint32_t addBannerLayers(const BannerLayers& layers);
std::optional<BannerLayers> bannerLayers(uint32_t id);

} // namespace mc::world
