# 0004. No Mojang code or assets in the repo
- Status: Accepted (2026-10-06)
- Context: The project replicates vanilla behaviour; Mojang's code, textures and sounds
  are copyrighted even for non-commercial use.
- Decision: Behaviour is reimplemented from the wiki and observation, never from
  decompiled source. All committed assets are our own. The game can load a resource
  pack at runtime from the git-ignored `resourcepacks/` folder (the user's own copy).
- Alternatives: commit vanilla textures (not ours to redistribute).
- Consequences: placeholder art in the repo; visual parity checks use the user's local
  pack. `Read(resourcepacks/**)` is denied to Claude to avoid copying from it.
