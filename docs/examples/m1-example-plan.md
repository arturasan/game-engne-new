# M1 example plan

M1 implementation remains spec-driven: each PR implements one active spec. This document maps those specs to small runnable examples so integration is validated through user-facing behavior, not only unit tests.

Bevy examples listed here were verified against `bevyengine/bevy@f667c282dad2c1419afb5836ded22a3ec263970e`. A listed Bevy path is grounding or inspiration, not a requirement to reproduce the example exactly.

One runnable example may validate several specs. A separate executable is not
required for every row. Prefer a small example ladder and shared milestone demos
over redundant examples created only to satisfy this map.

## Classification

| Class | Meaning |
| --- | --- |
| M1 required | Needed to integrate or demonstrate the M1 playable-engine core. |
| M2 candidate | Useful follow-up once M1 is complete; do not create M2 specs yet. |
| future/research | Worth studying later, but outside the current milestone. |
| intentionally out of scope | Explicitly not part of this milestone or project direction. |

## M1 Required Map

| Spec | Target behavior | Bevy documentation category | Inspected Bevy example path(s) | Proposed C++ example path | Current status | Intended artifact |
| --- | --- | --- | --- | --- | --- | --- |
| `0000` bring-up | Build and test smoke run that proves an app can execute a bounded frame loop. | App, headless app, schedule runner | `examples/hello_world.rs`, `examples/app/headless.rs` | `examples/hello_window/` | Exists; spec implemented | Console output, CI result |
| `0001` ECS archetype storage | Spawn entities with components and iterate matching component sets deterministically. | ECS entities, components, queries | `examples/ecs/ecs_guide.rs` | `examples/ecs_iteration/` | Proposed; no example yet | Console output, CI result |
| `0002` window and headless input | Open a window, pump keyboard/mouse state, and support headless execution for CI. | Windowing and input | `examples/input/keyboard_input.rs`, `examples/input/mouse_input.rs`, `examples/app/headless.rs` | `examples/input_probe/` | Proposed; no example yet | Console output, CI result |
| `0003` logging | Emit human-readable and structured logs from app systems. | App logging | `examples/app/logs.rs` | `examples/logging_demo/` | Proposed; no example yet | Console output, structured JSON log |
| `0004` resources and events | Insert resources, send typed events/messages, and read them from systems in deterministic order. | Resources, messages/events, system ordering | `examples/ecs/ecs_guide.rs`, `examples/ecs/message.rs`, `examples/ecs/send_and_receive_messages.rs` | `examples/resources_events/` | Proposed; no example yet | Console output, CI result |
| `0005` renderer clear color | Open a renderable window or headless surface and clear to a known color. | Window clear color and rendering startup | `examples/window/clear_color.rs` | `examples/clear_color/` | Proposed by spec; not implemented | Screenshot, CI result |
| `0006` transform and camera | Spawn transforms and a 2D/3D camera, then update transform/camera state over frames. | Transforms and cameras | `examples/transforms/transform.rs`, `examples/camera/2d_top_down_camera.rs`, `examples/camera/camera_orbit.rs` | `examples/camera_transform/` | Proposed; no example yet | Console output or screenshot |
| `0007` asset system v0 | Load an image or raw bytes through `AssetServer`, store in `Assets<T>`, and report load state. | Assets and handles | `examples/asset/asset_loading.rs` | `examples/asset_load/` | Proposed; no example yet | Console output, CI result |
| `0008` sprite plugin | Render and move a sprite with keyboard input. | 2D sprites and time-based movement | `examples/2d/sprite.rs`, `examples/2d/move_sprite.rs` | `examples/sprite_demo/` | Required by spec; not implemented | Screenshot, video, replay input |
| `0009` deterministic replay | Drive an example from recorded input and compare per-frame hashes. | Headless app, deterministic system behavior | `examples/app/headless.rs`, `examples/2d/move_sprite.rs` | `examples/sprite_demo/` plus `tests/replay/recordings/sprite_demo_001.replay` | Required by spec; not implemented | Golden hash file, CI result |
| `0010` screenshot diff | Capture an example frame and compare against a golden PNG. | Headless rendering and screenshots | `examples/app/headless_renderer.rs`, `examples/window/screenshot.rs`, `examples/window/clear_color.rs` | `examples/clear_color/`, `examples/sprite_demo/` | Required by spec; not implemented | Golden PNG, diff PNG on failure, CI result |
| `0011` PBR mesh plugin | Render a lit rotating cube and a glTF-derived mesh/material scene. | 3D scene, PBR, glTF loading | `examples/3d/3d_scene.rs`, `examples/3d/pbr.rs`, `examples/gltf/load_gltf.rs` | `examples/pbr_demo/` | Required by spec; not implemented | Screenshot, video |
| `0012` audio v0 | Load and play a short sound, with null backend support for CI. | Audio playback | `examples/audio/audio.rs` | `examples/audio_demo/` | Required by spec; not implemented | Audible demo, console/log confirmation in CI |
| `0013` commands deferred mutation | Queue spawn/despawn/component/resource mutations from systems and flush at safe points. | Commands and deferred world mutation | `examples/ecs/ecs_guide.rs`, `examples/2d/move_sprite.rs`, `examples/gltf/load_gltf.rs` | `examples/commands_demo/` | Proposed; no example yet | Console output, CI result |

## M2 Candidates

These are useful once the M1 completion review has recorded what shipped. They do not create M2 specs by themselves.

| Example target | Bevy grounding | Why defer |
| --- | --- | --- |
| Sprite atlas or tile map demo | `examples/2d/texture_atlas.rs`, `examples/2d/tilemap_chunk.rs` | Spec 0008 explicitly excludes atlases and tilemaps. |
| Multi-camera or viewport demo | `examples/3d/split_screen.rs`, `examples/camera/custom_projection.rs` | M1 has one main camera. Multi-camera is roadmap M2. |
| Custom material/shader demo | `examples/shader/shader_material.rs`, `examples/shader_advanced/custom_render_phase.rs` | M1 ships built-in sprite/PBR shaders only. |
| Asset hot-reload demo | `examples/asset/hot_asset_reloading.rs` | Spec 0007 excludes hot reload. |

## Future/Research

| Topic | Bevy grounding | Why it is research |
| --- | --- | --- |
| Reflection and inspector ergonomics | `examples/reflection/reflection.rs`, `examples/reflection/serialization.rs` | Project reflection is deferred/minimal until later milestones and ADR review. |
| Render graph and custom phases | `examples/shader_advanced/custom_phase_item.rs`, `examples/shader_advanced/custom_render_phase.rs` | Requires renderer maturity beyond M1. |
| Animation graph and skinned meshes | `examples/animation/animation_graph.rs`, `examples/animation/animated_mesh.rs` | Outside the M1 playable-core slice. |
| Picking and editor-style interaction | `examples/picking/simple_picking.rs`, `examples/picking/mesh_picking.rs` | Belongs to editor-grade ergonomics, not M1. |

## Intentionally Out Of Scope

| Topic | Reason |
| --- | --- |
| Reproducing every Bevy example | Bevy is grounding, not a compatibility target. |
| Mobile, web, and no-std examples | Project is Fedora/Linux-first through M4 and returns to broader platforms at M5. |
| UI-heavy examples | UI/editor work is later roadmap scope. |
| Advanced renderer features such as bindless, meshlets, post-processing, and atmospheric effects | M1 renderer scope is clear color, sprites, and basic PBR only. |
