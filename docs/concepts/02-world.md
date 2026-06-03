# 02 — World

`World` is the data store. Everything mutable that the game cares about lives in a `World`: entities, components, resources, event buffers.

A typical game has **two** worlds: the main world (gameplay) and the render world (per-frame snapshot used by the GPU pipeline). When unqualified, "the world" means the main world.

## What's in a World

```
World
├── entity slot table         — alive entities + generations
├── archetypes                — component data, grouped by component set
├── resources                 — typed singletons (Time, Input, AssetServer, …)
├── events                    — double-buffered typed message queues
└── command queue             — pending structural changes (see 06-commands.md)
```

## What you do with a World

From inside a system you almost never touch `World` directly. You receive `Query<...>`, `Res<T>`, `Commands` as typed parameters and let the scheduler ensure safety.

From **outside** a system — startup code, tests, plugins — you use `World` directly:

```cpp
engine::World w;

// Resources: typed singletons.
w.insert_resource<engine::Time>();
auto& time = w.resource<engine::Time>();

// Entities + components.
auto player = w.spawn(Transform{}, Velocity{ .linear = {1, 0, 0} });
w.add<Health>(player, Health{ .hp = 100 });

// Read.
if (auto* h = w.get<Health>(player)) {
    engine::log::info("player hp: {}", h->hp);
}

// Iterate.
w.query<Transform, const Velocity>().each(
    [dt = time.delta()](engine::Entity, Transform& t, const Velocity& v) {
        t.translation += v.linear * dt;
    });
```

## Direct mutation rules

From inside a running system **the world is locked**: you may read and write components your query declared, and you may read resources you declared, but you may not:

- spawn or despawn entities,
- add or remove components,
- insert or remove resources,
- run nested queries that overlap the running system's access.

All of those go through `Commands` (deferred — see `06-commands.md`).

The rule exists because the scheduler runs systems in parallel based on declared access. Mutating structure mid-iteration would invalidate other systems' assumptions.

## Determinism

Iteration order over a `World` is **insertion-deterministic**: given the same spawn/despawn/add/remove sequence, the same entities come out in the same order. This is what makes deterministic replay (spec 0009) work.

The corollary: **do not** rely on `Entity` id equality across runs. Entity ids are reused, generations differ. For save files, use a serialisable `Name` component or an explicit id table.

## Two worlds, one App

`App` owns both:

- `world()` returns the main world.
- `render_world()` returns the render world (used by extract systems and `engine/render/`).

Plugins almost always touch only the main world. The render world is engine-internal; sprite and PBR plugins schedule extract systems into it.

The two-world split is what makes `simulation frame N+1` able to run while `render frame N` is still on the GPU (planned for M3 — see `12-rendering.md`).

---

**Bevy mapping:** `engine::World` ↔ `bevy::World`. The "no direct mutation from inside a system" rule and the main/render split are taken straight from Bevy 0.11+.
