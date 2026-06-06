# Bevy grounding reference

This document defines how this project uses Bevy as an external reference. It is process guidance, not an implementation spec.

## Reference baseline

- Bevy reference branch/tag: `latest`
- Pinned commit: `f667c282dad2c1419afb5836ded22a3ec263970e`
- Commit date: 2026-03-02T01:21:31Z
- Date selected: 2026-06-07

Normal implementation work uses this pinned commit when consulting Bevy examples or source. Use URLs that include the commit SHA, not moving branch links, in implementation PRs.

Baseline upgrades happen only during a deliberate docs/architecture review. Do not update the pinned commit opportunistically during feature work.

## Authority Order

1. Active spec
2. Accepted ADRs
3. Project architecture constraints
4. Existing code and tests
5. Pinned Bevy docs/examples/source
6. Other external research

Bevy clarifies target semantics and user-facing mental models. It does not override project contracts.

## Translation Principles

- Adapt behavior and mental model, not Rust syntax.
- Specs and ADRs override Bevy.
- Public APIs remain engine-owned, even when names intentionally mirror Bevy.
- Third-party backend types must not leak into public engine headers.
- Bevy features outside the active milestone do not become implementation requirements.
- Prefer a clean C++26 design over mimicking Rust traits, borrowing, macros, derive systems, or reflection.
- Keep Rust-specific implementation details out of C++ unless the active spec explicitly calls for an analogous mechanism.
- If Bevy behavior conflicts with this project's spec, ADR, architecture, existing code, or M1 scope, report the conflict and wait for approval before changing the contract.

## Intentional Project Differences

| Area | Bevy | This engine |
| --- | --- | --- |
| Language | Rust | C++26 |
| Window/platform | Bevy platform stack | SDL3 |
| M1 rendering backend | Bevy renderer/wgpu stack | SDL GPU abstraction |
| ECS | Bevy ECS | custom archetype ECS |
| Platform priority | broad cross-platform | Fedora/Linux is the sole supported platform through M4; Windows and macOS return at M5 |
| Reflection | extensive Bevy reflection | deferred/minimal |
| Scope | mature general engine | learning-oriented long-term engine |

These differences are intentional constraints, not defects. Treat them as translation boundaries.

## Research Protocol

When behavior is uncertain:

1. Check the active spec, accepted ADRs, architecture docs, and existing tests.
2. Consult pinned Bevy documentation and the relevant example paths.
3. Consult pinned Bevy source when documentation and examples are insufficient.
4. Consult Bevy issues, RFCs, or design discussions only when rationale is needed.
5. Report the uncertainty, Bevy behavior found, exact reference consulted, proposed C++ adaptation, and any conflict with this project.
6. Wait for approval before changing a contract.

Use these citation forms in implementation PRs:

- Documentation: include the Bevy docs page title and URL, plus the date accessed if the page is not tied to the pinned commit.
- Examples: cite `bevyengine/bevy@f667c282dad2c1419afb5836ded22a3ec263970e:<path>`.
- Source: cite `bevyengine/bevy@f667c282dad2c1419afb5836ded22a3ec263970e:<path>` and summarize the relevant behavior.
- Issues/RFCs/design discussions: cite the issue/discussion/PR number and explain why rationale was needed.

Do not copy large passages from Bevy documentation or source. Summarize the observed behavior and link the reference.

## Common Reference Areas

Use these Bevy areas as grounding when the active spec calls for the corresponding project feature:

| Project area | Bevy grounding |
| --- | --- |
| App and plugins | `examples/app/plugin.rs`, `examples/app/plugin_group.rs`, Bevy `App` and `Plugin` docs |
| ECS entities/components/queries | `examples/ecs/ecs_guide.rs`, Bevy ECS docs and `bevy_ecs` source |
| Resources | `examples/ecs/ecs_guide.rs`, Bevy `Resource`, `Res`, and `ResMut` docs |
| Events/messages | `examples/ecs/message.rs`, `examples/ecs/send_and_receive_messages.rs` |
| Schedules and systems | `examples/ecs/startup_system.rs`, `examples/ecs/custom_schedule.rs`, schedule docs |
| Commands/deferred mutation | examples that use `Commands`, plus `bevy_ecs` command source when needed |
| Transforms and cameras | `examples/transforms/transform.rs`, `examples/camera/2d_top_down_camera.rs`, `examples/camera/camera_orbit.rs` |
| Assets | `examples/asset/asset_loading.rs`, Bevy asset docs |
| Rendering concepts | `examples/2d/sprite.rs`, `examples/2d/move_sprite.rs`, `examples/3d/3d_scene.rs`, `examples/3d/pbr.rs`, render docs/source as needed |
| Audio | `examples/audio/audio.rs` |
| Screenshot/headless artifacts | `examples/app/headless.rs`, `examples/app/headless_renderer.rs`, `examples/window/screenshot.rs` |

## PR Reporting

For Bevy-grounded implementation work, include this section in the PR body when Bevy research materially influenced the implementation:

```md
## Bevy grounding

- Baseline:
- Documentation/examples/source consulted:
- Semantics adopted:
- C++ adaptation:
- Intentional differences:
- Unresolved questions:
```

This section is not required for purely local fixes where Bevy did not materially affect the work.
