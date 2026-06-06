# Core

- Project: Bevy-inspired 2D + 3D game engine in C++26 with plugin architecture.
- Canonical agent brief: `AGENTS.md`. Other agent instruction files defer to it.
- Active roadmap work is tracked in `docs/roadmap.md` and spec metadata. Before selecting work, identify the lowest-numbered unblocked draft spec unless the user explicitly chooses another task. Do not cache current spec completion state in this memory.
- Work must be spec-scoped: one PR = one `specs/<feature>.md`; read scope, out-of-scope, and files-not-to-touch before edits. Do not implement roadmap/future architecture unless active spec requires it.
- Source map on disk now: `engine/core` = App/Plugin/Schedule/log/result; `engine/ecs` = Entity/World/archetype/resources/events; `engine/platform` = Window/Input/events with SDL3 hidden; `examples/hello_window`; `tests/unit`. `plugins/`, render, render2d/render3d, assets, audio are planned/docs-backed but not all present yet.
- Architecture rule: dependencies flow downward from `examples/` -> `plugins/` -> `engine/`; no plugin header included by engine; `engine/core` and `engine/ecs` stay independent of platform/render/assets.
- Non-negotiable public-header boundary: no third-party type in any public `engine/**/*.hpp`. Canary: `grep -rE 'SDL_|SDL_GPU|glm::|spdlog|ImGui' engine/**/*.hpp` should have zero hits.
- Dependencies may be used behind `engine::` interfaces and backend translation units only; new dependencies or dependency swaps require an ADR in `docs/adr/` first.
- Build-vs-buy identity layers are in-house: `App`, `World`, archetype storage, `Schedule`, `Query`, `Commands`, `Plugin`, `Events`, `Resources`, `Handle<T>/Assets<T>`. Do not introduce EnTT/flecs.
- Accepted ADRs: ADR 0002 Fedora-only through M4 and C++26 baseline; ADR 0003 SDL3 GPU render backend for M1-M4; ADR 0004 process-local integer ECS component IDs. ADR 0001 is superseded by 0002.
- Debugging convention: inspect `build/logs/last_run.jsonl` before guessing about example/runtime failures.
- Read when needed: project workflow/commands in `mem:suggested_commands`; platform/build/deps in `mem:tech_stack`; coding/API conventions in `mem:conventions`; done criteria in `mem:task_completion`.
