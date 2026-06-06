# Development plan

This document is the **execution view** of the roadmap. The roadmap (`docs/roadmap.md`) says *what* gets built and in which milestone; this plan says *how the work gets ordered, parallelized across agents, and verified day to day*.

If you are an agent picking up work, read this **after** `AGENTS.md` and **before** picking a spec.

---

## Where we are

| Phase | Status | Artifact |
|---|---|---|
| M0  — Scaffolding | done (on disk, never compiled) | 27 files, 12 specs, full doc tree |
| M0.5 — Bring-up on Fedora | done (merged) | `specs/0000-bring-up.md` |
| M1  — Playable engine core | **active** | Current specs are M1 backlog unless their metadata marks them deferred or `Milestone: Future` |
| M2–M6 | future | roadmap only; M2 planning starts only after the M1 completion review |

The Phase-0 bring-up risk is retired: the scaffold now has a working Fedora loop via `cmake --workflow --preset check`. Future agents should keep that command green before starting feature work.

---

## Guiding principles

1. **One spec = one PR.** No exceptions. If a spec balloons, file a follow-up spec and shrink the original.
2. **Bring-up first, features second.** A broken inner loop costs more than any feature is worth.
3. **CI green is the only definition of "merged".** Local "works on my machine" is not enough.
4. **The Fedora-only window (M0–M4) is for velocity, not laziness.** Don't write code that is *gratuitously* Linux-specific — keep `std::filesystem::path`, avoid hard-coded `/`, prefer portable APIs when the cost is zero. ADR 0002 lists what cleanup we accept at M5.
5. **The abstraction rule (no third-party types in public engine headers) is non-negotiable through M4.** If it slows down the renderer enough to hurt, file an ADR proposing a targeted exception — don't quietly violate it.
6. **Agents work on the lowest-numbered unblocked spec by default.** Coordinate via the dependency DAG below before grabbing a higher-numbered one.

### Concepts, specs, and examples

Concept docs describe the target mental model. They are allowed to be ahead of current code because they explain where the engine is going, especially for agents learning the Bevy-style vocabulary. They do **not** authorize implementation outside the active spec.

After core foundations, implemented features should normally be consumed by at least one runnable example before their milestone is considered integrated. Specs remain the implementation unit; examples validate API ergonomics, integration, and user-facing behavior.

---

## M0.5 — Bring-up (~0.5 wk, done)

**Goal:** prove the scaffold compiles, tests pass, CI is green on a fresh Fedora clone.

**Spec:** `specs/0000-bring-up.md`.

**Day-by-day:**

| Day | Activity |
|---|---|
| 1 | Set up Fedora workstation per `docs/setup/fedora.md`. Toolbox with GCC 16 if host has 15. Install vcpkg. |
| 1 | First `cmake --preset linux-clang-asan` — record everything that breaks. |
| 2 | Fix CMake / preset / vcpkg.json issues. Get `linux-clang-asan` building. |
| 2 | Get `linux-gcc-rel` building. |
| 3 | Get `cmake --workflow --preset check` working end-to-end. |
| 3 | First PR. Get CI green. |

**Exit criteria:** every checkbox in `specs/0000-bring-up.md` ticked. This phase is merged; do not reopen it for M1 feature work.

---

## M1 — Playable engine core (~6 wk)

**Goal:** sprite on screen + lit cube + beep + deterministic replay + screenshot diff, all in CI.

### Spec dependency DAG (reminder)

```
0001 ECS ─┬─> 0004 Resources/Events ─┬─> 0006 Transform ─┬─> 0008 Sprite ──┐
          │                          │                   ├─> 0011 PBR  ────┤
          └──────────────────────────┘                   │                 │
0002 Window ──> 0005 Renderer ──────────────────────────┘                 │
                  │                                                        │
0003 Logging ─────┘                                                        │
                                                                           │
0007 Assets ──────────────────────────────────────────────────────────────┤
                                                                           │
                                  0009 Replay ──> 0010 Screenshot ─> demo gate
                                                                           │
                                  0012 Audio ────────────────────────────┘
```

**Leaves (no deps, parallelizable):** 0001, 0002, 0003, 0007.

### Suggested ordering for a single agent

