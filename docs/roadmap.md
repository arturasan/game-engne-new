# Roadmap

Milestones are sized in **rough engineer-weeks** for an experienced C++ engineer working with an AI agent. Doubled for any agent working alone, halved for a focused team of two. Sizing is for **planning order**, not commitment.

Each milestone has a **demo gate** — a concrete runnable artifact that proves the milestone is done. No milestone closes without its demo.

---

## M0 — Scaffolding *(done)*

CMake + presets, vcpkg manifest, AGENTS.md + agent stubs, doctest harness, pre-commit, CI matrix skeleton, minimal `engine::App`. ADR 0001 (C++23 baseline, now superseded by ADR 0002, which raises the baseline to C++26 for the Fedora-only era). Spec 0000 (bring-up) is the next spec to execute.

**Demo gate:** `cmake --workflow=check` green, `hello_window` prints 5 frames.

---

## M0.5 — Bring-up on Fedora *(~0.5 wk, in progress)*

The Phase-0 scaffold was written but never compiled (network blocked on the authoring machine). Before any feature work begins, the scaffold must build clean on a fresh Fedora 40+ workstation with GCC 16 / Clang 20. Tracked in `specs/0000-bring-up.md`.

**Demo gate:** on a fresh clone, `cmake --workflow=check` finishes in under 60s cold and under 10s incremental; `hello_window` prints 5 frames; CI is green.

---

## M1 — Playable engine core *(~6 weeks)*

The smallest possible end-to-end engine: open a window, run an ECS world, render a sprite, play a sound, exit cleanly. Everything an agent needs to verify "the engine works" by looking at pixels and listening to a beep.

| Spec | Title | Sizing |
|---|---|---|
| 0001 | ECS archetype storage | 1.5 wk |
| 0002 | Platform: Window + Input (SDL3 hidden) | 0.5 wk |
| 0003 | Logging + structured JSON sink | 0.3 wk |
| 0004 | Resources + Events | 0.5 wk |
| 0005 | Renderer abstraction — clear color (SDL3 GPU hidden, ADR 0003) | 1 wk |
| 0006 | Transform + Camera2d/Camera3d | 0.3 wk |
| 0007 | Asset system v0 (Handle, synchronous loader) | 0.7 wk |
| 0008 | Sprite plugin — first sprite on screen | 0.7 wk |
| 0009 | Deterministic replay harness | 0.5 wk |
| 0010 | Screenshot diff harness + first golden PNG | 0.5 wk |
| 0011 | PBR mesh plugin — first lit cube (3D) | 1 wk |
| 0012 | Audio v0 — play a wav | 0.3 wk |

**Demo gate:** `examples/sprite_demo` renders a sprite with input-driven movement; `examples/pbr_demo` renders a lit rotating cube; `examples/audio_demo` plays a beep on keypress. All three examples produce stable screenshot/replay outputs in CI.

**Dependency DAG (which spec blocks which):**

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

Specs 0001, 0002, 0003, 0007 are **leaves** (no deps) — work on them in parallel if multiple agents are available.

---

## M2 — Renderer maturity *(~5 weeks)*

Make the renderer pleasant to use, not just functional. Material system, render graph, multi-camera, post-processing hooks.

- Material/shader authoring API (the *only* place backend types leak, behind `detail::backend_handle`)
- Render graph (nodes, edges, resource lifetimes)
- Multiple cameras + viewports
- Render layers (sprite, gizmo, ui)
- Post-process chain (HDR → tonemap → present)
- Sprite atlases + tilemap (2D)
- glTF 2.0 mesh import (3D)
- Skeletal animation (vertex-skinned, no IK)

**Demo gate:** `examples/scene_3d` loads a glTF model with PBR materials, ambient + directional + point lights, post-tonemap, free-camera. `examples/tile_world` renders a 100×100 tilemap with parallax background.

---

## M3 — Editor-grade ergonomics *(~6 weeks)*

The engine becomes usable for actual game prototyping by humans, not just for engine-team self-validation.

- Hot-reload of plugins via `cr.h` (DLL swap)
- Asset hot-reload (file watcher → `AssetEvent::Modified`)
- Inspector panel using Dear ImGui (read-only first, then edit)
- Reflection layer for components (C++23 fallback today, swap to P2996 when toolchains catch up)
- Save/load scenes (JSON, then bincode)
- Profiler integration (Tracy)
- gamepad input (SDL3 already supports it; expose in `engine::Input`)

**Demo gate:** edit a sprite's color in the inspector, save the scene, reload, observe the change persisted; modify a shader file, see the result on screen without restarting.

---

## M4 — Physics + networking foundations *(~5 weeks)*

- Box2D v3 (2D physics) behind `engine::Physics2D`
- Jolt (3D physics) behind `engine::Physics3D`
- Fixed-timestep integration with the existing `Schedule` system
- Networking transport abstraction (ENet first; spec the QUIC swap)
- Client-server scene replication (snapshot interpolation, no rollback yet)
- Input prediction / server reconciliation

**Demo gate:** `examples/pong` — two clients connect, paddles physics-collide with ball, latency tolerant up to 200 ms RTT.

---

## M5 — Multi-platform expansion *(~6 weeks)*

ADR 0002 narrowed M0–M4 to Fedora only. M5 re-introduces the platforms. Expect non-trivial cleanup of Linux-isms accumulated during M1–M4 (path casing, filesystem assumptions, signal-handler usage, etc.) before any Windows/macOS build succeeds.

- File a successor ADR to 0002 re-establishing the compiler matrix; if a second render backend is needed, also a successor to ADR 0003.
- Windows (MSVC + MinGW UCRT) — un-mark `docs/setup/windows.md`, update pinned tool versions, reinstate `win-*` CI jobs.
- macOS (Clang + Metal — SDL3 GPU's Metal backend, or a second Diligent/Vulkan path; decide via ADR at M5 start).
- Web (WASM via Emscripten; SDL3 GPU's WebGPU backend if mature, else emit GLES; audio via WebAudio behind miniaudio façade).
- Android (NDK build, OpenGL ES / Vulkan via SDL3 GPU).
- Portability test pass: enable `clang-tidy portability-*`, fix all hits.

**Demo gate:** the M3 demo runs on Fedora, Windows, macOS, in a browser, and on an Android device.

---

## M6 — Production hardening *(open-ended)*

- Asset pipeline (offline conversion → engine-native binary format)
- Streaming for large worlds
- Crash reporter (Breakpad / Crashpad behind façade)
- Telemetry sink (privacy-respecting opt-in)
- Documentation pass + tutorial series
- First public release candidate

---

## What's explicitly **not** on the roadmap (yet)

- Scripting language bindings (Lua / Python / C#). Add ADR + spec when needed.
- Editor as a separate application (M3 inspector is in-engine ImGui only).
- Console platforms (NDA-encumbered; needs a corporate sponsor).
- Ray tracing.
- Multiplayer matchmaking / cloud services.

---

## How to use this document as an agent

When given a task, locate it on the roadmap:

1. **Has a spec already?** → work the spec.
2. **In the current milestone but no spec?** → read the milestone overview, ask for a spec to be written, do not implement freestyle.
3. **In a future milestone?** → escalate. Future milestones may need rework of earlier code.
4. **Not on the roadmap?** → escalate. Almost certainly out of scope.
