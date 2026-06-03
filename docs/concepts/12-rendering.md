# 12 — Rendering mental model

Rendering is the one area where the engine has **two worlds**. This doc explains why and how systems move data between them.

If you only ever spawn sprites and never touch shaders, you can stay in the main world and ignore the rest of this doc. The split matters when you write extract systems, custom render phases, or new material types.

## The two worlds

```
   main world                            render world
 ┌──────────────┐                      ┌──────────────┐
 │ Transform    │   ── extract ──>     │ ExtractedSprite
 │ Sprite       │   each frame         │ ExtractedMesh │
 │ Velocity     │                      │ ViewUniforms  │
 │ Player       │                      │ ...           │
 │ ...          │                      │               │
 └──────────────┘                      └──────────────┘
       │                                      │
   gameplay runs here                  GPU work runs here
   (Update, PostUpdate)                (Extract, Render)
```

**Main world** holds gameplay state. Components on entities, the things players think about: position, health, sprite handles. Sim systems run here.

**Render world** holds GPU-shaped data. A flat list of "drawables" with their backend buffer offsets, view matrices, sort keys. Render systems run here.

The two worlds are independent `World` instances. They share types where it's convenient (`Transform`, `Camera`), but the entities are not the same entities. A sprite in main world produces an `ExtractedSprite` in render world each frame.

## The frame, in render terms

```
1. main world sim runs    (Update, PostUpdate)
2. extract                — copy needed data into render world
3. render world resets    — drop last frame's extracted entities
4. render phases run      — per phase: gather, sort, batch, draw
5. submit                 — send command encoder to GPU
6. present                — swap-chain → screen
```

Steps 2–6 happen entirely inside the engine. From a plugin author's perspective: register an extract system that puts your component's data into the render world, register a render phase that knows how to draw it. The frame loop wires the rest.

## Why two worlds?

Three concrete wins:

1. **Determinism.** Sim is the source of truth. Render can drop a frame, jitter, run at a different rate — none of it leaks back into gameplay.
2. **Parallelism (M3).** While GPU renders frame N, CPU is free to sim frame N+1. Possible only because render holds its own copy.
3. **Backend isolation.** Render-world components hold GPU buffer offsets, pipeline ids, etc. (today: SDL3 GPU handles per ADR 0003; swappable later). Those types are forbidden in the main world. Concentrating them in the render world keeps the abstraction rule (no third-party types in public engine headers) intact.

The cost: a memcpy per frame for the extracted set. With instanced sprites, it's a few bytes per sprite. Worth it.

## Extract systems

An extract system reads from main world and writes to render world. It runs after `Last` and before the render phases.

```cpp
void extract_sprites(
    const World&  main,
    World&        render,
    Query<const Sprite, const GlobalTransform, const Visibility> main_q)
{
    render.clear_extracted<ExtractedSprite>();
    for (auto [entity, sprite, xform, vis] : main_q) {
        if (!vis.visible) continue;
        render.spawn(ExtractedSprite{
            .texture = sprite.texture,
            .matrix  = xform.matrix,
            .color   = sprite.color,
            .sort_key = compute_sort_key(xform.translation, sprite.layer),
        });
    }
}
```

What's true:

- Extract systems read main world, write render world. Never the other way.
- They run in a special schedule (`Extract`) that the engine drives.
- They are short. They copy, transform shape, project shape. They do not compute lighting or sort — that's the render phase.

## Render phases

A **phase** is a strongly-typed bucket of draw work. Phases run in a fixed order:

```
Phase2dSprite      — opaque + alpha-blended sprites
Phase3dOpaque      — opaque PBR
Phase3dTransparent — transparent PBR (back-to-front)
PhaseUi            — overlays
PhasePostProcess   — tonemap, bloom, …
```

Each phase has its own sort key:

- Sprite: `(layer, y)` for painter's algorithm.
- 3D opaque: front-to-back (early-Z).
- 3D transparent: back-to-front (alpha correctness).
- UI: explicit z-index.

A render-phase system:

1. Pulls extracted entities of its phase type out of the render world.
2. Sorts them.
3. Batches contiguous draws with the same material/texture.
4. Records draw calls into the frame's `CommandEncoder`.

You write a new phase when you add a fundamentally new render category. You write a new **draw function within an existing phase** when you add a material variant. Most plugins do the latter.

## Cameras

A camera is a main-world entity with `Camera` + `Transform`. The extract step copies relevant cameras into the render world. Each render phase iterates render-world cameras and renders the scene from each.

Multi-camera (split-screen, mini-map, render-to-texture) lands in M2. M1 has exactly one main camera, marked with `MainCamera`.

## When you can ignore all of this

If you spawn entities with `SpriteBundle` or `MeshBundle` and never touch a shader, the existing plugins (`SpritePlugin`, `PbrPlugin`) handle extract and render for you. Most game code never sees the render world.

You start caring when:

- You write a custom material (custom SPIR-V shader bytecode, custom pipeline).
- You add a new visual primitive (lines, particles, decals).
- You optimise (skip extract for off-screen entities, custom sort).

For those, read `docs/architecture/04-rendering.md` for the exact API and `plugins/sprite/` for the smallest worked example.

---

**Bevy mapping:** main world + render world + extract step ↔ Bevy's `MainWorld` + `RenderApp` + extract schedule (introduced in 0.6, refined since). Phase types ↔ Bevy's `RenderPhase<I>`. The "extract is a memcpy" mental model is the same.
