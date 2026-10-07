# 0002. Reference version: Java Edition 1.21.x
- Status: Accepted (2026-10-06)
- Context: Vanilla behaviour differs a lot between versions (world height, block
  states, worldgen).
- Decision: Replicate Java Edition 1.21.x. Fundamentals (Y −64..319, block states,
  paletted sections, 20 TPS) are 1.21 from day one; complex systems (multi-noise
  worldgen) are phased in, with temporary deviations listed in game-design.md.
- Alternatives: Beta 1.7.3 (much smaller), 1.12.2 (pre-flattening).
- Consequences: largest target; the wiki documents it best; worldgen is the hardest
  part and gets its own milestone (M8).