```
0001 ECS                (1.5 wk)  — biggest unknown, do first
0003 Logging            (0.3 wk)  — used by everything
0002 Window+Input       (0.5 wk)
0004 Resources/Events   (0.5 wk)
0005 Renderer           (1.0 wk)
0006 Transform/Camera   (0.3 wk)
0007 Assets             (0.7 wk)
0008 Sprite plugin      (0.7 wk)  — first visible artifact (PR demo!)
0009 Replay harness     (0.5 wk)
0010 Screenshot harness (0.5 wk)
0011 PBR plugin         (1.0 wk)
0012 Audio              (0.3 wk)
0013 Commands           (TBD)     — M1 backlog; prepares M2 scheduler work
```

Total before spec 0013 was 7.8 weeks of sequential work for one experienced engineer with an AI agent. Spec 0013 needs sizing during M1 execution. Halve roughly for two engineers working independent paths through the DAG.

### Suggested ordering for two agents in parallel

Agent A track (rendering): 0002 → 0005 → 0006 → 0008 → 0010 → 0011
Agent B track (logic): 0001 → 0003 → 0004 → 0007 → 0009 → 0012

The two tracks join at 0008 (sprite) which is the first cross-track integration test.

### Example-driven / spec-driven workflow

Use this loop when a feature has a Bevy analogue or an example target:

1. Select the roadmap milestone or example target.
2. Inspect the equivalent pinned Bevy behavior, using `docs/references/bevy.md`.
3. Explore our current code read-only.
4. Identify the concrete capability gap.
5. Harden the active spec without expanding past its stated scope.
6. Record adopted semantics and intentional differences.
7. Implement in a fresh bounded session.
8. Review independently against the active spec and the Bevy references consulted.
9. Run the adapted C++ example.
10. Capture the requested artifact: console output, screenshot, video, golden file, or CI result.

Do not implement future architecture text unless the active spec requires it. If the Bevy behavior suggests scope not present in the active spec, record it as an intentional difference or follow-up question; do not implement it.

### Per-spec workflow (the loop)

```
1. Read AGENTS.md (always re-read; it's short).
2. Locate the roadmap milestone and confirm the active spec belongs to that milestone.
3. Read the spec end to end. Read its Out-of-scope twice.
4. Read the architecture chapter(s) it references.
5. If behavior is uncertain, inspect the pinned Bevy docs/examples/source per `docs/references/bevy.md`.
6. grep the engine for existing related code — there is more reuse than you think.
7. Write public headers + tests FIRST. Implementation second.
8. Add or update the smallest C++ example that consumes the feature when the spec calls for one.
9. After every meaningful change: `cmake --workflow --preset check`.
10. Before opening a PR, update the spec status, checklist, and PR link as described below.
11. Address review. Merge. Update the spec file with a one-line note if reality deviated from plan.
```

Do not implement future architecture text unless the active spec requires it.

### Spec metadata

Every spec starts with this metadata block:

```md
- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: TBD
- Implementation PR: TBD
- Merged in: TBD
```

Field meanings:

- `Owner`: person or agent currently responsible for moving the spec.
- `Milestone`: roadmap milestone that owns the work, such as `M1`; use `Future` only when the spec is intentionally outside the current milestone.
- `Status`: one of the status values below.
- `Tracking issue`: issue URL or number for discussion before implementation, or `TBD`.
- `Implementation PR`: PR URL that implements the spec, or `TBD`.
- `Merged in`: merge commit, release, or PR merge reference once the implementation has landed, or `TBD`.

Status values:

- `draft`: accepted backlog item that is not currently in review.
- `in-review`: implementation PR is open and review is in progress.
- `implemented`: implementation PR has merged; all completed and incomplete acceptance criteria are recorded in the spec.
- `deferred`: intentionally postponed; keep the reason in the spec or roadmap.
- `superseded`: replaced by another spec, ADR, or roadmap item; link the replacement.

When Codex opens a PR for a spec, it must change `Status` to `in-review` and add the PR URL to `Implementation PR`. It must check off only acceptance criteria actually satisfied by that PR. If a criterion is partially satisfied, leave it unchecked and add a short note explaining what shipped and what remains. After the implementation PR merges, a follow-up docs/process PR may change `Status` to `implemented` and fill `Merged in`.

### Definition of done for an M1 spec

- All satisfied Acceptance Criteria checked; any partial criterion remains unchecked with a note.
- CI green on both jobs.
- Public headers do not include any third-party header (the grep canary).
- A `[fast]` doctest covers the new behavior. Run in <100ms.
- If the spec introduces a visible artifact (sprite, cube, sound): an example under `examples/` demonstrates it, runs cleanly for at least 60 frames.
- If the spec is renderer-touching: screenshot test added or updated.
- Conventional commit message.
- Spec file's checklist actually edited to reflect what shipped.

