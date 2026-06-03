# ECS — World, archetypes, queries

## Scope

The Entity-Component-System layer. Stores entities (handles) and their components (typed data) in **archetype tables** for cache-friendly iteration. Systems read/write components via **queries**.

This module has zero dependencies on rendering, platform, or assets.

## Public API (target shape)

```cpp
namespace engine {

// 64-bit POD handle: 32-bit slot id + 32-bit generation. Stable across moves.
struct Entity {
    std::uint32_t id;
    std::uint32_t gen;
    constexpr auto operator<=>(const Entity&) const = default;
};

class World {
public:
    // Entity lifecycle.
    Entity spawn();
    template <typename... Cs> Entity spawn(Cs&&... cs);   // spawn + bulk add
    bool   despawn(Entity e);
    bool   alive(Entity e) const;

    // Component manipulation.
    template <typename T> T*       get(Entity e);
    template <typename T> const T* get(Entity e) const;
    template <typename T> bool     has(Entity e) const;
    template <typename T, typename... Args> T& add(Entity e, Args&&... args);
    template <typename T> bool     remove(Entity e);

    // Resources (singleton typed state).
    template <typename R, typename... Args> R& insert_resource(Args&&... args);
    template <typename R> R*       resource();
    template <typename R> const R* resource() const;

    // Query API.
    template <typename... Cs> Query<Cs...> query();
};

// Query is a range over (Entity, Cs&...). Use mutable T for write, const T for read.
template <typename... Cs>
class Query {
public:
    auto begin();
    auto end();
    auto each(auto&& fn);                  // forwards (Entity, Cs&...) to fn
    std::size_t size() const;
};

}  // namespace engine
```

Example:

```cpp
World w;
Entity e = w.spawn(Transform{}, Velocity{ .linear = {1,0,0} });
w.query<Transform, const Velocity>().each(
    [dt](Entity, Transform& t, const Velocity& v) { t.translation += v.linear * dt; });
```

## Storage model

**Archetype tables.** All entities with the same set of component types share one table:

```
Archetype A = { Transform, Velocity }
    column<Transform>: [t0 t1 t2 ...]
    column<Velocity>:  [v0 v1 v2 ...]
    entities:          [e0 e1 e2 ...]
```

Iteration over `Query<Transform, Velocity>` walks each matching archetype's columns linearly. Cache-friendly, SIMD-amenable, very fast.

**Trade-off:** add/remove of a component moves the entity to a different archetype (allocate slot in new archetype, copy components, mark old slot empty). For high-churn components (e.g. `Hovered`), this is expensive. Address in M2 with an opt-in sparse-set storage class.

## Component identification

```cpp
namespace engine {
using ComponentId = std::uint32_t;

template <typename T>
ComponentId component_id_for() {
    static ComponentId id = next_component_id();
    return id;
}
}  // namespace engine
```

A monotonic counter, allocated lazily on first use. Stable within a process; not stable across processes (use type names or `typeid` for serialization).

Archetype identity = sorted set of `ComponentId`. Pre-computed hash for fast lookup.

## Archetype transitions

Each archetype keeps an **edge cache**:

```
struct Archetype {
    ...
    std::unordered_map<ComponentId, Archetype*> add_edges;
    std::unordered_map<ComponentId, Archetype*> remove_edges;
};
```

`world.add<Foo>(e, ...)` looks up `current_archetype.add_edges[id]`; on miss, computes the destination archetype and caches the edge. Subsequent identical operations are O(1).

## Entity slot reuse

```cpp
struct Slot {
    std::uint32_t archetype_idx;   // ~0u = dead
    std::uint32_t row;             // index within archetype
    std::uint32_t generation;      // bumped on despawn
};
std::vector<Slot> slots_;
std::vector<std::uint32_t> free_list_;
```

`spawn()` pops from `free_list_` if non-empty, else extends `slots_`. `despawn(e)` bumps generation, pushes onto `free_list_`. A stale `Entity{id, old_gen}` fails `alive()` cleanly.

## Queries

**Compile-time** filter resolution:

```cpp
template <typename... Cs>
auto query() {
    static const auto signature = std::array{ component_id_for<std::remove_cvref_t<Cs>>()... };
    // Match all archetypes whose component set is a superset of `signature`.
    auto matching = find_matching_archetypes_cached(signature);
    return Query<Cs...>(matching);
}
```

`Query`'s iterator holds a pointer-vector of `Archetype*` and per-archetype column pointers; advances within an archetype, then jumps to the next. Const-ness in the type signature (`const T` vs `T`) drives read/write access tracking (recorded in `SystemContext` per spec 0004).

Filters beyond positional components (`With<T>`, `Without<T>`, `Changed<T>`) are M2.

## Resources

Resources are singleton typed values living in `World`. Stored in a `std::unordered_map<std::type_index, std::unique_ptr<void, void(*)(void*)>>` with a type-erased deleter. Looked up by `std::type_index`. Insertion is once; subsequent `insert_resource` overwrites and asserts in debug.

## Change detection (M2, not M1)

Every component slot will carry `added: Tick`, `changed: Tick`. The World holds a monotonically-increasing global `Tick`. Systems mutating a component via `DerefMut`-equivalent bump `changed`. `Query<&T, Changed<T>>` filters by comparing against the system's `last_run_tick`.

Stubbed at the right layer in M1 so it can be added without API breakage.

## Determinism

Iteration order = (archetype-insertion order, row order within archetype). Both are insertion-deterministic, so given the same `spawn`/`despawn`/`add`/`remove` sequence, iteration is bit-identical across runs. **Do not** rely on entity ID equality across runs for save files — generations differ; use a serializable `Name` component instead.

## Decisions & alternatives

| Decision | Rationale | Rejected |
|---|---|---|
| Archetype storage default | Cache-friendly iteration, dominant case | Sparse-set default (faster churn, slower iteration) |
| `ComponentId` is runtime int, not `typeid` | Smaller, faster hash, denser arrays | `std::type_index` everywhere (works but heavier) |
| Resources separate from components | They have different lifetimes (process-wide vs per-entity) | Singleton archetype (Bevy 0.10 did this; messy) |
| Iteration order is insertion-deterministic | Free determinism win | Hash-order iteration (faster but non-deterministic) |

## Open questions

- How to handle component **add inside iteration**? Two options: defer to end of system (Bevy's `Commands`), or panic. Decision: defer (spec 0004 adds `Commands`).
- Memory layout of small components (1–4 bytes): pack into a single SoA column or separate? Decision: separate for now, profile in M2.
