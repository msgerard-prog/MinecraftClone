---
name: add-block
description: Add a vanilla block (with its block states, model, texture and tests) to the block registry. Use when asked to add or implement any block, e.g. "add oak logs", "implement glass".
---
# Add a block

> When the registry's real API differs from these steps, update this skill in the
> same commit.

1. **Look it up.** Read the block's minecraft.wiki page. Note: id, block-state
   properties and values, hardness, blast resistance, light emission, light
   filtering/opacity, collision shape, render type (solid / cutout / translucent),
   tool & drops, sound group, special behaviour (random ticks, gravity, ...).
2. **Register**: add an entry to the `blocks::` enum in `src/world/Blocks.h` and a
   matching `r.add("<id>", {settings}, {{&properties::x, "default"}})` line in
   `buildVanillaBlocks()` (`src/world/Blocks.cpp`) at the same position (the `check`
   asserts the order). Settings: `hardness`, `resistance`, `lightEmission`,
   `opaqueCube` (false for glass, leaves, plants...), `layer`. New property? Add it to
   `namespace properties` (Blocks.h/.cpp) with vanilla's value order (booleans are
   `true, false`) — reuse existing ones (`axis`, `snowy`).
   Until the JSON model loader exists, also add the block's model to the C++ model
   table in `src/rendering/` (see docs/data-formats.md › Resource formats).
3. **Behaviour** beyond plain cubes: subclass/behaviour hook in `src/world/blocks/`.
   Logic is tick-based and deterministic.
4. **Resources** (our own, ADR 0004/0006): the block's textures already exist in
   `assets/minecraft/textures/block/` (every vanilla block texture was generated) —
   check the names; use `add-asset` only if one is missing or needs redesign,
   then add `assets/minecraft/blockstates/<id>.json` and
   `assets/minecraft/models/block/<id>.json` in vanilla's format (`cube_all`,
   `cube_column`, `cross` ...).
5. **Data**: loot table `assets/data/minecraft/loot_table/blocks/<id>.json` and tags
   (`mineable/pickaxe` ...) once M9 exists.
6. **Tests** in `tests/world_blocks.cpp`: state count and default state string
   (the round-trip test already covers every state); any special rule.
7. `tools/test.sh`, then the `visual-check` skill showing the block (place it in the
   debug scene / at a known `--pos`). Look at the PNG.
8. Update `docs/data-formats.md` if a new property type or model kind was added.
   Commit: `world: add <block>`.
