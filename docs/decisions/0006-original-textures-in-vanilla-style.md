# 0006. Original textures in vanilla's style, sharper than vanilla
- Status: Accepted (2026-10-06)
- Context: The repo can't contain Mojang's textures (ADR 0004), but the user wants the
  game to look like Minecraft without their own jar — "recreate, not copy" — and a
  little sharper, more vivid.
- Decision: Every texture is original pixel art made by our generator
  (`tools/textures/gen_placeholders.py`) following `docs/art-style.md`: vanilla's
  subjects, file names and 16x16 style; our own palettes, shapes and pixel layouts;
  crisp ramps, top-left light, bevels, more contrast than vanilla. Never traced,
  sampled or derived from Mojang files. Users can still override with their own pack.
- Alternatives: keep crude placeholders (repo looks unfinished); hand-drawn pixel art
  (slower to iterate, harder to keep consistent); higher resolution (changes the feel).
- Consequences: each new block's texture is designed when the block is added
  (`add-block` → `add-asset`). Style changes are made in one script and regenerate
  everything consistently.
