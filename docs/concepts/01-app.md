# 01 — App

`App` is the program. It owns the `World`, the schedules, and the run loop. Every game built on the engine starts the same way:

```cpp
#include "engine/core/app.hpp"
#include "engine/default_plugins.hpp"
#include "my_game/game_plugin.hpp"

int main() {
    return engine::App{}
        .add_plugins(engine::DefaultPlugins{})
        .add_plugin(MyGame::GamePlugin{})
        .run();
}
```

Three things happen here:

1. An `App` is constructed (empty world, empty schedules).
2. Plugins **configure** the app — they add systems, insert resources, register event types.
3. `run()` enters the loop. It returns the process exit code when the app stops.

`App` is a **builder**. Every configuration method returns `App&`. You wire the whole game up before `run()`; you do not configure the app from inside a system.

## What App owns

- The main `World` (entities, components, resources, events).
- The render `World` (separate; populated per frame by extract systems).
- The schedules, keyed by tag type (`Update`, `PostUpdate`, …).
- The per-frame order of those schedules.
- Bookkeeping: frame counter, exit-requested flag.

## What App does not own

- The OS window. That belongs to `PlatformPlugin`.
- The GPU device. That belongs to `RenderPlugin`.
- The asset cache. That belongs to `AssetPlugin`.

This separation is intentional. An `App` with no plugins runs an empty world for one frame and exits. Useful for tests. Add `DefaultPlugins` and you get a real game shell.

## Lifecycle

```
App()                  // empty
  .add_plugin(...)     // plugins call back into the app to register their pieces
  .insert_resource(...)
  .run()               // calls prepare(), then loops
    prepare()          // resolves system access, builds schedule DAGs (once)
    loop:
      tick all schedules in order
      check exit_requested
    teardown()
```

`prepare()` is implicit on the first `run()`. After it has run, adding new systems is an error (M1 limitation; lifted in M3 when hot reload arrives).

## Stopping cleanly

Two ways:

```cpp
// 1) From any system, via the world:
world.resource<engine::AppControl>().request_exit();

// 2) For tests and examples — bounded run:
app.set_max_frames(60).run();
```

`run()` returns 0 on clean exit. Non-zero on a fatal error (asset load failure that cascaded, GPU device lost twice in a row, etc.).

## Minimal complete app

```cpp
struct HelloPlugin {
    void build(engine::App& app) const {
        app.add_system<engine::Update>([](engine::World& w, engine::SystemContext&) {
            engine::log::info("frame {}", w.resource<engine::Time>().frame_count);
        });
    }
};

int main() {
    return engine::App{}
        .add_plugin(engine::LogPlugin{})
        .add_plugin(engine::TimePlugin{})
        .add_plugin(HelloPlugin{})
        .set_max_frames(5)
        .run();
}
```

Runs 5 frames, logs each, exits 0. That's `examples/hello_window/` minus the window.

---

**Bevy mapping:** `engine::App` ↔ `bevy::App`. Fluent builder API. `run()` returns instead of looping forever (Bevy's `App::run()` also returns since 0.13, but most games never reach the line after it).
