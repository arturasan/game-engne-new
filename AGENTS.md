# AGENTS.md — Canonical brief for AI coding agents

This file is the single source of truth for **all** AI agents (Codex, Copilot, Gemini, Claude Code) working in this repo. Other agent files (`CLAUDE.md`, `GEMINI.md`, `.github/copilot-instructions.md`) point here.

## What this project is

A Bevy-inspired 2D + 3D game engine in C++26. Plugin architecture. **Fedora-only through M4** (see ADR 0002); Windows + macOS return at M5.

## Non-negotiable rules

1. **No third-party type appears in any public engine header.** All deps (SDL3, SDL3 GPU, spdlog, GLM, miniaudio, etc.) live behind interfaces in `engine/` and are implemented in `*_backend.cpp` files. Grep test: `grep -rE 'SDL_|SDL_GPU|glm::|spdlog|ImGui' engine/**/*.hpp` must return zero hits.
   - **Adding a third-party library is allowed** if it sits behind an `engine::` interface AND has an ADR justifying it. Default to "build our own" only where the mental model demands tight integration (ECS, Schedule, Plugin, Commands) — see "Build vs buy" below.
2. **One PR = one spec.** Read `specs/<feature>.md` before editing. Honor its "Out of scope" section.
3. **Do not touch files listed in the spec's "files-not-to-touch" section.**
4. **No new dependencies without an ADR.** Add an ADR under `docs/adr/` first.
5. **Conventional Commits.** `feat:`, `fix:`, `refactor:`, `docs:`, `test:`, `build:`, `ci:`, `chore:`.
6. **C++26 baseline** (Fedora-only era). Use any feature both GCC 16 and Clang 20 implement. If only one implements it, guard with `__cpp_*` and provide a `-std=c++23` fallback for the lagging compiler. C++23 is still acceptable for files that don't need anything newer.

## How to build & test

```sh
# First time only
git submodule update --init --recursive
# (vcpkg manifest mode auto-resolves deps on configure)

# Fast inner loop (~10s incremental)
cmake --workflow --preset check

# Full configure/build for a single preset
cmake --preset linux-clang-asan && cmake --build --preset linux-clang-asan
ctest --preset linux-clang-asan
```

Active presets: `linux-clang-asan`, `linux-gcc-rel`. The `win-*` presets remain in `CMakePresets.json` but are unmaintained until M5.

**Fedora primary toolchain:** GCC 16 (via `toolbox` if the host has an older default), Clang 20. See `docs/setup/fedora.md`.

## Where things live

- `engine/core/` — App, Plugin, Schedule
- `engine/ecs/` — World, archetype storage, queries
- `engine/platform/` — Window, Input (SDL3 hidden)
- `engine/render/` — Renderer abstraction (SDL3 GPU hidden — see ADR 0003)
- `engine/render2d/`, `engine/render3d/` — phase-specific render code
- `plugins/` — first-party plugins built on the engine core
- `tests/unit/` — doctest unit tests (tagged `[fast]` or `[slow]`)
- `tests/refs/` — golden PNGs for screenshot tests
- `tests/replay/` — golden frame-hash logs for deterministic replay
- `examples/` — runnable demos; smallest reproducer of any feature lives here
- `docs/adr/` — architecture decision records (MADR template)
- `specs/` — per-PR feature specs
- `build/logs/last_run.jsonl` — structured JSON logs from the most recent example run. **Always grep this before guessing what went wrong.**

## Test framework

doctest. Tag tests `[fast]` (must run in <100ms) or `[slow]`. `cmake --workflow --preset check` runs only `[fast]`.

## Where to read more

- `docs/setup/onboarding.md` — first-day-on-project guide.
- `docs/setup/fedora.md` — primary supported platform (Fedora 40+, GCC 16 / Clang 20).
- `docs/setup/windows.md` — **unmaintained until M5**; see ADR 0002.
- `docs/development-plan.md` — execution view: which spec to pick next, how to parallelize across agents, what "done" means per PR.
- `docs/roadmap.md` — milestones M0–M6 and the spec dependency DAG.
- `docs/architecture/00-overview.md` — layered architecture overview; the rest of `docs/architecture/` (01–09) covers core, ECS, platform, rendering, assets, scheduler, testing, error handling, and code conventions.
- `docs/concepts/README.md` — Bevy-style mental-model tour (App, World, Systems, Queries, Commands, Resources, Events, Schedules, Plugins, Assets, Rendering). Start here if you've never used an ECS engine.
- `docs/adr/` — accepted architecture decisions. Latest first: **ADR 0003** (SDL3 GPU render backend), **ADR 0002** (Fedora-only through M4). File a new ADR before adding a dependency or changing a contract.
- `specs/` — open work, one spec per upcoming PR. **Start at `specs/0000-bring-up.md` on any fresh clone.**

## Style

- `.clang-format` and `.clang-tidy` are authoritative. Pre-commit runs both.
- snake_case for functions, vars, files. PascalCase for types. `ALL_CAPS` for macros only.
- `#pragma once` (no include guards).
- Prefer free functions over member functions when no state is captured.

## When you're stuck

1. Read the spec again.
2. Read `build/logs/last_run.jsonl`.
3. Check `docs/adr/` for prior decisions on the area.
4. If still stuck, **stop and report** — do not refactor unrelated code looking for a fix.

## Build vs buy

The engine **builds its own** for layers where the Bevy mental model demands tight integration: `App`, `World`, archetype storage, `Schedule`, `Query`, `Commands`, `Plugin`, `Events`, `Resources`, `Handle<T>` / `Assets<T>`. These are the engine's identity; wrapping someone else's model leaks.

The engine **uses third-party libs behind an `engine::` interface** for everything else where a mature lib gives real value: SDL3 (window/input), SDL3 GPU (renderer), spdlog (logging), GLM (math), miniaudio (audio), Jolt / Box2D (physics), stb_image / tinygltf (asset decoding).

Any new dependency requires an ADR. Any swap (e.g. SDL3 GPU → raw Vulkan, GLM → own math) requires an ADR. The public-header rule (rule 1) holds regardless of which side of the line a lib lands on.

## What NOT to do

- Do not build your own ECS storage *outside* `engine/ecs/`, and do not bring in EnTT/flecs — the engine ships its own World/archetype implementation by design (see "Build vs buy").
- Do not edit `tests/refs/` PNGs by hand. Use `--update-refs` on the screenshot harness.
- Do not disable pre-commit hooks. If a hook is wrong, fix the hook in `.pre-commit-config.yaml`.
- Do not amend or force-push commits unless explicitly asked.
- Do not add comments that restate what the code does.
