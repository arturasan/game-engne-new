# 0001 — ECS archetype storage

- Owner: TBD
- Status: draft
- Tracking issue: TBD

## Scope

Implement the first cut of the ECS storage layer in `engine/ecs/`. Components are stored in **archetypes** (struct-of-arrays groups keyed by a sorted set of component type IDs). Entities are generational handles. Add/remove of a component moves an entity between archetypes via an edge graph.

## Acceptance criteria

- [ ] `engine::World` exposes: `spawn()`, `despawn(Entity)`, `add<T>(Entity, T)`, `remove<T>(Entity)`, `get<T>(Entity) -> T*`, `has<T>(Entity) -> bool`.
- [ ] `engine::Entity` is a 64-bit POD: `{uint32 id, uint32 generation}`. Reusing a freed slot bumps the generation; stale handles fail `has()` cleanly.
- [ ] Components are stored in **archetype tables** (one contiguous `std::vector<T>` per component column per archetype).
- [ ] Archetype transitions on add/remove are O(component-count), not O(entity-count). Use an edge cache on each archetype.
- [ ] A trivial `for_each<A, B>(World&, fn)` iterator visits every entity that has both `A` and `B`, with no per-archetype hash lookup in the inner loop.
- [ ] Unit tests in `tests/unit/test_ecs.cpp` cover: spawn/despawn, generational invalidation, add+remove round-trip, iteration over 100k entities across 3+ archetypes.
- [ ] All tests tagged `[fast]`; full suite runs in under 100ms on the `linux-clang-asan` preset.

## Out of scope

- Multithreaded scheduling / parallel system execution.
- Change detection (`Changed<T>`, ticks).
- Sparse-set storage opt-in.
- Resources (singleton state) — separate spec.
- Events — separate spec.
- Queries with filters (`With<T>`, `Without<T>`) beyond positional component lists.
- Reflection / serialization of components.

## Files not to touch

- `engine/core/*` — App, Plugin, Schedule already exist and stay as-is.
- `engine/platform/*`, `engine/render*` — separate concerns.
- `tests/unit/test_app.cpp` — existing test must keep passing.

## Notes for the implementing agent

- Read `docs/adr/0001-cpp23-baseline.md`. Use C++23 features (`std::expected`, deducing-this) freely; do not reach for C++26.
- Component identification: use `std::type_index` or a custom `component_id_for<T>()` returning a process-stable integer. Pick one and document the choice in a follow-up ADR.
- Bevy's `bevy_ecs::archetype` is the reference shape — translate, don't copy.
- Do not introduce EnTT or flecs as a dependency. We are building this on purpose.
- Keep the public API in `engine/ecs/world.hpp` minimal. Internals (archetype, table, edge graph) live in `engine/ecs/detail/`.
