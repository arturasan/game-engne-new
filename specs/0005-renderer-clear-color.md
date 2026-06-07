# 0005 — Renderer: clear color (SDL3 GPU hidden)

- Owner: TBD
- Milestone: M1
- Status: in-review
- Tracking issue: TBD
- Implementation PR: https://github.com/arturasan/game-engne-new/pull/12
- Merged in: TBD
- Implements: ADR 0003

## Scope

Stand up the smallest coherent renderer foundation for M1:

- create an SDL3 GPU device behind `engine::` types;
- integrate with the single primary `Window` from spec 0002 when not headless;
- clear the current frame to a configurable `ClearColor`;
- support deterministic headless clear/readback for tests and the screenshot harness;
- expose only the atomic clear/readback surface required by specs 0008, 0010, and 0011 to build on later.

This spec does **not** introduce the render world, extract schedule, shader loading, draw calls, texture assets, sprites, cameras, PBR, or a render graph. Those remain target architecture from `docs/architecture/04-rendering.md` and are owned by later specs.

## Prerequisites

- Spec 0002 is implemented: `PlatformPlugin` inserts the primary `Window`, pumps `WindowResizeEvent`, stores the requested `WindowConfig::vsync`, and supports `ENGINE_HEADLESS=1`.
- Spec 0004 is implemented: `World` resources, `Events<T>`, `EventReader<T>`, `Res<T>`, and `ResMut<T>` are available.
- ADR 0003 is accepted: the M1-M4 render backend is SDL3 GPU and SDL GPU types must not appear in public engine headers.
- SDL3 from `vcpkg.json` is the only top-level engine dependency used for rendering. Linux M1 explicitly requires the existing SDL3 dependency with its vcpkg `vulkan`, `wayland`, and `x11` features enabled; `vulkan` enables SDL's Vulkan GPU backend, `wayland` enables the Fedora Tier-1 native window path, and `x11` retains fallback/XWayland support. These are SDL feature selections, not new top-level engine libraries.

## Public API

Public headers live under `engine/render/` and include only standard-library and `engine::` types.

```cpp
namespace engine {

struct Color {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

struct ClearColor {
    Color value{0.1F, 0.1F, 0.1F, 1.0F};
};

struct RendererConfig {
    bool debug = false;
};

struct HeadlessRenderConfig {
    Extent2d size{256, 256};
};

struct ReadbackImage {
    Extent2d size{};
    std::uint32_t bytes_per_row = 0;
    // M1 readback format is tightly packed RGBA8_UNORM in linear byte order:
    // byte 0 = R, byte 1 = G, byte 2 = B, byte 3 = A.
    std::vector<std::byte> pixels;
};

enum class FrameStatus {
    Rendered,
    Skipped,
};

class Renderer {
public:
    static Result<Renderer> create_for_window(Window& window,
                                              RendererConfig config = {});
    static Result<Renderer> create_headless(HeadlessRenderConfig config = {},
                                            RendererConfig renderer_config = {});

    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;

    [[nodiscard]] Result<FrameStatus> render_clear(Color color);
    [[nodiscard]] Result<void> resize(Extent2d size);
    [[nodiscard]] Result<ReadbackImage> read_back();

    [[nodiscard]] bool headless() const noexcept;
    [[nodiscard]] Extent2d size() const noexcept;
};

struct RenderPlugin {
    RendererConfig config{};
    HeadlessRenderConfig headless{};
    void build(App& app) const;
};

} // namespace engine
```

All fallible void renderer operations use `Result<void>`. `Result<void>` is supported by the project toolchains and tests; no fallback success marker is part of this spec.

`Texture`, `Buffer`, `ShaderModule`, `Pipeline`, `BindGroup`, `CommandEncoder`, and public multi-step `Frame` APIs are intentionally deferred. They are target renderer concepts from the architecture doc, but clear color does not need them and exposing placeholder handles now would create public API that cannot yet maintain useful lifetime or usage invariants.

For M1, `Renderer::render_clear(Color)` is the only public frame operation. Internally it performs command-buffer acquisition, render target acquisition, clear-pass recording, command-buffer submission, and implicit window presentation on submission.

## Private backend boundary

SDL3 and SDL GPU types may appear only in `.cpp` files. No SDL headers, SDL types, or SDL symbol names may appear in any `.hpp`, including `engine/render/detail/*.hpp`.

Public and private headers must pass this canary:

