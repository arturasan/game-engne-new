# 0006 — Transform + Camera

- Owner: TBD
- Status: draft
- Tracking issue: TBD

## Scope

Add the math layer (`engine/math/`) wrapping GLM, and the core spatial components: `Transform`, `GlobalTransform`, `Parent`, `Children`, `Camera`, `Camera2d`, `Camera3d`. Implement transform propagation as a `PostUpdate` system.

## Acceptance criteria

- [ ] `engine::vec2/vec3/vec4`, `quat`, `mat3/mat4` exposed in `engine/math/types.hpp`. GLM is the implementation; no `glm::` appears in any public header.
- [ ] Grep test: `grep -r 'glm::' engine/**/*.hpp` returns zero hits.
- [ ] `Transform { vec3 translation; quat rotation; vec3 scale; }` with helpers: `from_xyz`, `looking_at`, `compute_matrix() -> mat4`.
- [ ] `GlobalTransform` is a derived `mat4` written by the propagation system. Never user-edited.
- [ ] `Parent { Entity }` + `Children { vector<Entity> }` form the scene hierarchy. Spawning with `Parent` and no `Children` on the parent fixes the parent's `Children` next propagation step.
- [ ] `transform_propagate_system` runs in `PostUpdate`, deterministic order, O(N).
- [ ] `Camera { mat4 projection; }`, `Camera2d { f32 scale; }`, `Camera3d { f32 fov; f32 near; f32 far; }`.
- [ ] Active camera selected by `MainCamera` marker component (later replaced by per-window targeting).
- [ ] Unit tests in `tests/unit/test_transform.cpp` tagged `[fast]`: parent-child propagation, deep hierarchies (5 levels), reparenting, scale composition.

## Out of scope

- Skeletal animation transforms.
- Frustum culling — spec 0008 adds a stub `Visibility` component; real culling is M2.
- Camera controllers (orbit, fly) — example-level code, not engine.
- Decomposing arbitrary matrices back into TRS.

## Files not to touch

- `engine/render*` (consumer of `GlobalTransform` and `Camera`, not editor).
- `plugins/*`.

## Notes for the implementing agent

- GLM uses column-major matrices. Document this in `engine/math/types.hpp`. Don't switch conventions.
- Wrap GLM types via thin `engine::vec*`, `engine::quat`, and `engine::mat*` value types. Any conversion to GLM belongs in private backend/detail implementation files; public headers must not include GLM headers, forward declarations, or GLM-named aliases.
- The trickiest review item: ensure `engine::mat4 * engine::mat4` does not regress to a GLM op on the binary. Use a small benchmark in `tests/unit/test_transform.cpp` to confirm SIMD codegen with `-O2`.
- Read `docs/architecture/04-rendering.md` for how `GlobalTransform` and `Camera` are consumed by extract systems.
