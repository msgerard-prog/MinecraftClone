---
name: add-block
description: Add a vanilla block (with its block states, model, texture and tests) to the block registry. Use when asked to add or implement any block, e.g. "add oak logs", "implement glass".
---
# Add a block

> The block registry lands in M2. If `src/world/Blocks.cpp` doesn't exist yet, stop and
> tell the user this needs M2. When the registry's real API differs from these steps,
> update this skill in the same commit.

1. **Look it up.** Read the block's minecraft.wiki page. Note: id, block-state
   properties and values, hardness, blast resistance, light emission, light
   filtering/opacity, collision shape, render type (solid / cutout / translucent),
   tool & drops, sound group, special behaviour (random ticks, gravity, ...).
2. **Register** in `src/world/Blocks.cpp` (+ `Blocks.h` handle) in the same place
   vanilla lists it. Properties via the shared `Property` objects (`axis`, `facing`,
   `waterlogged` ...) — reuse, don't redefine. Comment with the wiki page.
3. **Behaviour** beyond plain cubes: subclass/behaviour hook in `src/world/blocks/`.
   Logic is tick-based and deterministic.
4. **Resources** (our own, ADR 0004): run the `add-asset` skill for the texture(s),
   then add `assets/minecraft/blockstates/<id>.json` and
   `assets/minecraft/models/block/<id>.json` in vanilla's format (`cube_all`,
   `cube_column`, `cross` ...).
5. **Data**: loot table `assets/data/minecraft/loot_table/blocks/<id>.json` and tags
   (`mineable/pickaxe` ...) once M9 exists.
6. **Tests** in `tests/world_blocks.cpp`: state count = product of property sizes;
   default state; string round-trip `minecraft:<id>[...]`; any special rule.
7. `tools/test.sh`, then the `visual-check` skill showing the block (place it in the
   debug scene / at a known `--pos`). Look at the PNG.
8. Update `docs/data-formats.md` if a new property type or model kind was added.
   Commit: `world: add <block>`.
