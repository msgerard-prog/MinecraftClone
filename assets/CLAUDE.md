# assets — local rules
Runtime files read from the source tree via `MC_ASSETS_DIR` (no copy step).

- `shaders/` — GLSL 4.60 core, `<name>.vert` + `<name>.frag` pairs.
- `minecraft/` — our placeholder **resource pack** in vanilla's layout
  (`blockstates/`, `models/block/`, `textures/block/`). See docs/data-formats.md.
- `data/minecraft/` — data-pack JSON (recipes, loot tables, tags).

Rules:
- **Only our own work** (ADR 0004). Never copy files from a vanilla jar or the
  `resourcepacks/` folder into here. Generate placeholders (tools script or by hand).
- Textures: 16×16 RGBA PNG, names match the vanilla id (`stone.png`, `oak_log_top.png`)
  so a real resource pack can override them file for file.
- JSON: 2-space indent, same keys as vanilla's formats.
- Use the `add-asset` skill for new textures and `add-shader` for shaders.
