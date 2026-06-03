# 0007 — Asset system v0

- Owner: TBD
- Status: draft
- Tracking issue: TBD

## Scope

Build the asset system: `Handle<T>`, per-type `Assets<T>` resource, `AssetServer`, and the `AssetLoader<T>` concept. Ship two loaders: `Image` (via `stb_image`) and a raw bytes loader. M1 loading is synchronous; `load<T>` is a blocking shim over `load_sync<T>` until scheduler/thread-pool work defines deterministic async handoff.

## Acceptance criteria

- [ ] `Handle<T>` = `{uint32 id, uint32 gen}` POD, 8 bytes, trivially copyable, hashable.
- [ ] `Assets<T>` resource: `add(T)`, `get(Handle<T>)`, `get_mut(Handle<T>)`, `remove(Handle<T>)`. Generational invalidation.
- [ ] `AssetServer` resource: `load<T>(path) -> Handle<T>` (M1 sync shim; blocks, records state, returns a ready/failed handle), `load_sync<T>(path) -> Result<Handle<T>>` (blocks and reports failure directly).
- [ ] `AssetState { Loading, Loaded, Failed }` queryable via `AssetServer::state(Handle<T>)`.
- [ ] `AssetLoader<T>` concept: `auto load(std::span<const std::byte> bytes, LoadContext& ctx) -> Result<T>`.
- [ ] `LoadContext` allows sub-asset registration (e.g. a glTF loader registering meshes + materials).
- [ ] `Image` loader: PNG, JPEG, BMP via `stb_image`. Returns `engine::Image { width, height, format, bytes }`.
- [ ] `RawBytes` loader: returns `std::vector<std::byte>`.
- [ ] Asset failure replaces with placeholder: pink-checkerboard for `Image`. Failure is logged and observable via `AssetState`.
- [ ] Grep test: `grep -r 'stb_image\|stbi' engine/assets/*.hpp` returns zero hits.
- [ ] Unit tests in `tests/unit/test_assets.cpp` tagged `[fast]` for handle semantics, `[slow]` for actual file loads from `tests/unit/fixtures/`.

## Out of scope

- Hot reload on file change (M2).
- glTF / mesh / audio loaders (later specs).
- Asset packs / virtual filesystem.
- Compression / streaming.
- Network-loaded assets.

## Files not to touch

- `engine/render*` (consumer of `Image` handle).
- `engine/scheduler/*` (true async loading waits for scheduler/thread-pool work).

## Notes for the implementing agent

- Read `docs/architecture/05-assets.md`. The `LoadContext` shape there is the contract.
- Do not create a hand-rolled loader thread in M1. `load<T>` may keep the future async API shape, but it must perform the same synchronous work as `load_sync<T>`.
- True async loading is deferred until the scheduler/thread-pool spec exists. That future implementation must populate `Assets<T>` on the **main thread** via a deterministic handoff point; worker threads must never write to `Assets<T>` directly.
- Fixture files live in `tests/unit/fixtures/test_assets/`. Keep them tiny (≤4×4 PNGs).
