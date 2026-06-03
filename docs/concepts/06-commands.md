# 06 — Commands

`Commands` is how a system **mutates the world's structure** — spawn, despawn, add or remove a component, insert a resource — without breaking the iteration that other parallel systems are doing.

## Why not just call `world.spawn` from inside a system?

Because:

- The system you're in declared which components it touches. `world.spawn(Foo{}, Bar{})` may add a new entity into archetypes another parallel system is iterating. That system's invariants would silently break.
- Spawns / removes invalidate component pointers and entity-slot mappings. A query mid-walk would see torn state.

So **direct structural mutation from inside a running system is forbidden**. The runtime asserts on it in debug.

What you do instead: enqueue a command. The world applies the queue at a **command flush point** — between systems, when nothing is iterating.

## The shape

```cpp
void cull_dead(Query<Entity, const Health> q, Commands cmd) {
    for (auto [e, h] : q) {
        if (h.hp <= 0) {
            cmd.despawn(e);
        }
    }
}

void spawn_loot(Query<const Transform, const LootDrop> q, Commands cmd) {
    for (auto [_, t, drop] : q) {
        cmd.spawn(
            Transform{ .translation = t.translation },
            Sprite{ .texture = drop.icon, .size = {32, 32} },
            Velocity{ .linear = {0, 1, 0} }
        );
    }
}

void apply_damage(Commands cmd, EventReader<DamageEvent> dmg) {
    for (const auto& d : dmg.read()) {
        cmd.entity(d.target).insert(Stunned{ .ticks = 30 });
    }
}
```

`Commands` is a tiny value (a pointer into a per-system command queue). You can request it on any system; doing so costs essentially nothing.

## What Commands can do

```cpp
cmd.spawn(a, b, c);                  // returns a deferred EntityRef
cmd.despawn(entity);
cmd.entity(e).insert(NewComponent{}); // add to existing entity
cmd.entity(e).remove<OldComponent>();
cmd.entity(e).despawn();              // same as despawn(e)
cmd.insert_resource(MyConfig{...});
cmd.remove_resource<MyConfig>();
cmd.queue([](World& w){ ... });       // arbitrary FnOnce escape hatch
```

The deferred `EntityRef` returned by `spawn` is **not** a real `Entity` yet — the entity is created at flush time. You can still chain `.insert` / `.remove` on it; those calls go into the same queue.

## When does the queue flush?

At well-defined points:

- After each schedule (e.g. between `Update` and `PostUpdate`).
- At `apply_deferred` boundaries inside a schedule, when you opt in by ordering.
- Before any system that depends on the new state — declare the dependency with `.after(apply_deferred)`.

You usually don't think about this. The common case "I despawned an enemy; the next system on the next schedule label sees it gone" just works.

If you need a flush mid-schedule — say a system spawns particles that a later same-frame system must process — order it explicitly:

```cpp
app.add_system<Update>(spawn_particles)
   .add_system<Update>(engine::apply_deferred, { .after = label_of<spawn_particles> })
   .add_system<Update>(animate_particles,      { .after = label_of<engine::apply_deferred> });
```

## When NOT to use Commands

- **Mutating an existing component's value.** That's just `transform.translation = ...` inside the query loop. No Commands needed.
- **Reading data.** Commands writes only. Use `Query` or `Res`.
- **Sending a message to another system.** Use events (see `08-events.md`).

The rule of thumb: if your change is "structure" (which entities exist, which components are on them), Commands. If it's "values on existing structure", direct write.

## What Commands cost

A few hundred nanoseconds per call (push into a typed ring buffer). The flush is a single tight loop that walks the queue. For most games this never shows up in profiles. For games that despawn 100k entities per frame: look at the bulk APIs (`cmd.despawn_batch(...)`) — see the relevant spec.

## A note on `world.spawn` outside systems

Direct `world.spawn(...)`, `world.add<...>`, etc. **are** allowed:

- in tests,
- in plugin `build(App&)` (which runs before the loop),
- in startup systems if you're sure no other system is mid-iteration (the schedule guarantees this for `Startup`).

Commands is required only inside running systems.

---

**Bevy mapping:** `engine::Commands` ↔ `bevy::Commands`, including the deferred-flush model. The `apply_deferred` ordering helper is the same concept as Bevy's `apply_deferred` system. C++ note: `Commands` is a value type holding a pointer to its per-system queue, not a reference.

**Spec status:** Commands is `specs/0013-commands-deferred-mutation.md` (planned). Resources and events ship first in spec 0004; Commands lands on top.
