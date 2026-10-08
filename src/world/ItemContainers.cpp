// The item contents table (M23.6, see the header).
#include "world/ItemContainers.h"

#include <deque>
#include <mutex>

namespace mc::world {

namespace {

struct Table {
    std::mutex lock;
    std::deque<ItemContents> entries; // index + 1 = id; a deque keeps references stable
};
Table& table() {
    static Table t;
    return t;
}

} // namespace

uint32_t addItemContents(const ItemContents& slots) {
    bool any = false;
    for (const ItemStack& s : slots)
        any = any || !s.empty();
    if (!any) return 0;
    Table& t = table();
    const std::lock_guard guard(t.lock);
    t.entries.push_back(slots);
    return static_cast<uint32_t>(t.entries.size());
}

ItemContents itemContents(uint32_t id) {
    Table& t = table();
    const std::lock_guard guard(t.lock);
    if (id == 0 || id > t.entries.size()) return {};
    return t.entries[id - 1];
}

} // namespace mc::world
