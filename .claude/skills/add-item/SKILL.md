---
name: add-item
description: Add a vanilla item (tool, food, material, block item) with stack size, durability, texture and tests. Use when asked to add any item.
---
# Add an item

> Items land in M9. If `src/gameplay/Items.cpp` doesn't exist yet, say so. Update this
> skill when the real API differs.

1. Read the item's minecraft.wiki page: id, max stack (1/16/64), durability, rarity,
   tool tier & mining speed, attack damage/speed, food values, fuel time.
2. Register in `src/gameplay/Items.cpp` in vanilla's order. Block items are created
   from the block registry automatically — don't hand-register them.
3. Tools: tier data (`wood`, `stone`, `iron`, `diamond`, `netherite`, `gold`) lives
   in one table; reuse it. Mining speed rules use block tags (`mineable/*`).
4. Texture via the `add-asset` skill → `assets/minecraft/textures/item/<id>.png`;
   model `assets/minecraft/models/item/<id>.json` (`item/generated` parent).
5. Tests in `tests/gameplay_items.cpp`: stack size, durability, mining speed vs a
   block, any special use behaviour.
6. `tools/test.sh`; screenshot of the item in the hotbar once UI exists. Commit.
