# 03 — Entities and components

## Entity

An `Entity` is a **64-bit POD handle**, nothing more:

```cpp
struct Entity {
    std::uint32_t id;
    std::uint32_t gen;
};
```

An entity is not an object. It does not "have" methods. It is a key into the world's tables. If you despawn an entity, the slot is reused and the generation is bumped — old handles fail `world.alive(e)` cleanly.

Two entities are equal if both fields are equal. `operator<=>` is defaulted; entities are usable in `std::map`, `std::set`, hash tables.

## Component

A **component** is a piece of data attached to an entity. It is a plain C++ type:

```cpp
struct Transform {
    engine::vec3 translation;
    engine::quat rotation;
    engine::vec3 scale = {1, 1, 1};
};

struct Velocity {
    engine::vec3 linear;
};

struct Health {
    int hp;
    int max_hp;
};
```

Rules:

1. **No inheritance.** A component is a value, not a base class.
2. **No virtuals.** Logic does not live on a component.
3. **Trivially relocatable when possible.** The ECS may move components when archetypes change. Standard `Trivially*` types are fastest; types with non-trivial move are fine but slower.
4. **One instance per type per entity.** If you need many of the same kind, wrap them in a container component or model them as separate child entities.

The component type itself **is** its identifier. You never give a component a name string. `world.get<Transform>(e)` is how you ask "does this entity have a Transform".

## Spawning

```cpp
// Empty entity.
auto e = world.spawn();

// Spawn with components in one call (preferred).
auto player = world.spawn(
    Transform{ .translation = {0, 0, 0} },
    Velocity{},
    Health{ .hp = 100, .max_hp = 100 }
);
```

Spawning with components in one call is faster than `spawn()` + `add<T>` chains: the entity lands in its final archetype directly.

## Adding and removing later

```cpp
world.add<Stunned>(player, Stunned{ .ticks_remaining = 30 });
world.remove<Stunned>(player);
```

Each add/remove moves the entity to a different archetype (allocate slot, copy columns, mark old slot empty). Inexpensive for occasional changes; **expensive** if you flip a tag every frame on thousands of entities. For high-churn flags, look at the M2 sparse-set opt-in.

## Bundles

A **bundle** is just a tuple of components you tend to spawn together. We don't have a `Bundle` trait — `spawn(...)` takes a pack of components. Convention:

```cpp
struct SpriteBundle {
    Sprite          sprite;
    Transform       transform;
    GlobalTransform global_transform;
    Visibility      visibility;
};

// Usage:
SpriteBundle bundle{
    .sprite = Sprite{ .texture = handle, .size = {64, 64}, .color = {1,1,1,1} },
};
auto e = world.spawn(bundle.sprite, bundle.transform,
                     bundle.global_transform, bundle.visibility);
```

When P2996 reflection ships, `spawn(bundle)` will auto-unpack any aggregate. Until then, the pattern above is the workaround. Helper `engine::spawn_bundle(world, bundle)` may appear; see the current ECS spec.

## Marker components

A component with no data is a **marker**:

```cpp
struct Player {};
struct MainCamera {};
```

Markers tag entities for queries: `world.query<Player, Transform>()` iterates exactly the entities that are players.

## What goes in a component, what doesn't

| In a component | Not in a component |
|---|---|
| Position, velocity, health, sprite handle | The sprite's render pipeline |
| AI state, blackboard | The AI algorithm |
| Inventory slot count | Inventory mutation logic |
| References (handles) to assets | The assets themselves |

If you would write a method on a class, write a system instead.

---

**Bevy mapping:** `engine::Entity` ↔ `bevy::Entity`, components ↔ Bevy components. `spawn(a, b, c)` ↔ Bevy's `spawn((a, b, c))` (tuple). Bundles are a convention here, a trait in Bevy.
