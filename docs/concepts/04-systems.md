# 04 — Systems

A **system** is a function that the scheduler calls each frame. That's the entire concept.

A system is not a class. It does not inherit from anything. Whether a function is a system is decided by what parameters it takes — the scheduler reads the parameter list and figures out what the system needs.

## The shape

A system takes typed parameters drawn from this set:

| Param | Meaning |
|---|---|
| `Query<T...>`        | iterate entities with components `T...` (read or mut by const-ness) |
| `Res<T>` / `ResMut<T>` | read or write the singleton resource of type `T` |
| `EventReader<E>`     | read events of type `E` since this system last ran |
| `EventWriter<E>`     | send events of type `E` |
| `Commands`           | deferred world mutation (see `06-commands.md`) |
| `Local<T>`           | system-local state, persists across frames (M2) |

A system returns `void`. Errors that need handling go through `Result<T>` on a function the system calls, not as a return.

## Today's syntax (M1)

C++26 reflection is not in shipping toolchains yet, so the M1 system signature is hand-rolled:

```cpp
app.add_system<engine::Update>(
    [](engine::World& w, engine::SystemContext& ctx) {
        auto  dt   = ctx.res<engine::Time>().delta();
        auto  q    = ctx.query<Transform, const Velocity>();
        for (auto [_, t, v] : q) {
            t.translation += v.linear * dt;
        }
    });
```

The `SystemContext&` is the access-tracking handle. You pull queries and resources off it; it records what you accessed so the scheduler (M2) can parallelise correctly.

## Tomorrow's syntax (M2/M3)

When reflection or hand-written deduction guides are in place, the same system becomes:

```cpp
app.add_system<engine::Update>(
    [](Query<Transform, const Velocity> q, Res<engine::Time> time) {
        for (auto [_, t, v] : q) {
            t.translation += v.linear * time->delta();
        }
    });
```

The shape mirrors Bevy. The migration is a single helper rewrite; system bodies don't change.

## Where systems run

By default a system is added to `Update`:

```cpp
app.add_system(my_system);                            // → Update
app.add_system<engine::PostUpdate>(propagate);        // explicit label
app.add_system<engine::FixedMain>(physics_step);      // fixed-timestep
```

See `09-schedules.md` for the per-frame order.

## Ordering, run conditions, sets

```cpp
app.add_system<engine::Update>(my_system,
    engine::SystemConfig{
        .after  = { engine::label_of<other_system> },
        .before = { engine::label_of<third_system> },
        .when   = [](const World& w){ return w.resource<GameState>().paused == false; },
    });
```

- `.after` / `.before` — explicit edges in the schedule DAG.
- `.when` — run condition. The system is skipped this frame if it returns false.
- System sets (named groups) land in M2; for M1 use explicit ordering.

## The three rules

1. **A system declares everything it touches.** No hidden globals. No stashing a `World*` somewhere.
2. **A system does not mutate world structure directly.** Use `Commands`.
3. **A system is short.** If your system reads like a novel, it should be three systems chained with `.after`.

## Examples by shape

```cpp
// Pure read.
void log_player_pos(Query<const Transform, const Player> q) {
    for (auto [e, t, _] : q) log::info("player at {},{},{}",
        t.translation.x, t.translation.y, t.translation.z);
}

// Read + write.
void apply_velocity(Query<Transform, const Velocity> q, Res<Time> time) {
    for (auto [_, t, v] : q) t.translation += v.linear * time->delta();
}

// Structural change via Commands.
void cull_dead(Query<Entity, const Health> q, Commands cmd) {
    for (auto [e, h] : q) if (h.hp <= 0) cmd.despawn(e);
}

// Event reader.
void on_input(EventReader<KeyEvent> events, ResMut<PlayerState> state) {
    for (const auto& e : events.read()) {
        if (e.code == Key::Space && e.pressed) state->jumping = true;
    }
}
```

---

**Bevy mapping:** `engine::System` is the same idea, same param taxonomy (`Query`, `Res`, `ResMut`, `EventReader/Writer`, `Commands`, `Local`). C++ has no `IntoSystem` trait; we hand-wrap until reflection lands.
