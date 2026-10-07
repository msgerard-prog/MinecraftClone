#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mc::world {

// Recipe ids ("minecraft:iron_ingot_from_smelting_raw_iron", wiki: Recipe) as small
// numbers, so block entities can count recipe uses without strings (a furnace's
// RecipesUsed). Ids are interned on first use and never removed; ids we don't have
// recipes for (from vanilla worlds) are kept too, so they save back unchanged.
// Thread-safe (chunk loading interns on worker threads).
using RecipeId = uint16_t;
inline constexpr RecipeId kNoRecipe = 0;

// The id's number (allocated on first use); kNoRecipe once 65535 ids exist.
RecipeId internRecipeId(std::string_view id);
// The id's text ("" for kNoRecipe or unknown numbers).
std::string recipeIdName(RecipeId id);

} // namespace mc::world
