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

**Asynchronous (the default):**

```cpp
auto handle = world.resource<AssetServer>().load<Image>("textures/player.png");
// returns immediately. handle is valid; the underlying Image is loaded
// on a worker thread and populated when ready.
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

`load_sync` returns a `Result<Handle<T>>`. `load` always returns a `Handle<T>` — query its state via the asset server.

## Asset state

```cpp
auto state = world.resource<AssetServer>().state(handle);
switch (state) {
case AssetState::Loading: ...
case AssetState::Loaded:  ...
case AssetState::Failed:  ...
}
```

A typical pattern: spawn an entity with a sprite handle immediately, and let the sprite renderer draw a **placeholder** (pink checkerboard) until the asset finishes loading. No special-casing required at the call site.

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

Loaders run on `TaskPool::io`. The result is shipped back to the main thread and inserted into `Assets<T>` at a safe point.

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
| Loader threads       | `TaskPool::io`                    |
| Placeholder data     | hard-coded fallback inside `Assets<T>` (pink checker / silent audio) |

## Hot reload (M3)

A file watcher on the asset root will turn file-modified events into `AssetEvent::Modified { handle }` events. Systems that care subscribe via `EventReader<AssetEvent>`. The handle stays the same; only the data behind it changes. This is why everything passes handles around, not data.

---

**Bevy mapping:** `Handle<T>`, `Assets<T>`, `AssetServer`, `AssetLoader<T>` ↔ identically named Bevy types. The async-by-default + placeholder pattern is the same. Bevy's `AssetEvent` is also our M3 plan.
