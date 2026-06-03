# 0005 — Renderer: clear color (SDL3 GPU hidden)

- Owner: TBD
- Milestone: M1
- Status: draft
- Tracking issue: TBD
- Implementation PR: TBD
- Merged in: TBD
- Implements: ADR 0003

## Scope

Stand up the rendering abstraction in `engine/render/`. First milestone: open a window (spec 0002) and clear it to a configurable color every frame. **SDL3 GPU** lives entirely in `engine/render/detail/` (see ADR 0003 for why SDL3 GPU over wgpu). Public headers must never mention `SDL_GPU*` symbols.

## Acceptance criteria

- [ ] `engine::Renderer` with `create(SurfaceDescriptor) -> Result<Renderer>`, `begin_frame() -> CommandEncoder`, `submit(CommandEncoder&&)`, `present()`, `resize(int, int)`.
- [ ] `engine::CommandEncoder`, `Texture`, `Buffer`, `ShaderModule`, `Pipeline`, `BindGroup` exist as opaque public handles. None expose SDL3 GPU types.
- [ ] `ClearColor` resource (RGBA floats); `RenderPlugin::build(App&)` schedules a `Render` system that clears the swap chain to `ClearColor`.
- [ ] `RenderPlugin` integrates with `PlatformPlugin` via the `Window` resource (gets the SDL window via an internal-only helper; converts to an `SDL_GPUDevice` + `SDL_GPUSwapchain` inside the backend).
- [ ] `ENGINE_HEADLESS=1` makes the renderer create an off-screen color target instead of a swap chain; `Renderer::read_back()` returns the rendered bytes.
- [ ] Grep test: `grep -rE 'SDL_GPU|SDL_Gpu' engine/render/*.hpp engine/render2d/*.hpp engine/render3d/*.hpp` returns zero hits.
- [ ] Unit test `tests/unit/test_renderer.cpp` (tagged `[slow]`): create a headless renderer, clear to red, read back, assert the center pixel is red.
- [ ] Example `examples/clear_color/` opens a window and clears it to cornflower blue. Runs cleanly for 60 frames.

## Out of scope

- Drawing geometry — spec 0008 introduces sprites.
- Render world / extract step — spec 0008 wires that.
- Render graph — M2.
- Multiple surfaces / windows.
- HDR / tonemapping.
- Shaders (the loader exists, but no built-in shader yet).

## Files not to touch

- `engine/core/*`, `engine/ecs/*`, `engine/platform/*` (consumer of, not editor of).
- `plugins/*`.

## Notes for the implementing agent

- Read `docs/architecture/04-rendering.md` and ADR 0003 — the abstraction rule is the whole point of this spec.
- SDL3 GPU lives behind `SDL3/SDL_gpu.h`. SDL3 itself is already a dep (spec 0002), so no new package.
- Window → surface: SDL3 GPU exposes `SDL_ClaimWindowForGPUDevice(device, window)` and `SDL_AcquireGPUSwapchainTexture(...)`. Keep that wiring in `engine/render/detail/sdl3_gpu_surface.cpp`.
- For headless: create an `SDL_GPUTexture` color target sized 256×256 by default. Configurable via `HeadlessConfig`.
- CI pinning: on Fedora CI run with the **Vulkan backend** of SDL3 GPU on **Mesa llvmpipe** (software rasterizer) for byte-stable screenshots. Set `SDL_GPU_DRIVER=vulkan` and `LIBGL_ALWAYS_SOFTWARE=1` / `MESA_LOADER_DRIVER_OVERRIDE=llvmpipe`.
- Wrap every SDL_GPU handle (`SDL_GPUBuffer*`, `SDL_GPUTexture*`, …) with a strong-typed opaque struct in `engine/render/detail/`. Lifetimes managed by `Renderer`.
