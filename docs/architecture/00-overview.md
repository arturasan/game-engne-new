# Architecture overview

## Design pillars

1. **Bevy-inspired.** ECS-first, plugin composition, builder-style App. We are not Bevy; we deliberately re-implement in C++26 during the Fedora-only era to learn and to own our destiny.
2. **No third-party type ever leaks into a public engine header.** Every dependency lives behind an interface in `engine/`. Backends are implemented in `*_backend.cpp` files that pull in the dep. Grep test: `grep -rE 'SDL_|SDL_GPU|glm::|spdlog' engine/**/*.hpp` must return zero hits.
3. **Plugin architecture.** Engine subsystems and game features alike are `Plugin`s. `DefaultPlugins` is just `add_plugin(WindowPlugin{}).add_plugin(InputPlugin{})...`.
4. **2D and 3D share everything possible.** `Transform`, `Camera`, `Material`, `Mesh`, `Visibility`, asset handles. They differ only at the render-phase level.
5. **Determinism is a feature.** Fixed-timestep sim, seeded RNG, frame-hash replay. Pays off in tests, networking, debugging.
6. **Cheap, parallelizable iteration.** Every change costs <10s to verify locally via `cmake --workflow --preset check`.

## Layer diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                              examples/                              │
│        hello_window  sprite_demo  pbr_demo  audio_demo  ...         │
└─────────────────────────────────────────────────────────────────────┘
                                   │
┌─────────────────────────────────────────────────────────────────────┐
│                              plugins/                               │
│   default_plugins   sprite   pbr   audio   tilemap   skinning ...   │
└─────────────────────────────────────────────────────────────────────┘
                                   │
┌─────────────────────────────────────────────────────────────────────┐
│                               engine/                               │
│                                                                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐             │
│  │  core    │  │   ecs    │  │  assets  │  │ scheduler│             │
│  │ App/     │  │ World/   │  │ Handle/  │  │ stdexec  │             │
│  │ Plugin/  │  │ Archetyp │  │ Loader   │  │ wrapper  │             │
│  │ Schedule │  │ Query    │  │          │  │          │             │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘             │
│       └─────────────┴─────────────┴─────────────┘                   │
│                            │                                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐ │
│  │ platform │  │  render  │  │ render2d │  │ render3d │  │  audio │ │
│  │ Window/  │  │ Renderer/│  │ sprite   │  │ mesh,    │  │ Mixer/ │ │
│  │ Input    │  │ Encoder/ │  │ phase    │  │ light,   │  │ Source │ │
│  │          │  │ Shader   │  │          │  │ phase    │  │        │ │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬───┘ │
└──────────────────────────────────────────────────────────────────────┘
       │             │             │             │              │
       ▼             ▼             ▼             ▼              ▼
   ┌──────┐    ┌──────────┐    (shares    (shares     ┌────────────┐
   │ SDL3 │    │ SDL3 GPU │    renderer)  renderer)   │  miniaudio │
   │      │    │ (ADR 03) │                           │            │
   └──────┘    └──────────┘                           └────────────┘
                                  ┌──────────┐    ┌──────────┐
                                  │   GLM    │    │  spdlog  │
                                  │ (math)   │    │ (logging)│
                                  └──────────┘    └──────────┘
```

**Rule:** dependencies only flow downward. No `plugins/` header is `#include`d by anything in `engine/`. No `engine/render*` header is `#include`d by `engine/core` or `engine/ecs`.

## Module responsibilities (one-line each)

| Module | Owns |
|---|---|
| `core` | `App`, `Plugin` concept, `Schedule` (DAG of systems) |
| `ecs` | `World`, archetype storage, query iteration, change detection |
| `assets` | `Handle<T>`, `Assets<T>` storage, `AssetLoader<T>`, hot reload |
| `scheduler` | concurrency primitives, system-parallelism conflict graph |
| `platform` | `Window`, `Input`, `Event`, raw input → semantic events |
| `render` | `Renderer`, `Surface`, `CommandEncoder`, `ShaderModule`, `Pipeline` |
| `render2d` | sprite phase, sprite batching, ortho camera |
| `render3d` | opaque/transparent phases, PBR pipeline, perspective camera |
| `audio` | `Mixer`, `AudioSource`, spatial audio |
| `math` | `vec2/3/4`, `mat4`, `quat`, transform composition |

## Where dependencies are hidden

| Backend dep | Hidden behind | Implementation TU |
|---|---|---|
| SDL3 | `engine/platform/window.hpp`, `engine/platform/input.hpp` | `engine/platform/sdl3_backend.cpp` |
| SDL3 GPU | `engine/render/renderer.hpp`, `engine/render/encoder.hpp` | `engine/render/sdl3_gpu_backend.cpp` |
| miniaudio | `engine/audio/mixer.hpp`, `engine/audio/source.hpp` | `engine/audio/miniaudio_backend.cpp` |
| GLM | `engine/math/*.hpp` (re-exports as `engine::vec3` etc.) | `engine/math/glm_backend.cpp` |
| spdlog | `engine/core/log.hpp` | `engine/core/spdlog_backend.cpp` |
| stdexec | `engine/scheduler/scheduler.hpp` | `engine/scheduler/stdexec_backend.cpp` |
| Box2D v3 | `engine/physics2d/world.hpp` (M4) | `engine/physics2d/box2d_backend.cpp` |
| Jolt | `engine/physics3d/world.hpp` (M4) | `engine/physics3d/jolt_backend.cpp` |
| tinygltf, stb_image | `engine/assets/loader_*.hpp` | `engine/assets/loader_*_backend.cpp` |

## The one place backend types do appear publicly

Material/shader authoring. Pure abstraction kills shader ergonomics. We provide an escape hatch:

```cpp
namespace engine::detail {
template <typename Backend>
auto backend_handle(const ShaderModule& s) -> Backend*;
}
```

This namespace is **never** documented in the public docs. It exists for material plugin authors and the editor.

## Frame loop (top to bottom)

```
App::run() {
    while (!exit_requested) {
        for (schedule in [First, PreUpdate, FixedMain, Update, PostUpdate, Last]):
            schedule.run(world)
        render_world.extract_from(world)         // copy entities marked Visible
        render_world.run(RenderSchedule)         // builds command buffers
        renderer.submit(command_buffer)
        window.present()
        ++frame
    }
}
```

`FixedMain` runs zero or more times per frame to catch the simulation up to a fixed timestep (default 60Hz).

`render_world` is a second `World` populated each frame by "extract" systems from the main world. This separates simulation from rendering and lets them parallelize cleanly.

## Cross-cutting concerns

| Concern | Where it lives |
|---|---|
| Logging | `engine/core/log.hpp` (spdlog hidden). Use `engine::log::info("...")`. |
| Errors | `engine/core/result.hpp` — alias for `std::expected<T, Error>`. No exceptions across module boundaries. |
| Time | `engine/core/time.hpp` — `Time::delta()`, `Time::elapsed()`, fixed-step state. |
| Random | `engine/core/rng.hpp` — seeded PCG; deterministic given seed. |
| Config | `engine/core/config.hpp` — read-only key-value loaded once at startup. |

See `docs/architecture/08-error-handling.md` for the error story in detail.

## What this document does **not** cover

- Per-module API (see the individual `0X-*.md` docs)
- Per-feature task breakdown (see `docs/roadmap.md` and `specs/`)
- Build/CI internals (see `AGENTS.md` and `.github/workflows/ci.yml`)
- Decision history (see `docs/adr/`)