### M1 exit demo

Recorded video (or live demo) of:

1. `./build/linux-clang-asan/Debug/examples/sprite_demo/sprite_demo` — sprite scrolls with arrow keys, beep plays on spacebar (audio_demo merged in).
2. `./build/linux-clang-asan/Debug/examples/pbr_demo/pbr_demo` — lit cube rotates, glTF helmet renders.
3. `ctest --preset linux-clang-asan` — all green for the fast test suite.

```sh
cmake --build --preset linux-clang-asan
./build/linux-clang-asan/Debug/examples/sprite_demo/sprite_demo
./build/linux-clang-asan/Debug/examples/pbr_demo/pbr_demo
ctest --preset linux-clang-asan
```

---

## M2 — Renderer maturity (~5 wk)

Not specced in detail yet. M2 planning starts only after the M1 completion review. Do not create M2 specs, including renderer-maturity specs, before the M1 demo gate is passed and the review records what actually shipped.

Possible M2 areas, to be specced later:

- Material/shader API (the one approved abstraction leak)
- Render graph
- Multi-camera + viewports
- Render layers
- Post-process chain
- Sprite atlas + tilemap
- Skeletal animation

Parallelism scheduler and audio determinism are future planning topics; do not assign spec numbers or start PRs for them until the M1 completion review opens M2 planning.

---

## M3+ — Ergonomics, physics, multi-platform

See roadmap. Specs not yet written. Do not start a spec for a milestone whose predecessor's demo gate has not passed.

---

## Agent collaboration rules

When multiple agents work the repo at once:

1. **Lock specs by opening a draft PR titled `wip: 0007 ...`** before doing any work. Other agents skip locked specs.
2. **Never edit a file listed in another spec's "Files not to touch".** If you need to, comment on that spec's PR and coordinate.
3. **Run the bring-up loop after every pull.** A green local build before you start writing prevents "is it me or main?" debugging.
4. **Commit early, push early.** Long-lived branches multiply conflicts. Daily push minimum.
5. **If you finish a spec and the next one is blocked, write tests or docs on completed work, not adjacent code.** Refactoring without a spec is forbidden (AGENTS.md rule).

## What to do when a spec is wrong

Specs are written before the code. Reality bites.

1. **Small deviation** (a parameter name, an enum value, a struct field): edit the spec in the same PR. Note it in the PR body.
2. **Medium deviation** (a sub-feature is harder than expected, an API choice doesn't work): open a follow-up spec and shrink the current one. Don't try to push through.
3. **Architectural deviation** (a doc in `docs/architecture/` is wrong): stop, file an ADR proposing the change, get it merged before continuing. Don't unilaterally change architecture in a feature PR.

## Velocity expectations

Realistic, with one experienced engineer + AI agent pair, full time:

- M0.5: 3–5 days.
- M1: 7–10 weeks (longer than the 6-week roadmap sizing — sizings are optimistic).
- M1 demo gate: ~2.5 months from a green M0.5.

Realistic, with two agents in parallel:

- M1: 5–7 weeks. The parallel speedup is sub-linear because of the 0008-sprite integration bottleneck.

The single biggest velocity risk is the **abstraction rule + renderer**. Spec 0005 is sized at 1 week but realistically takes 1.5–2 weeks for someone seeing SDL3 GPU for the first time. SDL3 GPU is younger than wgpu so expect to read source / file the occasional upstream issue. Budget accordingly. (See ADR 0003.)

---

## Tracking

- **Per-milestone status:** updated in `docs/roadmap.md` (this file is process, not state).
- **Per-spec status:** the metadata block at the top of each `specs/*.md` file (`draft`, `in-review`, `implemented`, `deferred`, `superseded`).
- **Bring-up findings:** the PR body for `specs/0000-bring-up.md`. Skim this before starting M1 — it tells you what to expect from the toolchain.
- **ADR log:** `docs/adr/`. Read newest first when an architectural question comes up.

---

## When the plan needs to change

Reopen this document (and likely the roadmap) when **any** of:

- M0.5 reveals fundamental scaffold problems (e.g. SDL3 GPU not packageable on Fedora 40 via vcpkg — would force a manual install path or base-image bump).
- A spec balloons past 2× its sizing.
- A second developer joins (changes the parallel-agent advice).
- The Fedora-only assumption is broken (someone wants to build on Windows before M5).
- A C++26 feature we depend on stalls in GCC 16 → revisit ADR 0002 / ADR 0001.

The plan is a living document. Edit it. Don't let it lie.
