// The item data tables (M28.2, see the header).
#include "world/ItemExtras.h"

#include <deque>
#include <mutex>

namespace mc::world {

namespace {

template <typename T> struct Table {
    std::mutex lock;
    std::deque<T> entries; // index + 1 = id within its kind
};
// Both kinds share the id space: lodestones even (2k + 2), books odd (2k + 1).
Table<LodestoneTarget>& lodestones() {
    static Table<LodestoneTarget> t;
    return t;
}
Table<BookContent>& books() {
    static Table<BookContent> t;
    return t;
}

} // namespace

uint32_t addLodestoneTarget(const LodestoneTarget& target) {
    auto& t = lodestones();
    const std::lock_guard guard(t.lock);
    t.entries.push_back(target);
    return static_cast<uint32_t>(t.entries.size()) * 2;
}

std::optional<LodestoneTarget> lodestoneTarget(uint32_t id) {
    auto& t = lodestones();
    const std::lock_guard guard(t.lock);
    if (id == 0 || id % 2 != 0 || id / 2 > t.entries.size()) return std::nullopt;
    return t.entries[id / 2 - 1];
}

uint32_t addBook(BookContent book) {
    auto& t = books();
    const std::lock_guard guard(t.lock);
    t.entries.push_back(std::move(book));
    return static_cast<uint32_t>(t.entries.size()) * 2 - 1;
}

std::optional<BookContent> bookContent(uint32_t id) {
    auto& t = books();
    const std::lock_guard guard(t.lock);
    if (id % 2 != 1 || (id + 1) / 2 > t.entries.size()) return std::nullopt;
    return t.entries[(id + 1) / 2 - 1];
}

} // namespace mc::world
