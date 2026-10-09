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
// The kinds share the id space: id = index x 4 + kind (books 1, lodestones 2, banners 3,
// fireworks 0 - at (index + 1) x 4, so 0 stays "none").
Table<LodestoneTarget>& lodestones() {
    static Table<LodestoneTarget> t;
    return t;
}
Table<BookContent>& books() {
    static Table<BookContent> t;
    return t;
}
Table<std::string>& names() {
    static Table<std::string> t;
    return t;
}
Table<BannerLayers>& banners() {
    static Table<BannerLayers> t;
    return t;
}
Table<Fireworks>& fireworkTable() {
    static Table<Fireworks> t;
    return t;
}

} // namespace

uint32_t addLodestoneTarget(const LodestoneTarget& target) {
    auto& t = lodestones();
    const std::lock_guard guard(t.lock);
    // Equal targets share an entry, so reloading a chunk doesn't grow the table (M28 review).
    for (size_t i = 0; i < t.entries.size(); ++i)
        if (t.entries[i] == target) return static_cast<uint32_t>(i) * 4 + 2;
    t.entries.push_back(target);
    return static_cast<uint32_t>(t.entries.size() - 1) * 4 + 2;
}

std::optional<LodestoneTarget> lodestoneTarget(uint32_t id) {
    auto& t = lodestones();
    const std::lock_guard guard(t.lock);
    if (id % 4 != 2 || id / 4 >= t.entries.size()) return std::nullopt;
    return t.entries[id / 4];
}

uint32_t addBook(BookContent book) {
    auto& t = books();
    const std::lock_guard guard(t.lock);
    for (size_t i = 0; i < t.entries.size(); ++i) // (equal books share an entry)
        if (t.entries[i] == book) return static_cast<uint32_t>(i) * 4 + 1;
    t.entries.push_back(std::move(book));
    return static_cast<uint32_t>(t.entries.size() - 1) * 4 + 1;
}

std::optional<BookContent> bookContent(uint32_t id) {
    auto& t = books();
    const std::lock_guard guard(t.lock);
    if (id % 4 != 1 || id / 4 >= t.entries.size()) return std::nullopt;
    return t.entries[id / 4];
}

uint32_t addBannerLayers(const BannerLayers& layers) {
    if (layers.count == 0) return 0;
    auto& t = banners();
    const std::lock_guard guard(t.lock);
    for (size_t i = 0; i < t.entries.size(); ++i)
        if (t.entries[i] == layers) return static_cast<uint32_t>(i) * 4 + 3;
    t.entries.push_back(layers);
    return static_cast<uint32_t>(t.entries.size() - 1) * 4 + 3;
}

std::optional<BannerLayers> bannerLayers(uint32_t id) {
    auto& t = banners();
    const std::lock_guard guard(t.lock);
    if (id % 4 != 3 || id / 4 >= t.entries.size()) return std::nullopt;
    return t.entries[id / 4];
}

uint32_t addFireworks(const Fireworks& f) {
    auto& t = fireworkTable();
    const std::lock_guard guard(t.lock);
    for (size_t i = 0; i < t.entries.size(); ++i)
        if (t.entries[i] == f) return static_cast<uint32_t>(i + 1) * 4;
    t.entries.push_back(f);
    return static_cast<uint32_t>(t.entries.size()) * 4;
}

std::optional<Fireworks> fireworks(uint32_t id) {
    auto& t = fireworkTable();
    const std::lock_guard guard(t.lock);
    if (id == 0 || id % 4 != 0 || id / 4 > t.entries.size()) return std::nullopt;
    return t.entries[id / 4 - 1];
}

uint32_t addName(std::string_view text) {
    if (text.empty()) return 0;
    auto& t = names();
    const std::lock_guard guard(t.lock);
    for (size_t i = 0; i < t.entries.size(); ++i)
        if (t.entries[i] == text) return uint32_t(i + 1);
    t.entries.emplace_back(text.substr(0, 50)); // (vanilla: 50 characters at most)
    return uint32_t(t.entries.size());
}

std::string_view nameText(uint32_t id) {
    auto& t = names();
    const std::lock_guard guard(t.lock);
    if (id == 0 || id > t.entries.size()) return {};
    return t.entries[id - 1]; // (deque entries never move)
}

} // namespace mc::world