```sh
grep -rE 'SDL_|SDL_GPU|glm::|spdlog|ImGui' engine --include='*.hpp'
```

Private headers may contain only backend-neutral opaque/PIMPL declarations. Backend `.cpp` files may include `SDL3/SDL.h` and `SDL3/SDL_gpu.h`.

The renderer needs the native SDL window for `SDL_ClaimWindowForGPUDevice`. Spec 0005 therefore allows the smallest internal-only platform ownership seam:

```cpp
namespace engine::detail {

class NativeWindowLease {
public:
    NativeWindowLease() = default;

    [[nodiscard]] void* native_handle() const noexcept;
    [[nodiscard]] bool vsync_requested() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    friend struct WindowAccess;
    // Implementation-defined. Holds shared ownership of the private WindowBackend.
};

struct WindowAccess {
    // Existing accessors remain.
    static NativeWindowLease acquire_native_lease(Window& window) noexcept;
};

} // namespace engine::detail
```

Rules for this seam:

- no SDL headers, SDL types, SDL-named members, or SDL symbol names in headers;
- SDL casts stay in `.cpp` files only;
- exact private representation is implementation-defined;
- the lease keeps the SDL window/backend alive;
- `Renderer` stores the lease for its entire windowed lifetime;
- renderer destruction releases the claimed GPU window before releasing the lease;
- the lease is released after the GPU claim is released and device-owned resources are destroyed;
- non-SDL/headless backends may return an empty lease;
- `RenderPlugin` treats an empty lease as unsupported for windowed rendering and follows the plugin failure policy below;
- headless rendering must not require a native window lease.

This spec allows the smallest private platform ownership change necessary, including changing `Window`'s private backend ownership from unique to shared if needed. Broader platform API changes are not allowed.

## SDL initialization ownership

Windowed mode:

- the native window lease keeps the platform SDL backend and SDL video initialization alive;
- the renderer must not shut down SDL video initialization that is owned by a live platform/window lease.

Headless mode without `PlatformPlugin`:

- renderer initialization must initialize the SDL video subsystem on the main thread before GPU-device creation;
- `Renderer` records whether it owns that SDL initialization;
- `Renderer` releases only initialization that it owns;
- cleanup must not shut down SDL still owned by another live platform/window lease.

Headless SDL initialization is not implicit. The implementation must make ownership visible in the private backend state.

## Ownership and destruction

`Renderer` owns all GPU state and is move-only.

Windowed renderer ownership:

1. SDL initialization lease supplied by the platform window.
2. SDL GPU device.
3. Claimed association between the device and the SDL window.
4. Per-call command buffer and acquired swapchain texture while `render_clear` is executing.

Headless renderer ownership:

1. Optional renderer-owned SDL video initialization.
2. SDL GPU device.
3. Off-screen color target texture.
4. Transfer/readback resources used by `read_back()`.
5. Per-call command buffer while `render_clear` or `read_back` is executing.

Destruction order is explicit:

1. no public frame object exists, so no caller-owned frame can remain active;
2. release/destroy transfer/readback resources;
3. destroy the headless color target, if any;
4. release the claimed window from the GPU device, if any;
5. destroy the GPU device;
6. release the native window lease;
7. release renderer-owned SDL initialization, if any.

The swapchain texture returned by SDL acquisition is not owned by the engine and must never be destroyed by `Renderer`. Command buffers are owned by SDL after acquisition/submission/cancellation rules; after submit or cancel, the engine must treat them as invalid.

Moving a `Renderer` transfers all ownership and leaves the source empty. Copying is forbidden.

## Thread affinity

M1 `Renderer` is thread-affine.

- `Renderer::create_for_window`, windowed `render_clear`, windowed `resize`, and windowed renderer destruction must execute on the thread that created the `Window`.
- A GPU command buffer must be submitted or cancelled on the same thread where it was acquired.
- A headless renderer must be created, used, resized, read back, and destroyed on its creation thread.
- Cross-thread renderer use and parallel render submission are out of scope.
- The private implementation should record the owning thread in debug builds and assert on incorrect use.
- No thread identifiers or synchronization primitives need to appear in the public API.

## Plugin and schedule ordering

Current `App` exposes only `First` and `Update`; it has no `Render` schedule and no custom schedule registry. Spec 0005 must not silently require broad `engine/core` scheduler work.

M1 decision: `RenderPlugin` appends one render-clear system to `Update`. `Schedule` is currently insertion ordered, so for M1:

