# 0013 — Commands: deferred world mutation

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: TBD
- Implementation PR: TBD
- Merged in: TBD
- Depends on: 0001 (ECS), 0004 (resources & events)

## Scope

Add **Commands**, the deferred world-mutation API. Systems that need to spawn, despawn, or change components/resources record those changes into a per-system `Commands` buffer; the engine flushes the buffer at a safe point. This is the prerequisite for parallel system execution (M2) — without Commands, structural changes from inside a system would race against concurrent readers.

This was previously listed as "out of scope, later" in spec 0004. Splitting it out so it can be implemented and reviewed independently.

## Acceptance criteria

- [ ] `engine::Commands` system param. Acquired by adding a `Commands` parameter to a system signature.
- [ ] Entity ops: `cmd.spawn(Components...) -> Entity`, `cmd.despawn(Entity)`, `cmd.entity(Entity).insert(C)`, `cmd.entity(Entity).remove<C>()`.
- [ ] Resource ops: `cmd.insert_resource<T>(T)`, `cmd.remove_resource<T>()`.
- [ ] Escape hatch: `cmd.queue([](World&){ ... })` records an arbitrary closure to run at flush time.
- [ ] Spawn returns a stable `Entity` handle immediately (generation pre-allocated); component data lands at flush.
- [ ] Flush points: end of each schedule label (`First`, `PreUpdate`, `FixedMain`, `Update`, `PostUpdate`, `Last`). Plus an explicit `apply_deferred` ordering helper that lets systems within a schedule observe a previous system's commands.
- [ ] Flush order within a buffer matches record order. Across buffers, deterministic order by system topological position.
- [ ] Conflict metadata: a system with `Commands` is treated as a writer of "structural" access (blocks parallel structural changes from other systems in the same schedule slot). Resource/component reads/writes are still tracked separately.
- [ ] Unit tests in `tests/unit/test_commands.cpp`: spawn-then-read in next schedule, despawn, insert/remove component, insert/remove resource, `queue` closure, `apply_deferred` mid-schedule visibility. Tagged `[fast]`.

## Out of scope

- Parallel scheduler itself (spec 0006-M2). This spec only ensures the access metadata is correct.
- `EntityCommands` chained builder beyond `insert`/`remove` (no `with_children`, no relationships).
- Hierarchy commands (`despawn_recursive`, `add_child`). Belong to a future hierarchy spec.
- Bundle insertion (no `Bundle` trait until reflection lands).

## Files not to touch

- `engine/platform/*`, `engine/render*`, `plugins/*`.
- `specs/0004-resources-and-events.md` — keep that scope frozen.

## Notes for the implementing agent

- Storage: per-system `Commands` owns a `std::pmr::monotonic_buffer_resource` + a `std::vector<CommandOp>` of type-erased ops. Reset after each flush.
- Entity pre-allocation: bump a generation/index pair from the world's free list under a small lock at `spawn` time; the actual archetype move happens at flush.
- Flush is a single-threaded pass; correctness, not throughput.
- Read `docs/concepts/06-commands.md` for the user-facing mental model that this spec must match.
- After landing, update `docs/concepts/06-commands.md` to remove the "Spec status" note.
