# Rendering — Renderer, render world, 2D + 3D

## Scope

The rendering layer. Abstracts a modern explicit-API GPU backend (**SDL3 GPU is the initial implementation** per ADR 0003, with Vulkan / D3D12 / Metal under it; a raw-Vulkan backend is available as a swap-in if SDL3 GPU's feature ceiling becomes the blocker). Defines a **render world** decoupled from the main world, **render phases** that share a unified pipeline, and the seams 2D and 3D plugins hook into.

## Public API (target shape)

```cpp
namespace engine {

// ----- Opaque backend handles -----
class Surface;            // owned by Renderer; one per Window (M1)
class Texture;
class Buffer;
class ShaderModule;
class Pipeline;
class BindGroup;
class CommandEncoder;

// ----- Renderer -----
class Renderer {
public:
    static Result<Renderer> create(const SurfaceDescriptor& sd);

    // Resource creation. All return owning handles; lifetimes tied to Renderer.
    Result<Texture>     create_texture(const TextureDesc&);
    Result<Buffer>      create_buffer(const BufferDesc&);
    Result<ShaderModule> create_shader(std::span<const std::byte> spirv);
    Result<Pipeline>    create_pipeline(const PipelineDesc&);
    Result<BindGroup>   create_bind_group(const BindGroupDesc&);

    // Frame lifecycle.
    CommandEncoder begin_frame();
    void           submit(CommandEncoder&&);
    void           present();
    void           resize(int width, int height);
};

// ----- Per-frame extracted view of the world -----
class RenderWorld;        // a second World; populated by extract systems

// ----- Phase tag types -----
struct Phase2dSprite     {};
struct Phase3dOpaque     {};
struct Phase3dTransparent{};
struct PhaseUi           {};
struct PhasePostProcess  {};

}  // namespace engine
```

## Frame flow

```
Update      — main world systems write components (Transform, Sprite, Mesh, Material, ...)
PostUpdate  — transform propagation, frustum cull → mark Visible
[Extract]   — engine-internal: copy (Transform, Material handles, Mesh handles, ...)
              into the RenderWorld. Allocates only on first appearance, then in-place updates.
[Render]    — RenderWorld systems run in phase order:
                 1. extract camera params
                 2. for each phase: batch, sort, record draw calls into the encoder
                 3. composite phases into the final image
              Renderer.submit(encoder)
              Window.present()
```

The main world is **never** touched during render. This is what lets us parallelize sim + render in M3.

## Why a render world?

- **Determinism + reproducibility:** the simulation owns truth; render can drop frames without leaking state.
- **Parallelism:** while frame N renders, frame N+1 simulates. Possible because the render world is a copy.
- **Backend-isolation:** the render world is where backend types are allowed (in `engine::detail::`). The main world stays pure.

## 2D + 3D unification

Both share:

- `Transform`, `GlobalTransform` (PostUpdate propagates parent → child)
- `Camera`, with `Camera2d` / `Camera3d` marker components selecting the projection
- `Visibility` + `ComputedVisibility`
- `Handle<Mesh>`, `Handle<Material>`, `Handle<Texture>`
- The same `Renderer`, `CommandEncoder`, `Pipeline`

They differ in:

- **Render phase** picked up by the extract system (a sprite goes to `Phase2dSprite`, a PBR mesh to `Phase3dOpaque`).
- **Material shader** (sprite shader vs PBR shader, both built on the same `ShaderModule` API).
- **Sort key** (2D: layer + y-pos; 3D opaque: front-to-back; 3D transparent: back-to-front).

A `Sprite` is just a quad mesh + a sprite material. A `MeshBundle` is a mesh + PBR material. No special-casing in the renderer for 2D vs 3D — phases are the only switch.

## Shader authoring (the one approved leak)

```cpp
// Pure abstraction kills shader ergonomics. We allow this:
namespace engine::detail {
    template <typename Backend>
    Backend* backend_handle(const Pipeline&);
}
```

A material plugin authoring a custom shader passes **SPIR-V bytecode** to `engine::create_shader(spirv_bytes)` (HLSL or GLSL → SPIR-V via DXC/glslang at build time). SDL3 GPU cross-compiles SPIR-V to the platform-native bytecode internally. Authors **do not** see `SDL_GPU*` types in their public API.

If a plugin needs to do something the engine doesn't yet support (e.g. a custom binding layout), it can reach into `engine::detail::backend_handle` — but this is undocumented and considered a bug in the engine's surface area, to be fixed by extending the abstract API.

## Render graph (M2)

For M1, the render is a fixed linear sequence of phases. M2 introduces a **render graph**:

- Nodes = render passes (shadow, gbuffer, light, post-tonemap, ui)
- Edges = resource dependencies (texture A → pass B reads A → pass C writes A')
- Topological execution, transient resource allocator reuses memory between non-overlapping passes

The phase types above are pre-existing node identities for the M2 graph.

## Headless render (CI)

When `ENGINE_HEADLESS=1`, the renderer:

- Creates an off-screen color target (no swap chain).
- Returns the rendered image as raw bytes for screenshot diffing (`engine::Renderer::read_back(target)`).
- Skips `present()`.

This makes screenshot tests run in any environment, but **must** be pinned to a software rasterizer (llvmpipe on Linux, Vulkan SwiftShader cross-platform) for deterministic outputs — see `docs/architecture/07-testing.md`.

## Decisions & alternatives

| Decision | Rationale | Rejected |
|---|---|---|
| SDL3 GPU backend (ADR 0003) | Zero new deps (SDL3 already required), cross-platform Vulkan/D3D12/Metal, smaller surface than wgpu | wgpu-native (Rust toolchain cost, WebGPU spec ceiling); Diligent / The Forge (large deps not yet justified); raw Vulkan (worth it later, not now) |
| Separate render world | Decouples sim from render, parallelizable | Single world (couples GPU lifetimes to gameplay code) |
| Phase tag types | Compile-time-checked, IDE-discoverable | String phases (Bevy's old approach) |
| Unify 2D + 3D under one Renderer | Share infra, avoid duplicate transform/camera/material code | Separate `Renderer2D` and `Renderer3D` (more code, less leverage) |
| SPIR-V as the shipped shader format | Universal, tooling-rich (DXC/glslang), SDL3 GPU consumes it natively | WGSL (tied to wgpu); writing our own preprocessor (out of scope) |

## Open questions

- Mesh data layout — interleaved vs SoA? Decision deferred to spec 0011.
- Bind group caching strategy — explicit user control vs auto? Decision deferred to M2.
- HDR pipeline — default linear-RGB intermediate? Yes for M2; sRGB swapchain at present.