- `PlatformPlugin` must be added before `RenderPlugin` for windowed rendering;
- `RenderPlugin` must be added after all gameplay/update plugins and systems;
- adding `Update` systems after `RenderPlugin` is unsupported until a real `Render` schedule lands;
- `examples/clear_color` must add `RenderPlugin` last.

Keep a real `Render` schedule out of spec 0005. It is deferred to spec 0008 or a dedicated scheduler/render-lifecycle spec.

## Plugin behavior

`RenderPlugin::build` must:

1. detect duplicate registration before initialization;
2. preserve an existing `ClearColor` resource or insert `ClearColor{}` if missing;
3. create the renderer before registering the clear system;
4. register the render-clear system only after renderer creation succeeds.

Duplicate registration is a programmer error for spec 0005. If a `Renderer` resource already exists, `RenderPlugin::build` logs the duplicate-registration error and asserts in debug builds. In release builds, it returns without replacing the resource or registering another system. A general plugin-uniqueness framework remains deferred.

Initialization failure policy:

- log the returned error;
- call `app.request_exit()`;
- do not insert `Renderer`;
- do not register the render system;
- do not throw.

Runtime render system:

- takes `App&`, `Res<ClearColor>`, `ResMut<Renderer>`, and `EventReader<WindowResizeEvent>`;
- consumes all unread resize events and applies the last relevant size before rendering;
- calls `Renderer::render_clear(clear_color.value)`;
- on resize or render failure, logs the error, requests app exit, and does not throw;
- treats `FrameStatus::Skipped` as a normal non-error state that does not log an error and does not exit.

If `ENGINE_HEADLESS=1`, `RenderPlugin` creates a headless renderer and does not require `PlatformPlugin`. If not headless and no `Window` resource exists during `RenderPlugin::build`, renderer creation fails through the policy above.

## Windowed frame flow

For every `render_clear` call where the windowed renderer is active:

1. Initialize the swapchain texture output to null.
2. Acquire a command buffer.
3. Call `SDL_WaitAndAcquireGPUSwapchainTexture` for the primary window.
4. If the acquisition call returns false:
   - cancel the command buffer with `SDL_CancelGPUCommandBuffer`;
   - invalidate the local command-buffer handle;
   - log the SDL acquisition error;
   - if cancellation also fails, include/log the SDL cancellation error;
   - return `ErrorCode::BackendError`.
5. If the acquisition call returns true with a null swapchain texture:
   - call `SDL_CancelGPUCommandBuffer`;
   - invalidate the local command-buffer handle;
   - return `FrameStatus::Skipped`.
6. If a non-null swapchain texture is acquired, record an empty render pass with one color target, `LOADOP_CLEAR`, `STOREOP_STORE`, and the requested clear color.
7. Submit the command buffer exactly once.
8. Return `FrameStatus::Rendered` after successful submission.

Cancellation is legal only because no swapchain texture was acquired. Once a non-null swapchain texture is acquired, the command buffer must not be cancelled. The backend must ensure it is submitted exactly once even when later recording cleanup is required.

SDL presents an acquired swapchain texture when the command buffer is submitted, so there is no public `present` operation in M1. `Window::swap()` remains a platform no-op for M1 renderer integration.

## Headless frame flow

Headless rendering is selected by `ENGINE_HEADLESS=1` or by explicitly calling `Renderer::create_headless`.

M1 headless target:

- dimensions: `HeadlessRenderConfig::size`, default `256x256`;
- zero dimensions are invalid and return `ErrorCode::InvalidArgument`;
- GPU texture format: prefer `R8G8B8A8_UNORM`; if unavailable for color target plus copy source usage, return `ErrorCode::UnsupportedFormat`;
- CPU readback format: tightly packed RGBA8_UNORM with `bytes_per_row == width * 4`;
- clear colors are clamped to `[0, 1]` and converted to bytes with normal UNORM rounding;
- `read_back()` is synchronous in M1 and blocks until the GPU copy has completed.

For every headless `render_clear` call:

1. Acquire a command buffer.
2. Record the same clear render pass against the off-screen target.
3. Submit the command buffer exactly once.
4. Return `FrameStatus::Rendered`.

`read_back()` copies the current off-screen color target to a transfer buffer, waits for completion, maps it, strips any backend row padding, and returns `ReadbackImage`.

`read_back()` before a successful submitted clear returns `ErrorCode::InvalidArgument`.

