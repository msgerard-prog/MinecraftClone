# ui — local rules
HUD (crosshair, hotbar, health), screens (inventory, crafting, pause), F3 debug
overlay, chat/command input. Depends on `core`, `rendering`, `gameplay`.

- Layout in vanilla's GUI-scale units (scaled pixels; GUI scale auto = largest that
  fits). Sprites from the GUI atlas, nearest filtering.
- UI reads gameplay state and issues actions; it never changes world/gameplay state
  directly (go through gameplay APIs so ticks stay deterministic).
- Text rendering: one batched draw per frame; no per-glyph draw calls, no per-frame
  string allocation (format into fixed buffers).
- Each new screen gets a screenshot check.
