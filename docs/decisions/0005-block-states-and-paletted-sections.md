# 0005. Dense global block-state ids and paletted sections
- Status: Accepted (2026-10-06)
- Context: Chunks store 98,304 blocks each; meshing, lighting and physics read block
  properties in tight loops. Vanilla 1.13+ models blocks as *states* and stores
  sections as a palette + packed indices.
- Decision: Every (block, property values) combination gets a dense `uint16`
  `BlockStateId` at startup (air = 0). Hot flags (`opaqueCube`, later light opacity)
  live in flat per-state arrays. Sections store states as vanilla does: a palette and
  a packed `uint64` array (entries never span two longs), single-value sections use
  0 bits, and 4–8 bit linear palettes grow to a direct 15-bit global palette.
- Alternatives: per-block id + metadata nibble (pre-1.13 model; can't express
  e.g. 6-way facing + waterlogged); a plain `uint16[4096]` per section (8 KiB each,
  simple but 10–100x more memory for typical terrain and unlike vanilla).
- Consequences: very little memory per section and a direct model of vanilla's
  storage (useful for M7 saves). Reads cost a shift and mask; meshing copies sections
  into a flat padded array first so the inner loop stays simple.