## Resize and minimize semantics

- `WindowResizeEvent{0, 0}` and swapchain acquisition returning a null texture are non-fatal skip states.
- `Renderer::resize({0, 0})` records the logical size as minimized/invalid and returns success for windowed renderers. It must not force a swapchain acquisition.
- For non-zero window resize events, the renderer updates its cached size before the next `render_clear`. SDL owns the swapchain internals, so M1 does not manually recreate a swapchain object; the backend re-queries acquired texture size and swapchain format after resize.
- Headless `resize(non_zero)` recreates the off-screen color target and invalidates prior readback contents. Headless `resize({0, 0})` returns `InvalidArgument`.
- If the window is minimized for many frames, the app remains responsive because platform events still pump in `First` and rendering skips in `Update`.

## Vsync and present mode

`WindowConfig::vsync` is the authoritative windowed vsync setting. `RendererConfig` does not contain a vsync field.

Required behavior:

- `WindowConfig::vsync == true`: use SDL's VSYNC present mode.
- `WindowConfig::vsync == false`: request an available non-vsync present mode through SDL support checks.
- If no requested non-vsync present mode is supported, fall back to VSYNC and log a warning.
- Headless rendering ignores vsync.

The requested vsync value is exposed to the renderer internally through `NativeWindowLease::vsync_requested()`.

## Error behavior

Fallible APIs return `Result<T>` and never throw across engine/plugin/backend boundaries. Fallible void APIs return `Result<void>`.

Required mappings:

- GPU device creation fails: `ErrorCode::BackendError` with SDL's error string and backend name.
- No supported GPU backend or required shader format exists: `ErrorCode::UnsupportedFormat`.
- Native window lease is absent for windowed renderer: `ErrorCode::BackendError`.
- Window claim fails: `ErrorCode::BackendError`.
- Command buffer acquisition fails: `ErrorCode::BackendError`.
- Swapchain acquisition returns false: `ErrorCode::BackendError`.
- Swapchain acquisition returns true with null texture: cancel the command buffer and return `FrameStatus::Skipped`, not an error.
- Command-buffer cancellation after null swapchain acquisition fails: log the SDL error and return `ErrorCode::BackendError`.
- Submission fails: `ErrorCode::BackendError`.
- Unsupported headless texture/readback format: `ErrorCode::UnsupportedFormat`.
- Zero-size headless target: `ErrorCode::InvalidArgument`.
- Readback/map/copy failure: `ErrorCode::BackendError`.

Backend errors must be logged at the backend boundary with the original SDL error string, then returned as `engine::Error`.

## Deterministic test strategy

Fast tests:

- public/private header canary: no SDL/SDL GPU/spdlog/GLM/ImGui names in any engine header;
- API/resource tests that do not create a GPU device, tagged `[fast]`.

Slow tests:

- `tests/unit/test_renderer.cpp` creates a headless renderer, clears to red through `render_clear`, reads back, and asserts a center pixel is within tolerance of RGBA `(255, 0, 0, 255)`;
- a resize test clears at `16x16`, resizes headless to `32x8`, clears again, and verifies `ReadbackImage::size` and `bytes_per_row`.
- a headless plugin-ordering integration test registers an `Update` system before `RenderPlugin`, changes `ClearColor` to a known value in that system, adds `RenderPlugin` last, runs one frame headlessly, reads back the target, and verifies the rendered color matches the value written by the earlier `Update` system. This proves ordering without a special renderer test hook.

GPU output should not be treated as byte-perfect across arbitrary hardware. M1 slow tests run only in a deterministic software-rendered CI environment and still compare with a small per-channel tolerance for UNORM conversion and backend differences. For clear-only RGBA8 on pinned llvmpipe the expected tolerance is at most one byte per channel; screenshot tests in spec 0010 may use broader perceptual tolerances.

Required CI environment for slow renderer/screenshot jobs:

```sh
ENGINE_HEADLESS=1
SDL_GPU_DRIVER=vulkan
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
LIBGL_ALWAYS_SOFTWARE=1
MESA_LOADER_DRIVER_OVERRIDE=llvmpipe
```

The CI artifact that proves spec 0005 works is a captured clear-color readback from `examples/clear_color` frame 5 as tightly packed raw RGBA bytes, or a dependency-free PPM file, stored under `build/test-output/` by the test harness. PNG encoding, golden PNGs, and golden-image comparison are owned by spec 0010.

