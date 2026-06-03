# 09 — Schedules

A **schedule** is an ordered DAG of systems. Each frame, the `App` runs several schedules in a fixed sequence; each schedule runs its systems with as much parallelism as their declared access allows.

## The per-frame sequence

```
First       — frame setup (time, frame counter)
PreUpdate   — gather inputs, drain network
FixedMain   — runs 0..N times depending on accumulator and fixed_dt
Update      — main game logic
PostUpdate  — derived state: transform propagation, visibility, culling
Last        — end-of-frame bookkeeping
[Extract]   — engine-internal: snapshot main → render world
[Render]    — engine-internal: GPU work + present
```

Each label (`First`, `Update`, …) is a **tag type**, not a string. You drop a system into a label by passing the tag:

```cpp
app.add_system<engine::Update>(my_logic);
app.add_system<engine::PostUpdate>(propagate_transforms);
app.add_system<engine::FixedMain>(physics_step);
```

If you omit the tag, the system goes into `Update`. That's the right default; reach for the others only when you have a reason.

## Why so many labels?

To resolve ordering conflicts in one obvious place. Some examples:

- **Input must run before logic.** `PreUpdate` ensures `Input` is populated before any `Update` system reads it.
- **Logic must run before transforms propagate.** `PostUpdate` runs after `Update`, so parent/child transform chains are coherent before render.
- **Physics must run at a fixed rate.** `FixedMain` is the only place where `Time::fixed_delta()` is meaningful.

Pick the right label and you rarely need explicit per-system ordering.

## Fixed timestep

`FixedMain` runs zero or more times per frame, based on an accumulator:

```
accumulator += time.delta()
while accumulator >= fixed_dt:
    run FixedMain schedule
    accumulator -= fixed_dt
```

Default `fixed_dt = 1/60 s`. Suitable for deterministic physics, networked sim, and the replay harness (spec 0009).

Inside `FixedMain` you read `Time::fixed_delta()`, not `Time::delta()`. The latter is per-frame wall time and varies; the former is the engine's fixed step.

## Ordering within a schedule

Two systems in the same schedule run in parallel if their declared access doesn't conflict. When you do need to order them:

```cpp
app.add_system<Update>(spawn_loot)
   .add_system<Update>(animate_loot, { .after = engine::label_of<spawn_loot> });
```

- `.after = { ... }` — set of system labels that must finish first.
- `.before = { ... }` — set of system labels that must run after this one.

For chains of three or more, prefer one tag system in the middle and put `.after` / `.before` on the others — the diff in a PR is easier to read.

## Run conditions

```cpp
app.add_system<Update>(award_score,
    { .when = [](const World& w){ return w.resource<GameState>().phase == GameState::Phase::Playing; } });
```

The system is skipped this frame if the predicate returns false. Common conditions live as helpers:

```cpp
.when = engine::resource_equals(GameState::Phase::Playing)
.when = engine::on_event<EnemyKilled>()       // run only on frames where the event fired
.when = engine::any_with<Player>()             // run only when at least one Player exists
```

Run conditions are cheap (called once per frame per system). Use them generously to keep frame cost flat in menus, pause states, etc.

## System sets (M2)

A **system set** is a named group you can target with `.after` / `.before` / `.when` as if it were a single system:

```cpp
struct PhysicsSet {};
app.configure_set<PhysicsSet>({ .when = engine::resource_equals(GameState::Phase::Playing) });

app.add_system<Update>(integrate_velocity, { .in_set = engine::set<PhysicsSet> });
app.add_system<Update>(resolve_collisions, { .in_set = engine::set<PhysicsSet>, .after = engine::label_of<integrate_velocity> });
```

In M1, you achieve the same with explicit `.after` lists and shared `.when` predicates. Sets are sugar; the underlying ordering is the same.

## Startup schedule

Code that runs once, before the first `First` of the first frame:

```cpp
app.add_system<engine::Startup>(spawn_initial_world);
```

This is the right place to spawn the camera, load the first scene, register the player. Startup systems can use `world.spawn` directly — no Commands needed.

## What the scheduler actually does

On the first `App::run()`, the scheduler:

1. For each schedule, collects every system's declared access.
2. Builds a conflict graph: two systems conflict if either writes a component or resource the other reads or writes.
3. Computes a topological order respecting `.after` / `.before`.
4. For M1: runs systems sequentially in topological order.
5. For M2+: dispatches independent systems to worker threads, respecting the conflict graph.

You can dump the graph for debugging once tooling lands (M3).

---

**Bevy mapping:** schedules and labels ↔ Bevy's `Schedule` + built-in labels (`First`, `PreUpdate`, etc.). `FixedMain` ↔ `FixedUpdate`. `Startup` ↔ `Startup`. System ordering with `.after` / `.before` is the same. System sets are `SystemSet` in Bevy.
