---
name: add-entity
description: Add a mob or other entity (physics, AI goals, spawning, model/rendering, tests). Use when asked to add a mob, projectile, item entity or any moving thing.
---
# Add an entity

> The entity system lands in M10. If `src/gameplay/entity/` doesn't exist, say so and
> stop. Update this skill when the real API differs.

1. Wiki page: hitbox width/height, eye height, health, speed (blocks/tick), armor,
   attack damage, AI goals with priorities, spawn rules (light level, biome, group
   size), drops, sounds.
2. Type + data in `src/gameplay/entity/<Name>.h/.cpp`: register its `EntityType` with
   dimensions and spawn group. Use pooled storage — no per-spawn heap churn.
3. Behaviour runs in `tick()` only: goals in vanilla priority order (e.g. zombie:
   float > attack > move-towards-target > wander > look). Keep previous/current
   position for interpolation.
4. Rendering in `src/rendering/entity/<Name>Renderer.*`: placeholder cuboid model
   first (our own textures, `add-asset`). Reads gameplay state, never modifies it.
5. Spawning rule in the spawner table; deterministic given the world RNG.
6. Tests `tests/gameplay_<name>.cpp`: hitbox, health/damage, one AI rule, spawn rule.
7. `tools/test.sh`; `visual-check` with the entity in view. Run `perf-reviewer` if it
   adds per-tick work for many entities. Commit.