## Example artifact

Add `examples/clear_color/` in the implementation PR.

Minimum behavior:

- windowed mode: add `PlatformPlugin`, insert or preserve `ClearColor` with cornflower blue, add `RenderPlugin` last, run for 60 frames, then exit cleanly;
- headless mode: same app runs without a real window when `ENGINE_HEADLESS=1`;
- no sprites, cameras, shaders, meshes, input UI, PNG encoding, or screenshots beyond the optional raw/PPM frame artifact used by tests.

## Acceptance criteria

- [x] `engine::Renderer`, `RenderPlugin`, `ClearColor`, `Color`, `RendererConfig`, `HeadlessRenderConfig`, `FrameStatus`, and `ReadbackImage` exist with the public behavior defined above.
- [x] Public frame rendering is atomic through `Renderer::render_clear(Color) -> Result<FrameStatus>`; no public `Frame`, `begin_frame`, `clear(Frame, ...)`, `submit`, `present`, or `CommandEncoder` API exists in 0005.
- [x] All fallible void renderer operations use `Result<void>` with no success-marker fallback.
- [x] SDL3 GPU is hidden in `.cpp` implementation files; `grep -rE 'SDL_|SDL_GPU|glm::|spdlog|ImGui' engine --include='*.hpp'` returns no matches.
- [x] Windowed renderer creation claims the SDL window through an internal-only `engine::detail::NativeWindowLease`; the lease keeps the platform backend/window alive for the renderer lifetime.
- [x] Renderer destruction releases the claimed GPU window before releasing the native window lease and releases only SDL initialization it owns.
- [x] Headless renderer creation does not require `PlatformPlugin` or a native window and explicitly owns/releases its SDL video initialization when it initializes SDL itself.
- [x] `Renderer` is thread-affine in M1: windowed renderer operations run on the window-creation thread, headless renderer operations run on the renderer-creation thread, command buffers are submitted/cancelled on their acquisition thread, and debug builds assert on incorrect use.
- [x] Failed windowed swapchain acquisition cancels the command buffer, invalidates the local handle, logs acquisition and cancellation errors as applicable, and returns `BackendError`.
- [x] Skipped windowed frames with a null swapchain texture cancel the command buffer with `SDL_CancelGPUCommandBuffer`, invalidate the local handle, and return `FrameStatus::Skipped`; acquired non-null swapchain command buffers are submitted exactly once and never cancelled.
- [x] `RenderPlugin` appends its render-clear system to `Update`; for M1 it must be added after all gameplay/update systems, and `examples/clear_color` adds it last.
- [x] `RenderPlugin::build` preserves an existing `ClearColor` resource or inserts `ClearColor{}` before registering the render system.
- [x] `RenderPlugin::build` handles initialization failure by logging, requesting app exit, inserting no `Renderer`, and registering no render system.
- [x] Runtime resize/render failures log, request app exit, and do not throw; `FrameStatus::Skipped` is not an error and does not exit.
- [x] Duplicate `RenderPlugin` registration is detected as a programmer error and does not replace the existing `Renderer` or register a second render system.
- [x] `WindowConfig::vsync` controls windowed present mode; non-vsync requests use SDL support checks and fall back to VSYNC with a warning when unsupported.
- [x] Minimized/zero-size swapchain acquisition skips rendering without failing the app.
- [x] Headless `read_back()` returns tightly packed RGBA8 bytes with deterministic dimensions and row pitch.
- [x] Slow headless renderer tests cover clear/readback and headless resize.
- [x] A slow headless plugin-ordering integration test proves an earlier `Update` system can update `ClearColor` before the render-clear system runs.
- [x] `examples/clear_color/` clears to cornflower blue for 60 frames and may emit raw RGBA or dependency-free PPM as the proof artifact.

## Implementation notes

