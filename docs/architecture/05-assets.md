# Asset system — Handle, Assets, Loader, hot reload

## Scope

Load files from disk (or other sources) into typed runtime objects, give them stable handles that survive reloads, and notify consumers when an asset changes.

## Public API (target shape)

```cpp
namespace engine {

template <typename T>
struct Handle {
    std::uint32_t id;
    std::uint32_t gen;
    constexpr auto operator<=>(const Handle&) const = default;
};

template <typename T>
class Assets {
public:
    Handle<T> add(T value);
    T*        get(Handle<T> h);
    const T*  get(Handle<T> h) const;
    bool      remove(Handle<T> h);

    // Iteration (for the renderer's extract step).
    auto begin();
    auto end();
};

class AssetServer {
public:
    // Returns a Handle immediately; load happens on a worker thread.
    template <typename T>
    Handle<T> load(std::string_view path);

    // Synchronous load (M1). Blocks the calling thread.
    template <typename T>
    Result<Handle<T>> load_sync(std::string_view path);

    // Register a loader for a (extension, T) pair.
    template <typename T>
    void register_loader(std::string_view extension, std::unique_ptr<AssetLoader<T>>);
};

template <typename T>
class AssetLoader {
public:
    virtual ~AssetLoader() = default;
    virtual Result<T> load(std::span<const std::byte> bytes, LoadContext&) = 0;
};

template <typename T>
struct AssetEvent {
    enum class Kind { Added, Modified, Removed };
    Handle<T> handle;
    Kind      kind;
};

}  // namespace engine
```

Example (M1, sync):

```cpp
auto h = world.resource<AssetServer>().load_sync<Image>("textures/player.png").value();
auto& img = *world.resource<Assets<Image>>().get(h);
```

## M1 vs later

| Capability | M1 | Later |
|---|---|---|
| Sync load | yes | yes |
| Async load (returns Handle immediately) | API shape only | M2 (coroutines + thread pool) |
| Hot reload (file watcher) | no | M3 |
| Sub-assets (e.g. materials inside glTF) | API shape only | M2 |
| Streaming (LOD, partial load) | no | M6 |

The M1 implementation calls the loader synchronously inside `load_sync`. The async `load` returns immediately with a Handle pointing to a placeholder; in M1 it transparently forwards to `load_sync`. This lets call sites be written for the final async API today without M1 needing the thread pool.

## Handle semantics

- Returned by reference into `Assets<T>` storage. Storage is internally a `std::vector<std::optional<T>>` keyed by `id`.
- `gen` bumps on `remove` (same slot-reuse model as ECS Entity). Stale handles fail `get()` cleanly (return `nullptr`).
- Handles are POD, trivially copyable, cheap to pass around.
- Two strong references to the same `Handle<T>` keep the asset alive; when both are dropped, the asset is **not** automatically freed (M1 has no refcount). Reference counting is M3 (needed for hot reload to swap atomically).

## Loader pattern

A loader takes raw bytes (the engine handles I/O) and returns a `T` (or sub-assets via `LoadContext`):

```cpp
class PngLoader final : public AssetLoader<Image> {
public:
    Result<Image> load(std::span<const std::byte> bytes, LoadContext&) override {
        int w, h, channels;
        auto* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes.data()),
            static_cast<int>(bytes.size()), &w, &h, &channels, 4);
        if (!pixels) return Err{ "PNG decode failed" };
        Image img{ .width = w, .height = h, .pixels = ... };
        stbi_image_free(pixels);
        return img;
    }
};
```

`stb_image` is included **only** in `loader_png_backend.cpp`. The header has no `stbi_*` references.

## LoadContext (sub-assets)

```cpp
class LoadContext {
public:
    template <typename U>
    Handle<U> add_subasset(std::string_view name, U value);

    // Path resolution for assets referenced by the file (e.g. .gltf -> .bin).
    std::filesystem::path resolve(std::string_view relative) const;
};
```

A glTF loader produces an asset graph: mesh, materials, textures, animations — each with its own handle, all linked from the root.

## Path resolution

- Asset paths are **virtual**, e.g. `"textures/player.png"`.
- The `AssetServer` resolves against registered roots; default root is `./assets/` relative to the executable.
- Absolute paths are rejected.
- Path separators are normalized to `/` regardless of platform.

This abstraction lets us add archive backends (zip, Bevy-style `.pkg`) without touching any loader.

## Hot reload (M3 preview)

```
file watcher → debouncer → AssetServer.reload(handle)
                             │
                             ▼
                    loader runs on new bytes
                             │
                             ▼
                  Assets<T>.replace(handle, new_T)
                             │
                             ▼
                  emit AssetEvent::Modified{handle}
                             │
                             ▼
                  systems reading AssetEvent re-bind
                  (e.g. material rebuilds bind groups)
```

Atomicity requires reference counts; until then, hot reload is a known-unsafe operation only used in development.

## Decisions & alternatives

| Decision | Rationale | Rejected |
|---|---|---|
| `Handle<T>` is POD `{id,gen}`, not ref-counted in M1 | Simplicity now; refcount added when needed for hot reload | Always `shared_ptr<T>`-like (more overhead, harder to serialize) |
| Loaders take bytes, not paths | Engine owns I/O, loader is pure CPU work, easy to test | Loaders open files themselves (couples I/O to format code) |
| Async `load` exists from day one as a sync shim | Call sites written for the final API; no churn later | Add async API in M2 (callers all need rewriting) |
| Virtual paths only | Portable, archive-able later | Real filesystem paths (couples assets to dev layout) |

## Open questions

- Where does the asset cache live (memory pressure / eviction)? Decision deferred to M6.
- Serialization format for cached preprocessed assets? Likely a tagged binary container; spec out in M6.
