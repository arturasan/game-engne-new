# 00 — The mental model

A game built on this engine is, at the core:

> One **App** that owns one **World**. The World holds **entities** (handles), their **components** (data on those entities), and **resources** (singleton data). The App runs **systems** (plain functions) on a schedule. Systems read and write components via **queries**, and mutate the world structure via **commands**. Everything else is plugins.

That sentence is the whole engine. The rest of these docs unpack each bolded word.

## Picture it as a loop

```
┌────────────────────────────────────────────────────────────┐
│  App::run()                                                │
│    each frame:                                             │
│      First       → time advance, frame counter             │
│      PreUpdate   → input, network in                       │
│      FixedMain   → physics, deterministic sim (0..N times) │
│      Update      → game logic                              │
│      PostUpdate  → transforms, visibility, cleanup         │
│      Last        → end-of-frame bookkeeping                │
│      [Extract]   → snapshot main → render world            │
│      [Render]    → GPU work + present                      │
└────────────────────────────────────────────────────────────┘
```

The schedule labels (`First`, `Update`, `PostUpdate`, …) are **tag types**, not strings. You drop a system into a label by passing the tag.

## Picture the data flow

```
                  resources (shared singletons)
                       │
        ┌──────────────┼──────────────┐
        │              │              │
     systems  ───  queries  ───  components on entities
        │
     commands  ───  deferred World mutation (apply at safe points)
        │
     events    ───  typed message channels, double-buffered
```

A system **receives** typed parameters (`Query<...>`, `Res<T>`, `EventReader<T>`, `Commands`), **does its work**, and **never** touches anything it didn't ask for. That contract is what lets the scheduler run independent systems in parallel.

## Six things to keep in mind

1. **Components are plain data.** No virtuals, no inheritance. If it has logic, it belongs in a system.
2. **Systems are plain functions.** No base class. Whether a function is a system is decided by what parameters it takes.
3. **Queries describe access.** `Query<Transform, const Velocity>` says "I write `Transform` and read `Velocity`". The scheduler reads this and parallelises.
4. **Direct world mutation from inside a system is forbidden.** Use `Commands`. The world applies them when no system is iterating.
5. **Plugins compose.** A plugin is anything with `void build(App&)`. `DefaultPlugins` is just one plugin that adds the others.
6. **Main world and render world are different worlds.** Sim writes to main; render reads a per-frame snapshot. Sim N+1 can run while render N is still on the GPU.

## Where to go next

Read `01-app.md` and keep going in order. Each doc adds one concept on top of the previous.

---

**Bevy mapping** — every concept above maps 1:1 to Bevy's `App`, `World`, `Entity`, `Component`, `Resource`, `System`, `Query`, `Commands`, `Event`, `Schedule`, `Plugin`. The names, shapes, and rules are deliberately the same.
