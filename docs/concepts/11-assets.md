# 11 — Assets

An **asset** is data loaded from somewhere outside the program: an image, a mesh, a shader, an audio clip, a glTF scene.

The engine doesn't pass assets around by value. It passes **handles** to them. The asset data lives in a per-type `Assets<T>` storage, owned by the world.

## The handle

```cpp
template <typename T>
struct Handle {
    std::uint32_t id;
    std::uint32_t gen;
    // operator==, hashable, trivially copyable.
};
```

A `Handle<Image>` is 8 bytes. You copy it freely, put it on components, store it in resources. The image itself lives once in `Assets<Image>`.

The generation lets the system safely detect "this asset was unloaded and another loaded into the same slot" — old handles fail `Assets<T>::get(h)` cleanly.

## Two ways to load

**Default load API (M1 sync shim, later async):**

```cpp
auto handle = world.resource<AssetServer>().load<Image>("textures/player.png");
// M1: blocks like load_sync, records AssetState, and returns the handle.
// Later: may return immediately once scheduler/thread-pool handoff exists.
```

**Synchronous (tests, examples, hot paths where you must block):**

```cpp
auto result = world.resource<AssetServer>().load_sync<Image>("textures/player.png");
if (!result) {
    log::error("failed: {}", result.error().message);
    return;
}
auto handle = *result;
```

`load_sync` returns a `Result<Handle<T>>`. `load` always returns a `Handle<T>` and records state via the asset server. In M1 it is not truly asynchronous.

## Asset state

```cpp
auto state = world.resource<AssetServer>().state(handle);
switch (state) {
case AssetState::Loading: ...
case AssetState::Loaded:  ...
case AssetState::Failed:  ...
}
```

A typical future pattern: spawn an entity with a sprite handle immediately, and let the sprite renderer draw a **placeholder** (pink checkerboard) until the asset finishes loading. In M1 the same state API exists, but loading completes during the blocking call.

## Resolving handles to data

```cpp
void render_sprites(Res<Assets<Image>> images, Query<const Sprite> q) {
    for (const auto& [_, s] : q) {
        const Image* img = images->get(s.texture);
        if (!img) continue;            // still loading or failed
        // use img->bytes, img->width, img->height ...
    }
}
```

`Assets<T>::get(handle)` returns `const T*`. Mutable access via `get_mut` is rare; most asset data is read-only after load.

## Loaders

Adding a new asset type means writing an `AssetLoader<T>`:

```cpp
struct ImageLoader {
    static constexpr std::array extensions = { "png", "jpg", "bmp" };

    engine::Result<Image> load(std::span<const std::byte> bytes,
                               engine::LoadContext& ctx) {
        // decode bytes (e.g. stbi_load_from_memory), return Image{...}.
        // call ctx.add_sub_asset<T>(name, value) for sub-assets if any.
    }
};

// Register once in a plugin:
app.add_asset_loader<Image>(ImageLoader{});
```

M1 loaders run synchronously on the calling thread. Later async loaders will run on the scheduler/thread-pool infrastructure and ship results back to the main thread at a deterministic safe point.

## Sub-assets

A glTF file produces meshes, materials, and textures all at once. The loader uses `LoadContext` to register each:

```cpp
auto root = ctx.add_sub_asset<Scene>("scene", Scene{...});
auto mesh = ctx.add_sub_asset<Mesh>("nodes/0/mesh", Mesh{...});
```

The caller of `load<Scene>("foo.gltf")` gets the root handle; named sub-assets are accessible by path-with-fragment (`load<Mesh>("foo.gltf#nodes/0/mesh")`).

## What lives where

| Path                 | Lives in                          |
|----------------------|-----------------------------------|
| `Handle<T>`          | small POD, copy anywhere          |
| `Assets<T>`          | resource on `World`               |
| `AssetServer`        | resource on `World`               |
| Loader execution     | calling thread in M1; scheduler/thread pool later |
| Placeholder data     | hard-coded fallback inside `Assets<T>` (pink checker / silent audio) |

## Hot reload (M3)

A file watcher on the asset root will turn file-modified events into `AssetEvent::Modified { handle }` events. Systems that care subscribe via `EventReader<AssetEvent>`. The handle stays the same; only the data behind it changes. This is why everything passes handles around, not data.

---

**Bevy mapping:** `Handle<T>`, `Assets<T>`, `AssetServer`, `AssetLoader<T>` ↔ identically named Bevy types. The placeholder/state pattern is the same; true async loading waits until this engine has scheduler/thread-pool support. Bevy's `AssetEvent` is also our M3 plan.