- `Renderer` is move-only and stores a backend-neutral PIMPL; the SDL GPU backend lives in `engine/render/sdl3_gpu_backend.cpp`.
- `NativeWindowLease` keeps shared ownership of the private platform backend and exposes only a `void*` native handle, requested vsync, and creation-thread metadata.
- Headless rendering initializes the SDL video subsystem when needed, selects SDL's offscreen video driver by default in headless mode, and releases only initialization it owns.
- Headless readback uses a download transfer buffer, fence wait, map/unmap, and returns tightly packed RGBA8 data.
- The existing SDL3 dependency is configured with its `vulkan`, `wayland`, and `x11` features in `vcpkg.json`; without the Linux window features, the local SDL 3.4.10 build exposed only `offscreen`, `dummy`, and `evdev` video drivers. After rebuilding SDL with the required features and Toolbox development packages, the compiled driver list is `wayland`, `x11`, `offscreen`, `dummy`, and `evdev`.
- Local real-window smoke in graphical Fedora Kinoite Toolbox succeeds for Wayland and X11 when the Vulkan ICD is pinned to lavapipe. This proves the engine window, swapchain, and present path independently of hardware GPU interop.
- RTX 5090 hardware Vulkan presentation inside the current Toolbox is moved to a dedicated developer-environment/tooling follow-up: https://github.com/arturasan/game-engne-new/issues/14. The default hardware Wayland run still fails before rendering with compositor dmabuf-import diagnostics and no SDL-reported supported SDR swapchain composition; the default hardware X11 run reaches the backend but fails swapchain creation with `vkCreateSwapchainKHR` / `VK_ERROR_INITIALIZATION_FAILED`. This does not block spec 0005 because headless Vulkan rendering/readback passes, Wayland and X11 native-window presentation both pass with lavapipe, SDL Wayland/X11 backend compilation is verified, and the remaining failure is isolated to host/container hardware presentation. Plain process exit code 0 is not treated as smoke success because initialization failure requests a clean app exit.
- Slow Clang ASan renderer validation uses a narrow external-library LeakSanitizer suppression file at `tests/sanitizers/lsan.supp`. The unsuppressed representative headless clear/readback test reports 512 bytes in 2 allocations, both 256 bytes, through SDL 3.4.10's Vulkan backend initialization stacks; both known allocations share the exact stack frame `VULKAN_INTERNAL_DeterminePhysicalDevice`.
- The allowed LeakSanitizer suppression rule is exactly `leak:^VULKAN_INTERNAL_DeterminePhysicalDevice$`. Engine-owned leaks remain fatal; a local canary leaking 123 bytes still fails with this suppression file enabled. Follow-up: https://github.com/arturasan/game-engne-new/issues/13.
- Intentional Bevy differences remain unchanged: no render sub-app, extract schedule, render graph, camera-specific clear color, or public frame/encoder API in this spec.

## Out of scope

- Drawing geometry, sprites, meshes, materials, or lights.
- Cameras and per-camera clear-color overrides.
- Render world, extract schedule, render phases, and render graph.
- A real `Render` schedule in `App`.
- General GPU resource handles: `Texture`, `Buffer`, `ShaderModule`, `Pipeline`, `BindGroup`, `CommandEncoder`.
- Public multi-step frame lifecycle APIs.
- Shader loading or shader format policy beyond device creation accepting the minimum SDL GPU shader flags needed to create the device.
- Multiple windows or surfaces.
- HDR, tonemapping, MSAA, depth/stencil targets, screenshots, PNG encoding, and golden diffing.
- General plugin uniqueness framework.
- New dependencies or CMake/vcpkg changes beyond enabling the existing SDL3 dependency's `vulkan`, `wayland`, and `x11` features for SDL GPU and Linux window presentation on M1.

## Files allowed

- `specs/0005-renderer-clear-color.md`
- `engine/render/**`
- `engine/platform/detail/window_backend.hpp` only for backend-neutral lease/PIMPL declarations and private ownership support
- `engine/platform/detail/sdl3_window_backend.cpp` only for SDL lease acquisition, SDL casts, SDL lifetime wiring, and vsync metadata
- `engine/platform/window.hpp` only for the internal `detail::WindowAccess` lease seam described above, with no SDL names
- `engine/platform/window.cpp` only to forward the internal native-window lease seam and any required private ownership plumbing
- `tests/unit/test_renderer.cpp`
- `examples/clear_color/**`
- CMake/example registration files only if needed to compile the new renderer files and example
- `.github/workflows/ci.yml` only for the required slow SDL GPU test step, renderer artifact upload, Linux SDL Wayland/X11 development packages, and avoiding stale SDL binary-cache restores for those explicit features
- `engine/platform/platform.cpp` only to keep `PlatformPlugin::build` from throwing on backend initialization failure before inserting partial platform resources or systems
- `tests/unit/test_platform.cpp` only for the matching fast regression coverage
- `vcpkg.json` only for enabling the existing SDL3 dependency's `vulkan`, `wayland`, and `x11` features required by SDL's Vulkan GPU backend and Linux M1 window presentation
- `tests/sanitizers/lsan.supp` only for the exact external SDL/Vulkan initialization suppression listed in the implementation notes

