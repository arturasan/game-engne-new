# 0011 — PBR mesh plugin

- Owner: TBD
- Status: draft
- Tracking issue: TBD

## Scope

Add 3D rendering: `plugins/pbr/` renders triangle meshes with a physically-based shader. Introduces `Mesh`, `Material`, `StandardMaterial`, glTF loader, `Phase3dOpaque` + `Phase3dTransparent` phases.

## Acceptance criteria

- [ ] `engine::Mesh` asset: vertex positions, normals, UVs, tangents, indices. Interleaved layout (see ADR 0011 if it exists; otherwise file one).
- [ ] `engine::Material` opaque handle; `StandardMaterial { base_color, metallic, roughness, base_color_texture, normal_texture, ... }`.
- [ ] `MeshBundle` aggregates `Handle<Mesh> + Handle<Material> + Transform + GlobalTransform + Visibility`.
- [ ] `PbrPlugin::build(App&)` registers components, extract system, both 3D phases, and the built-in PBR shader.
- [ ] glTF 2.0 loader (via `tinygltf`) loads `.gltf` and `.glb` into `Mesh` + `StandardMaterial` + texture assets via `LoadContext` (spec 0007).
- [ ] Directional light component `DirectionalLight { color, illuminance, direction }`. One per scene for v0.
- [ ] Camera setup: `Camera3d` from spec 0006; perspective projection.
- [ ] Sort: opaque front-to-back, transparent back-to-front.
- [ ] `examples/pbr_demo/` renders a lit textured cube + a glTF damaged helmet. Orbits the camera.
- [ ] Screenshot test for `pbr_demo` frame 30. Tagged `[slow]`.
- [ ] Grep test: `grep -r 'tinygltf' engine/**/*.hpp plugins/pbr/*.hpp` returns zero hits.

## Out of scope

- Shadows (M3).
- IBL / environment maps (M3).
- Skinned meshes / animation (later).
- Multiple lights (one directional is the v0 limit).
- Custom materials beyond `StandardMaterial`.
- glTF animations, morph targets, sparse accessors.
- KTX2 / DDS texture formats — PNG only via spec 0007.

## Files not to touch

- `plugins/sprite/*` — separate plugin, separate concerns.
- `engine/render*` public surface — phases register via existing extension points only.

## Notes for the implementing agent

- File an ADR for mesh data layout (interleaved vs SoA) before coding. The choice has perf and ergonomic tradeoffs.
- The PBR shader (`plugins/pbr/shaders/pbr.wgsl`) is non-trivial. Reference Bevy's `pbr.wgsl` and the glTF 2.0 BRDF spec; do not copy verbatim. Document derivations.
- glTF accessor → vertex buffer conversion: keep this in `plugins/pbr/detail/gltf_loader.cpp`. Use `tinygltf` headers there; never in a public header.
- HDR pipeline: linear-RGB intermediate render target, tonemap to sRGB swapchain in a final pass. Tag this as a TODO if not yet wired in the renderer — minimum viable is direct sRGB output.
- The damaged helmet glTF is the standard test asset; download it via CMake `FetchContent` into `examples/pbr_demo/assets/` (gitignored).
