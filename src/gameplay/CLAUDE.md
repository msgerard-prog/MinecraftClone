# gameplay — local rules
Player, movement physics & collision, entities, items, inventory, crafting, game modes.
Depends on `core` and `world`; **no OpenGL**. Renderers for entities live in
`rendering`, reading gameplay state.

- All simulation runs in `tick()` at 20 TPS. Store previous + current position so the
  renderer can interpolate. Never read frame delta here.
- Movement/physics constants come from the wiki (Player › Movement, Entity › Motion):
  per-tick values, applied in vanilla's order (input → gravity → collide → friction).
- Collision is AABB vs block collision shapes, axis by axis (Y first, then X, Z),
  matching vanilla's step-up and sneaking-edge behaviour.
- Entities: no `new`/`delete` per spawn in hot paths — pooled storage (M10).
- Use the `add-entity`, `add-item`, `add-recipe` skills.
