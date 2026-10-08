#pragma once

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
};
uint32_t addLodestoneTarget(const LodestoneTarget& t);
std::optional<LodestoneTarget> lodestoneTarget(uint32_t id);

// A book and quill's pages, or a written book's (vanilla minecraft:writable_book_content /
// written_book_content: title, author, generation 0 original .. 3 tattered).
struct BookContent {
    std::string title, author;
    int generation = 0;
    std::vector<std::string> pages;
};
uint32_t addBook(BookContent book);
std::optional<BookContent> bookContent(uint32_t id);

} // namespace mc::world
