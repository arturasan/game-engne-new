# 05 — Queries

A **query** is how a system asks the world for entities that match a component pattern. It is the primary input to almost every system.

## Anatomy

```cpp
Query<Transform, const Velocity>
//      ^^^^^^^^   ^^^^^^^^^^^^^^
//      mutable    read-only
```

The type list is **what components must be present**. Const-ness on each parameter says whether the system writes it (`T`) or only reads it (`const T`). The scheduler reads this to decide which systems can run in parallel.

## Iterating

```cpp
void move_things(Query<Transform, const Velocity> q, Res<Time> time) {
    // Range-based (common case):
    for (auto [entity, transform, velocity] : q) {
        transform.translation += velocity.linear * time->delta();
    }

    // Or the explicit form, when you want to early-return per-archetype:
    q.each([&](Entity, Transform& t, const Velocity& v) {
        t.translation += v.linear * time->delta();
    });
}
```

`each` and range-for are equivalent for simple cases. `each` is slightly faster (no iterator object), useful in hot paths.

## Filters

Bare component lists give "must have all of these". The full surface (planned through M2) adds filters:

| Filter | Meaning |
|---|---|
| `With<T>`       | must also have `T`, but don't fetch it (use for markers) |
| `Without<T>`    | must NOT have `T` |
| `Or<A, B>`      | one of the inner filters matches |
| `Added<T>`      | `T` was added since this system last ran (M2) |
| `Changed<T>`    | `T` was modified since this system last ran (M2) |

```cpp
// All transforms on entities tagged Player but not Dead.
Query<Transform, With<Player>, Without<Dead>>
```

In M1 only positional component lists (no filters) exist; `With`/`Without` are spec 0014. Plan your APIs around the full set so the migration is trivial.

## Random access

```cpp
auto q = ctx.query<Transform>();
if (auto* t = q.get(some_entity)) {
    t->translation = {0, 0, 0};
}
```

`q.get(entity)` is O(1) (a slot lookup + an archetype hop). Useful when one system needs to peek at a specific entity's components by handle.

## Why queries beat raw `World::get`

Inside a system you can write:

```cpp
// DON'T:
for (Entity e : enemies_resource->ids) {
    auto* t = world.get<Transform>(e);   // <-- hash lookup per entity
    auto* v = world.get<Velocity>(e);    // <-- another one
    if (t && v) t->translation += v->linear * dt;
}
```

vs.

```cpp
// DO:
for (auto [_, t, v] : ctx.query<Transform, const Velocity>()) {
    t.translation += v.linear * dt;
}
```

The query walks contiguous archetype columns. No hash lookups, no per-entity branches. The performance difference at 10k entities is roughly 50×.

## What queries do not do

- They do not spawn or despawn (use `Commands`).
- They do not add or remove components on the entities they iterate (use `Commands`).
- They do not survive across frames. A query is built per call; cheap because the world caches the matching archetype list.

## Parallel iteration (M2)

```cpp
q.par_each([](Entity, Transform& t, const Velocity& v) {
    t.translation += v.linear * fixed_dt;
});
```

`par_each` splits the matching archetypes across worker threads. Same body as `each`. Only safe because the query declared its access; the scheduler ensures no other parallel system writes the same components.

---

**Bevy mapping:** `engine::Query<...>` ↔ `bevy::Query<...>`. Const-vs-mut signals access here; Bevy uses `&T` vs `&mut T`. Same filter taxonomy (`With`, `Without`, `Or`, `Added`, `Changed`).
