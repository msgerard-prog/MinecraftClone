#include "world/RecipeIds.h"

#include <deque>
#include <mutex>
#include <unordered_map>

namespace mc::world {

namespace {

struct Table {
    std::mutex mutex;
    std::deque<std::string> names{std::string()}; // [0] = kNoRecipe
    std::unordered_map<std::string, RecipeId> ids;
};

Table& table() {
    static Table t;
    return t;
}

} // namespace

RecipeId internRecipeId(std::string_view id) {
    Table& t = table();
    std::lock_guard lock(t.mutex);
    std::string key(id);
    if (const auto it = t.ids.find(key); it != t.ids.end()) return it->second;
    if (t.names.size() > 0xFFFF) return kNoRecipe;
    const auto n = static_cast<RecipeId>(t.names.size());
    t.names.push_back(key);
    t.ids.emplace(std::move(key), n);
    return n;
}

std::string recipeIdName(RecipeId id) {
    Table& t = table();
    std::lock_guard lock(t.mutex);
    return id < t.names.size() ? t.names[id] : std::string();
}

} // namespace mc::world
