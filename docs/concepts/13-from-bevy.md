# 13 — From Bevy

If you've used Bevy, this page is the only one you strictly need. The shapes match; the rules match; the names match where C++ allows.

## Direct translation

| Bevy (Rust)                                | This engine (C++26)                              |
|--------------------------------------------|--------------------------------------------------|
| `App::new()`                               | `engine::App{}`                                  |
| `app.add_plugins(DefaultPlugins)`          | `app.add_plugin(engine::DefaultPlugins{})`       |
| `app.add_systems(Update, my_system)`       | `app.add_system<engine::Update>(my_system)`      |
| `app.insert_resource(MyRes{...})`          | `app.insert_resource<MyRes>(...)`                |
| `app.add_event::<MyEvent>()`               | `app.add_event<MyEvent>()`                       |
| `app.run()`                                | `app.run()`                                      |
| `World`                                    | `engine::World`                                  |
| `Entity`                                   | `engine::Entity`                                 |
| `Commands`                                 | `engine::Commands`                               |
| `Query<&T>` / `Query<&mut T>`              | `Query<const T>` / `Query<T>`                    |
| `Res<T>` / `ResMut<T>`                     | `Res<T>` / `ResMut<T>`                           |
| `EventReader<E>` / `EventWriter<E>`        | `EventReader<E>` / `EventWriter<E>`              |
| `Local<T>`                                 | `Local<T>` (M2)                                  |
| `Handle<T>`                                | `Handle<T>`                                      |
| `Assets<T>` / `AssetServer`                | `Assets<T>` / `AssetServer`                      |
| `Bundle`                                   | aggregate struct, unpacked at spawn site (P2996 → reflection helper) |
| `Plugin` trait                             | `Plugin` concept (`void build(App&)`)            |
| `Schedule`, `SystemSet`                    | schedule tag types; `SystemSet` (M2)             |
| `FixedUpdate`                              | `engine::FixedMain`                              |
| `Startup`                                  | `engine::Startup`                                |
| `MainWorld` + `RenderApp`                  | `App::world()` + `App::render_world()`           |

## What's identical in spirit

- **Mental model.** App → World → Entities/Components/Resources, systems do work, plugins compose.
- **Schedule order.** `First`, `PreUpdate`, `FixedMain`, `Update`, `PostUpdate`, `Last`, then engine-internal extract + render.
- **Access rules.** Systems declare what they touch via parameter types; the scheduler parallelises based on conflicts.
- **Commands deferral.** Structural changes from inside a system queue up; flushed at safe points.
- **Events.** Per-reader cursors, double-buffered, dropped after two frames.
- **Assets.** `Handle<T>` decouples references from storage; async load with placeholders.
- **Render world.** Per-frame snapshot via extract systems; sort + batch in render phases.

## What's deliberately different

- **No `IntoSystem` magic.** Rust's trait-based system conversion has no clean C++ equivalent yet. Until P2996 reflection arrives in shipping compilers, M1 systems take `(World&, SystemContext&)`. The migration to `(Query<...>, Res<...>, ...)` syntax is mechanical when reflection lands; the system body doesn't change.
- **Schedule labels are tag types, not enum variants.** Compile-time-checked, IDE-discoverable, no string churn.
- **One canonical Plugin shape.** No `name()`, `ready()`, `cleanup()` defaults. Add them if a real use case appears.
- **No `Bundle` trait yet.** Spawn takes a component pack. Aggregate-bundle types are a convention until reflection.
- **Const-on-the-component, not mut-on-the-reference.** `Query<Transform, const Velocity>` ↔ `Query<&mut Transform, &Velocity>`. C++ doesn't have explicit `&mut`; we read const-qualification of the type parameter instead.
- **`Result<T>` for fallible operations, no exceptions across module boundaries.** Mirrors Rust `Result` for the same reason: predictable cost, ABI-safe.

## What's missing today (vs. Bevy 0.13+)

- Reflection (P2996) — components are not introspectable at runtime yet. Affects inspector, save/load. M3.
- States — Bevy's `States` machine. Useful but not core; write your own enum-typed resource for now.
- Sub-apps — Bevy supports multiple sub-apps (each with its own world + schedule). We have two (main, render); a general sub-app API arrives M3.
- Tracing/diagnostics — Tracy or equivalent profiler hookup. M3.

## What's missing forever (intentionally)

- A `World::despawn_recursive` that walks `Children` automatically without you opting in. You'll opt in.
- Implicit `From`/`Into` conversions on components. Components are POD; conversions are explicit functions.
- A global allocator for ECS storage. You can swap allocators per archetype family if you really need to.

## When you're stuck

Bevy intuition transfers. If you would solve a problem in Bevy with "an event + a system that reads it", do exactly that here — same names, same shapes. If a Bevy pattern looks awkward in C++, check the reference docs (`docs/architecture/`) and architecture decisions (`docs/adr/`) — there is usually a one-line difference that explains it.

---

**The whole engine in one sentence, again:** one App owns one World, World holds entities/components/resources, App runs systems on a schedule, systems read with Queries and mutate with Commands, everything else is plugins.
