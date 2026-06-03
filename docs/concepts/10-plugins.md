# 10 — Plugins

A **plugin** is anything with a single method:

```cpp
void build(engine::App& app) const;
```

That's the whole contract. A plugin is not a base class. It is whatever C++ type satisfies the `Plugin` concept:

```cpp
template <typename T>
concept Plugin = requires(T t, App& a) {
    { t.build(a) } -> std::same_as<void>;
};
```

Plugins are how you compose an engine. `DefaultPlugins` is itself a plugin that calls `add_plugin` on a bag of others. Your game is a plugin you add on top.

## Shapes

```cpp
// 1) Stateless tag plugin.
struct LogPlugin {
    void build(App& app) const {
        app.insert_resource<LogConfig>({})
           .add_system<First>(flush_log_buffers);
    }
};

// 2) Plugin with config knobs (preferred when config exists).
struct WindowPlugin {
    std::string title  = "engine";
    int         width  = 1280;
    int         height = 720;
    bool        vsync  = true;

    void build(App& app) const {
        app.insert_resource<WindowConfig>({
            .title = title, .width = width, .height = height, .vsync = vsync });
        app.add_system<First>(pump_window_events);
    }
};

// Usage:
app.add_plugin(WindowPlugin{ .title = "My Game", .width = 1920, .height = 1080 });

// 3) Plugin group (composition).
struct DefaultPlugins {
    void build(App& app) const {
        app.add_plugins(
            LogPlugin{},
            TimePlugin{},
            WindowPlugin{},
            InputPlugin{},
            AssetPlugin{},
            RenderPlugin{},
            Render2dPlugin{},
            Render3dPlugin{},
            AudioPlugin{}
        );
    }
};
```

## What a plugin can do in `build()`

Anything `App` exposes:

- `insert_resource<T>(...)` — set up a singleton.
- `add_event<E>()` — register an event type.
- `add_system<Schedule>(fn, cfg)` — schedule a system.
- `add_plugin(...)` / `add_plugins(...)` — compose with other plugins.

What it **cannot** do:

- Run systems immediately. `build` is called during `App` construction, before the world is "alive". You're declaring, not executing.
- Spawn entities (you can, but most use cases want a `Startup` system instead, so the spawn is part of the schedule).
- Open windows or load assets. Those are systems; they run in the loop.

## When to write a plugin

Almost always. The smallest game has at least one — its own. Reach for a plugin whenever you have a coherent feature: an input scheme, an audio system, a UI screen, a debug overlay.

Rule of thumb: **one feature, one plugin, one folder**. If you can describe a feature in a sentence ("the inventory system", "the dialogue runner"), it's a plugin.

## Plugin groups

A plugin group is just a plugin whose `build` calls `add_plugins`. There's no special "group" type. `DefaultPlugins` above is a group; so is anything you write.

When you want a group with options:

```cpp
struct DefaultPlugins {
    bool headless = false;
    bool audio    = true;

    void build(App& app) const {
        app.add_plugin(LogPlugin{});
        app.add_plugin(TimePlugin{});
        if (!headless) {
            app.add_plugin(WindowPlugin{});
            app.add_plugin(InputPlugin{});
            app.add_plugin(RenderPlugin{});
        }
        if (audio) app.add_plugin(AudioPlugin{});
    }
};

// Usage in CI / tests:
engine::App{}.add_plugin(DefaultPlugins{ .headless = true, .audio = false }).run();
```

## Plugin ordering

Plugins build in the order you add them. Most of the time the order doesn't matter, because plugins only **declare** (insert resources, add systems) — actual execution order is the schedule's job, not the plugin's.

When it does matter: a plugin that depends on a resource set up by another plugin must be added after it. If you accidentally add them in the wrong order, you'll see an assertion at `prepare()` time (the resource is missing). Plugin order is a one-line fix; this is on purpose.

## What plugins replace

In a classical OO engine you'd have an `Engine` class with member subsystems (`engine.audio`, `engine.physics`, `engine.input`, …). Adding a feature means editing `Engine` and recompiling the world.

Here, those subsystems are plugins; the "engine" is what you compose. The same machinery powers first-party engine features and third-party game-specific features.

## Hot reload (M3 preview)

Plugins are also the unit of hot reload. A plugin compiled into a shared library can be unloaded, recompiled, and reloaded mid-run via `cr.h`. Game code lives in plugins precisely because we want this. The engine core does not hot-reload.

---

**Bevy mapping:** `engine::Plugin` concept ↔ `bevy::Plugin` trait. Composition via `add_plugin` / `add_plugins` is identical. `DefaultPlugins` is the same convention. The single difference: Bevy's `Plugin` is a trait with default-implementable hooks (`ready()`, `cleanup()`, `name()`); we keep just `build()` for now and add hooks if a real need shows up.
