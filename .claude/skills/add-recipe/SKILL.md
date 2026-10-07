---
name: add-recipe
description: Add crafting, smelting or other recipes in vanilla data-pack JSON format. Use when asked to add recipes or make an item craftable.
---
# Add a recipe

1. Check the item's wiki page › Crafting / Smelting for the exact pattern, ingredients
   (items or `#tags`), result count, and cook time / XP.
2. Write `assets/data/minecraft/recipe/<id>.json` in vanilla's format
   (`minecraft:crafting_shaped` with `pattern`/`key`/`result`, `crafting_shapeless`,
   `smelting`/`blasting`/`smoking` with `cookingtime` and `experience`). Our own file —
   written from the wiki, never copied from a jar.
3. New tags it needs → `assets/data/minecraft/tags/item/<tag>.json`.
4. Test in `tests/gameplay_crafting.cpp`: the grid matches (including mirrored shaped
   recipes, which vanilla allows), wrong grids don't, result count.
5. `tools/test.sh`. Commit `data: add <id> recipe`.
