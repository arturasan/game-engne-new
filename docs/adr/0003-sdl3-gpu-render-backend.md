# 0003 — SDL3 GPU as the M1 render backend; own Vulkan path deferred

- Status: accepted
- Date: 2026-06-03
- Related: ADR 0002 (Fedora-only through M4)

## Context

The original plan named **wgpu-native** as the hidden backend for `engine::Renderer`. Reasons to revisit:

- wgpu-native is Rust-built and exposes a C shim. Bringing it in pulls `cargo` + `rustc` into the build pipeline, on top of the existing C++26 toolchain. Heavier than the value it delivers for our M1–M4 scope.
- wgpu tracks the **WebGPU** spec, which is intentionally conservative: no bindless resources, no mesh shaders, no work graphs, ray tracing only via vendor extensions. We don't need those for 2D + basic 3D, but the gap matters once we start optimising or doing GPU-driven rendering (M3+).
- Fedora-only era (ADR 0002) makes the Rust build cost a per-developer tax with no platform-portability upside (we'd still only target Linux).
- We already depend on SDL3 for window + input. **SDL3 added a first-class GPU API in 2024** (Vulkan / D3D12 / Metal cross-platform). Using it costs zero additional dependencies.
- Alternatives evaluated:
  - **Diligent Engine** — feature-rich, but a large C++ dependency we don't need yet.
  - **The Forge** — AAA-grade, heavier, more opinionated than the engine wants to be.
  - **Raw Vulkan** — maximum control, maximum code. Worth it only when SDL3 GPU's ceiling is the actual blocker.

## Decision

Through milestones M1–M4, `engine::Renderer` is implemented by **a single SDL3 GPU backend** (`engine/render/sdl3_gpu_backend.cpp`). SDL3-GPU types never appear in public headers — the existing rule from `AGENTS.md` holds.

- **M1–M2:** `SpritePlugin` and the basic PBR test render against SDL3 GPU.
- **M3 review:** if GPU-driven rendering, bindless, mesh shaders, or compute features beyond SDL3 GPU's surface become required, file a follow-up ADR and add a second backend (`engine/render/vulkan_backend.cpp`) selectable at configure time.
- The `engine::Renderer` / `CommandEncoder` / `ShaderModule` / `Pipeline` abstractions are designed against SDL3 GPU's shape but kept generic enough that a raw-Vulkan backend can implement the same interface without leaking SDL3 types.
- Shaders are authored as **SPIR-V** (or HLSL → SPIR-V via DXC). SDL3 GPU cross-compiles to backend-specific bytecode at load time; that path stays inside the backend.

## Consequences

- **Build:** no Rust toolchain. SDL3 is already in `vcpkg.json`.
- **Feature ceiling:** SDL3 GPU exposes compute, render passes, samplers, vertex/index/uniform/storage buffers. It does **not** expose bindless, mesh shaders, work graphs, or ray tracing. Plugins that need those will block until the M3 review.
- **Cross-platform:** SDL3 GPU runs on Vulkan, D3D12, Metal. Even though we're Fedora-only through M4, this de-risks M5 (Windows/macOS re-introduction).
- **Risk:** SDL3 GPU is younger than wgpu; we may hit bugs and need to file upstream issues. Mitigation: keep backend surface small and well-tested.
- **Reversal cost:** swapping to wgpu, Diligent, or raw Vulkan is a single-backend rewrite, no public-header churn. The whole point of the abstraction rule.

## Spec impact

- `specs/0005-renderer-clear-color.md` — implement against SDL3 GPU.
- `specs/0008-sprite-plugin.md`, `specs/0011-pbr-mesh-plugin.md` — same.
- `docs/architecture/04-rendering.md` and `docs/development-plan.md` — replace wgpu references with SDL3 GPU.
- `AGENTS.md` table that lists wgpu — update to SDL3 GPU.

## Revisit triggers

- A confirmed need for bindless / mesh shaders / work graphs / ray tracing (spec-driven, not speculative).
- SDL3 GPU's release pace falls behind our needs.
- A second backend is required for non-Linux platforms in M5 — that ADR will decide whether to add raw Vulkan or adopt Diligent/wgpu then.
