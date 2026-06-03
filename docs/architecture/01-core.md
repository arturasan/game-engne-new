# Core — App, Plugin, Schedule

## Scope

The bootstrap layer. Everything else in the engine depends on this; this depends on nothing in the engine.

Three concepts:

- **`App`** — builder + driver. Owns the `World`, the schedules, and the run loop.
- **`Plugin`** — concept (not a class). Anything that can configure an `App`.
- **`Schedule`** — a DAG of systems with run conditions and ordering constraints.

These map 1:1 to Bevy's `App`, `Plugin`, `Schedule`.

## Public API (target shape)

```cpp
namespace engine {

// Plugin: any type T with a `build(App&)` member.
template <typename T>
concept Plugin = requires(T t, App& a) {
    { t.build(a) } -> std::same_as<void>;
};

// A system is anything callable from a SystemContext (param introspection added in 0004).
using SystemFn = std::move_only_function<void(World&, SystemContext&)>;

// Schedules are identified by an enum-like tag type.
struct First       {};
struct PreUpdate   {};
struct FixedMain   {};
struct Update      {};
struct PostUpdate  {};
struct Last        {};

class App {
public:
    // Plugin composition.
    template <Plugin P> App& add_plugin(P plugin);
    template <Plugin... Ps> App& add_plugins(Ps... ps);  // fold; equivalent to chained add_plugin

    // System registration.
    template <typename ScheduleTag = Update>
    App& add_system(SystemFn fn, SystemConfig cfg = {});

    // Resources (singleton state).
    template <typename R, typename... Args> App& insert_resource(Args&&... args);
    template <typename R> R&       resource();
    template <typename R> const R& resource() const;

    // Events.
    template <typename E> App& add_event();

    // Lifecycle.
    App& set_max_frames(std::uint64_t n);   // primarily for tests/examples
    App& request_exit();
    [[nodiscard]] std::uint64_t frame() const noexcept;
    [[nodiscard]] World&        world() noexcept;

    // Run loop.
    int run();
};

}  // namespace engine
```

## Schedule order per frame

```
First       — frame setup, time advance
PreUpdate   — input gather, network receive
FixedMain   — fixed-timestep sim (0..N iterations); systems here see Time::fixed_delta()
Update      — main game logic (per-frame)
PostUpdate  — transform propagation, visibility culling
Last        — cleanup, frame counters
[Extract]   — copy to render world (engine-internal, between Last and Render)
[Render]    — record + submit GPU work
```

`FixedMain` runs `floor((accumulator + dt) / fixed_dt)` times per frame, default `fixed_dt = 1/60 s`. Accumulator is in `engine::Time`.

## Plugin pattern

A plugin is anything with `build(App&)`. Common shapes:

```cpp
// 1) Stateless tag plugin.
struct LogFramePlugin {
    void build(App& app) const {
        app.add_system([](World& w, SystemContext&) {
            log::info("frame {}", w.resource<Time>().frame_count);
        });
    }
};

// 2) Plugin with config knobs (preferred when there is config).
struct WindowPlugin {
    std::string title = "engine";
    int width = 1280, height = 720;
    void build(App& app) const;
};

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
            Render3dPlugin{}
        );
    }
};
```

## Internals (sketch)

`App` owns:

- `World world_`
- `World render_world_`
- `std::unordered_map<std::type_index, Schedule> schedules_`
- `ScheduleOrder main_order_`  — vector of `std::type_index` defining the per-frame sequence
- `std::uint64_t frame_count_`, `bool exit_requested_`

`Schedule` owns:

- `std::vector<SystemNode>` where `SystemNode = { SystemFn, SystemConfig, ResolvedAccess }`
- A pre-computed topological order (rebuilt only when systems are added)
- A parallel executor (added in spec 0004) that respects the conflict graph

`add_system` is **lazy**: it records the system but does not analyze access until `run()` first calls `prepare()`. This lets the user add systems and resources in any order.

## System parameters (pre-reflection path)

Until reflection is both available in the active Fedora toolchains and approved by an ADR, we hand-write the parameter introspection. A `SystemContext` carries opaque handles; concrete system functions pull resources/queries from it:

```cpp
app.add_system([](World& w, SystemContext& ctx) {
    auto& time = ctx.res<Time>();
    auto  q    = ctx.query<&Transform, &const Velocity>();   // mut Transform, read-only Velocity
    for (auto [transform, velocity] : q) {
        transform.translation += velocity.linear * time.delta();
    }
});
```

`ctx.res<T>()` and `ctx.query<...>()` are the canonical access points. They record what was accessed for the conflict graph (spec 0004 implements the bookkeeping; for the M1 single-threaded executor, recording is a no-op).

## Decisions & alternatives

| Decision | Rationale | Rejected alternative |
|---|---|---|
| `Plugin` is a concept, not a base class | Zero v-table cost, plain values, composable as tuples | Inheritance with `virtual build()` |
| Schedules keyed by **tag type** | Compile-time-checked, zero string churn, IDE-friendly | String labels (Bevy's old approach) |
| `App` owns both main and render worlds | Frame loop sequencing is centralized | Two `App`s as in Bevy 0.10+ (more flexible but more setup overhead for a small team) |
| Hand-written `SystemContext` until approved reflection support | Works today; one swap-out file if reflection becomes policy | Wait for reflection (blocks M1) |
| `std::move_only_function` for system storage | Allows lambdas with move-only captures; no SBO surprises | `std::function` (copy-required) |

## What is **not** in core

- The render-world extract step (lives in `engine/render/`)
- Hot-reload of plugins (M3; uses `cr.h` over the same `Plugin` concept)
- Async system execution (M3+)

## Open questions (resolve before M2)

- Should plugins have explicit `Plugin::ready()` / `Plugin::cleanup()` hooks like Bevy? Decision: defer — `App::run()` start/end systems cover ~95% of cases.
- Sub-apps (separate worlds with their own schedules) — needed for editor? Decision: probably yes at M3, not before.
