# 07 — Resources

A **resource** is a singleton value stored on the `World`, keyed by its C++ type. One resource per type per world. If you need many of something, that's a component on entities, not a resource.

```cpp
struct Time {
    double  elapsed;
    double  delta;
    std::uint64_t frame_count;
};

struct ClearColor {
    engine::vec4 rgba;
};

struct Score {
    int value;
};
```

These are all resources. They have no link to any entity; they exist once for the duration of the run.

## Inserting and reading

From a plugin:

```cpp
struct ScorePlugin {
    void build(engine::App& app) const {
        app.insert_resource<Score>(Score{ .value = 0 });
    }
};
```

From a system:

```cpp
void show_score(Res<Score> score) {
    engine::log::info("score: {}", score->value);
}

void bump_score(ResMut<Score> score, EventReader<EnemyKilled> killed) {
    for (const auto& _ : killed.read()) score->value += 10;
}
```

- `Res<T>` is shared read access. Many systems can run in parallel with it.
- `ResMut<T>` is exclusive write access. No other system reading or writing `T` runs at the same time.

Pick the smallest access that does the job. The scheduler reads it to maximise parallelism.

## Resources vs components

The line is sharp:

|                      | Resource           | Component                |
|----------------------|--------------------|--------------------------|
| Lifetime             | the run            | the entity               |
| Count                | exactly one        | zero or one per entity   |
| Accessed by          | type               | entity + type            |
| Iteration            | n/a                | via `Query`              |
| Example              | `Time`, `Input`    | `Transform`, `Health`    |

When in doubt: if you would naturally write "the X" (the time, the score, the asset server), it's a resource. If you would write "a Y" or "this Y" (a transform, an inventory), it's a component.

## Mutation rules

A resource is just data. Inside a system, you mutate through `ResMut<T>`:

```cpp
void tick_time(ResMut<Time> time) {
    time->frame_count += 1;
}
```

From outside a system (tests, plugin builders, startup):

```cpp
auto& t = world.resource<Time>();
t.frame_count = 0;
```

Resources do **not** go through `Commands`. They are not part of entity structure; mutating their fields is no different from mutating a local variable, as long as the access is declared.

Inserting or removing a resource entirely (changing the world's structure) does go through `Commands`:

```cpp
void enable_replay(Commands cmd) {
    cmd.insert_resource(ReplayActive{});
}
```

## Common engine-supplied resources

Set up by `DefaultPlugins`:

- `Time` — elapsed seconds, delta, fixed delta, frame counter.
- `Input` — keyboard and mouse state, "just pressed" / "just released".
- `ClearColor` — render clear value.
- `AssetServer` — load handles by path.
- `Assets<T>` — per-type asset storage.
- `Events<T>` — per-event-type message queues.
- `MainWindow` — the OS window handle (opaque).

You can always grep `engine/**/default_plugins.cpp` for the authoritative list.

## Resources you write

Almost every game ends up with a small bag of resources:

```cpp
struct GameState {
    enum class Phase { Menu, Playing, Paused, GameOver };
    Phase phase = Phase::Menu;
};

struct LevelConfig {
    int  difficulty;
    bool tutorials_enabled;
};
```

Keep them small. A resource that has accumulated 20 fields is usually three resources in a trenchcoat.

---

**Bevy mapping:** `engine::Res<T>` / `ResMut<T>` ↔ `bevy::Res<T>` / `ResMut<T>`. Same insert-once-by-type semantics, same access-tracking. The "smart pointer" wrapper around the actual `T` is also the same.
