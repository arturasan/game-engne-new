# 0004 — Resources and events

- Owner: TBD
- Milestone: M1
- Status: implemented
- Tracking issue: TBD
- Implementation PR: https://github.com/arturasan/game-engne-new/pull/6
- Merged in: 9bb11a5 (2026-06-03)

## Scope

Add **resources** (singleton state) and **events** (per-frame typed message channels) to `engine::World`. These are the missing pieces between bare ECS (spec 0001) and useful systems (spec 0002+).

## Acceptance criteria

- [x] `World::insert_resource<T>(T)`, `World::resource<T>() -> T&`, `World::try_resource<T>() -> T*`, `World::remove_resource<T>()`.
- [x] Inserting a resource twice replaces. Removing a missing resource is a no-op (debug-asserts).
- [x] `Events<T>` resource with `send(T)`, `read() -> std::span<const T>`. Double-buffered: events sent in frame N are readable in frame N and N+1, then dropped.
- [x] `EventReader<T>` system param tracks per-reader cursor so each reader sees each event exactly once.
- [x] `App::add_event<T>()` inserts `Events<T>` and registers the per-frame swap system in `First`.
- [x] `Schedule` accepts systems that take `Res<T>`, `ResMut<T>`, `EventReader<T>`, `EventWriter<T>` as parameters (extending the M0 stub).
- [x] Conflict detection: two systems with `ResMut<T>` cannot run in parallel (data for spec 0006 of M2; the access metadata must already be present in M1).
- [x] Unit tests in `tests/unit/test_resources.cpp` and `tests/unit/test_events.cpp`: insert/get, replace, double-buffer drop, reader cursor, multiple readers. Tagged `[fast]`.

## Out of scope

- Parallel scheduler (M2 / spec 0006 of M2).
- Persistent / serialized resources.
- `Local<T>` system-local state (later).
- Commands buffer for deferred world mutation — see spec 0013.

## Files not to touch

- `engine/platform/*`, `engine/render*`, `plugins/*`.

## Notes for the implementing agent

- Resource storage: `std::unordered_map<std::type_index, std::unique_ptr<void, void(*)(void*)>>` or a sparse vec keyed by `type_id_for<T>()`. Either is fine; document choice.
- Events: ring of two `std::vector<T>` per channel. Swap on `First`.
- System param machinery: extend the M0 hand-written `system_traits<F>`. Reflection isn't here yet; expect some boilerplate per param type.
- Read `docs/architecture/02-ecs.md` for storage conventions.

## Implementation notes

- Resource storage uses `ResourceId`, a process-local integer id allocated with the same monotonic pattern as `ComponentId`, but with a separate counter and map because resources do not participate in archetype signatures.
- `App::first()` is the early schedule phase for M1. `App::add_event<T>()` inserts `Events<T>` and registers event maintenance there before the regular update schedule runs each frame.
- `Events<T>::read()` returns a contiguous span backed by the channel's readable buffer. Internally the channel retains current and previous event records so `EventReader<T>` can keep an independent per-system cursor.
