# 0007 — Asset system v0

- Owner: TBD
- Status: draft
- Tracking issue: TBD

## Scope

Build the asset system: `Handle<T>`, per-type `Assets<T>` resource, `AssetServer`, and the `AssetLoader<T>` concept. Ship two loaders: `Image` (via `stb_image`) and a raw bytes loader. Asynchronous loads happen on `TaskPool::io`.

## Acceptance criteria

- [ ] `Handle<T>` = `{uint32 id, uint32 gen}` POD, 8 bytes, trivially copyable, hashable.
- [ ] `Assets<T>` resource: `add(T)`, `get(Handle<T>)`, `get_mut(Handle<T>)`, `remove(Handle<T>)`. Generational invalidation.
- [ ] `AssetServer` resource: `load<T>(path) -> Handle<T>` (returns immediately; populates `Assets<T>` later), `load_sync<T>(path) -> Result<Handle<T>>` (blocks).
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
- `engine/scheduler/*` (consumer of `TaskPool`).

## Notes for the implementing agent

- Read `docs/architecture/05-assets.md`. The `LoadContext` shape there is the contract.
- `TaskPool::io` is wired in spec 0004 of M2 if not present; for M1, a hand-rolled `std::thread` per load is acceptable — leave a TODO with the issue link.
- The async `load<T>` populates `Assets<T>` on the **main thread** via a deferred command queue. Don't write to `Assets<T>` from worker threads — too much locking, and breaks the determinism contract.
- Fixture files live in `tests/unit/fixtures/test_assets/`. Keep them tiny (≤4×4 PNGs).