## Files forbidden

- `engine/core/**`
- `engine/ecs/**`
- unrelated `engine/platform/**`
- `plugins/**`
- `tests/refs/*.png`
- specs other than 0005, unless implementation discovers a contradiction that must be resolved in its owning spec first

## Bevy grounding notes

References consulted against the pinned baseline `bevyengine/bevy@f667c282dad2c1419afb5836ded22a3ec263970e`:

- `examples/window/clear_color.rs`: Bevy models clear color as a resource, used as the background before drawing; this spec adopts the resource model but does not require cameras yet.
- `crates/bevy_render/src/lib.rs`: Bevy's `RenderPlugin` initializes renderer resources and a separate `RenderApp` with `ExtractSchedule` and `Render`; this spec intentionally defers that architecture because current `App` cannot host it without broader core changes.
- `crates/bevy_render/src/renderer/mod.rs`: Bevy separates device/queue/instance ownership and presents windows after graph execution; this spec adapts the ownership idea to a single SDL GPU device and SDL's submit-presents-swapchain behavior.
- `crates/bevy_render/src/view/window/mod.rs`: Bevy has a window-render integration plugin and extracted window surface state; this spec uses the smaller current engine seam of `Window` resource plus `detail::WindowAccess`.
- `examples/app/headless.rs`: Bevy can run apps without window/render plugins; this spec keeps headless renderer creation independent from `PlatformPlugin`.
- `examples/app/headless_renderer.rs`: Bevy headless rendering uses an off-screen image target and GPU-to-CPU copy with row-pitch handling; this spec adopts a synchronous M1 readback with normalized RGBA8 output.
- `examples/window/screenshot.rs`: Bevy screenshots are a higher-level command/observer workflow; this spec leaves screenshot capture and PNG/golden comparison to spec 0010.

Intentional differences:

- Bevy's render sub-app/extract/render schedules are not implemented in 0005.
- Bevy's clear color is camera-aware; 0005 has only one global clear color because cameras land in specs 0006/0008/0011.
- Bevy uses wgpu; this engine uses SDL3 GPU per ADR 0003.

## SDL GPU grounding notes

Official SDL3 GPU documentation consulted:

- `SDL_CreateGPUDevice`: creates the GPU context from shader-format flags, debug mode, and optional driver name.
- `SDL_GetGPUShaderFormats` and `SDL_GPUShaderFormat`: shader format support is backend-dependent; M1 should not expose this in public API.
- `SDL_ClaimWindowForGPUDevice`: a window must be claimed before acquiring swapchain textures and must be used on the creating thread.
- `SDL_ReleaseWindowFromGPUDevice`: releases the claimed window and destroys SDL's swapchain structure.
- `SDL_SetGPUSwapchainParameters` and `SDL_GPUPresentMode`: VSYNC/SDR is always supported; other modes need support checks.
- `SDL_GetGPUSwapchainTextureFormat`: swapchain format can change if parameters change.
- `SDL_AcquireGPUCommandBuffer` and `SDL_GPUCommandBuffer`: command buffers are SDL-managed and invalid after submit.
- `SDL_CancelGPUCommandBuffer`: cancellation is valid only before a swapchain texture is acquired and invalidates the command buffer.
- `SDL_WaitAndAcquireGPUSwapchainTexture` / `SDL_AcquireGPUSwapchainTexture`: acquired swapchain textures are SDL-owned, tied to the command buffer, auto-presented on submit, and may be null without error.
- `SDL_BeginGPURenderPass`: clearing is modeled as the load operation at the beginning of a render pass.
- `SDL_SubmitGPUCommandBuffer`: submission starts GPU execution and invalidates the command buffer.
- `SDL_CreateGPUTexture`, `SDL_GPUTextureFormat`, and `SDL_GPUTextureSupportsFormat`: headless targets must use supported color-target/copy-source formats.
- `SDL_BeginGPUCopyPass`, `SDL_DownloadFromGPUTexture`, `SDL_GPUTextureTransferInfo`, and `SDL_MapGPUTransferBuffer`: readback is a GPU copy to a transfer buffer followed by mapping; row pitch may need normalization.

## Unresolved questions

No ADR change or human approval is required by this hardened spec.
