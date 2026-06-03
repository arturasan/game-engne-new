# 0008 — Sprite plugin

- Owner: TBD
- Status: draft
- Tracking issue: TBD

## Scope

First end-to-end visible feature: `plugins/sprite/` draws textured quads on screen. Wires together `Transform` (0006), `Image` asset (0007), `Renderer` (0005), and the render world / extract step (0004 events + new code).

## Acceptance criteria

- [ ] `Sprite { Handle<Image> texture; vec2 size; vec4 color; vec2 anchor; }`.
- [ ] `SpriteBundle` aggregates `Sprite + Transform + GlobalTransform + Visibility`.
- [ ] `SpritePlugin::build(App&)`:
  - Registers `Sprite` component.
  - Adds `extract_sprites_system` in the engine-internal `Extract` schedule.
  - Adds `Phase2dSprite` to the render phase set.
  - Loads + registers the built-in WGSL sprite shader.
- [ ] `RenderWorld` (introduced minimally here) has its own `World` and is reset/repopulated each frame by extract systems.
- [ ] Batching: contiguous sprites with the same texture are drawn in one draw call (instanced quad).
- [ ] Sort key: `(z_layer, y_position)` — back-to-front for transparency.
- [ ] `examples/sprite_demo/` renders 100 sprites across a checker texture, camera scrolling with arrow keys.
- [ ] Screenshot test `tests/refs/sprite_demo_frame_30.png` matches (within perceptual tolerance). Tagged `[slow]`.
- [ ] Unit test `tests/unit/test_sprite.cpp` tagged `[fast]`: extract system pulls only entities with `Sprite + Visibility::visible`.

## Out of scope

- Sprite atlases / texture arrays — sprites are one-texture-per-draw in v0.
- 9-slice / nine-patch sprites.
- Sprite animation (frame stepping) — example-level concern.
- Custom sprite materials — only the built-in shader for v0.
- 3D rendering — spec 0011.

## Files not to touch

- `engine/render*` public headers — extend in-place if needed but no new public types.
- `engine/core/*`, `engine/ecs/*`.

## Notes for the implementing agent

- Read `docs/architecture/04-rendering.md` on extract + render world.
- The built-in sprite shader lives in `plugins/sprite/shaders/sprite.wgsl`. Embed at compile time via a CMake `configure_file` that turns it into a `const char[]` in a `.cpp`.
- Use instanced quads: one vertex buffer (4 verts), one instance buffer (per-sprite transform + UV + color), `draw_indexed_instanced`.
- The RenderWorld is a `World` instance owned by `Renderer`. Don't expose it as a public type; expose only `RenderWorld&` to extract systems via system param.
- Screenshot determinism: pin to SwiftShader in CI. See `docs/architecture/07-testing.md`.
