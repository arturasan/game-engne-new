# Error handling

## TL;DR

- Use `engine::Result<T>` = `std::expected<T, Error>` for fallible operations.
- **No exceptions cross module boundaries.** Throwing is fine inside a single TU; never let an exception leak out of a public engine function.
- `assert` / `ENGINE_ASSERT` for programmer errors (invariants); `Result` for runtime errors (file missing, GPU OOM).

## Error type

```cpp
namespace engine {

enum class ErrorCode : std::uint32_t {
    Ok = 0,
    NotFound,
    PermissionDenied,
    InvalidArgument,
    OutOfMemory,
    UnsupportedFormat,
    BackendError,           // wraps an opaque backend message
    Cancelled,
    Timeout,
};

struct Error {
    ErrorCode   code;
    std::string message;          // human-readable, may include format-specific detail
    std::source_location where = std::source_location::current();
};

template <typename T>
using Result = std::expected<T, Error>;

}  // namespace engine
```

Construction helpers:

```cpp
return engine::Err{ ErrorCode::NotFound, std::format("file '{}' not found", path) };
```

## Conventions

- **Public functions that can fail return `Result<T>`.** Even if the failure is "rare". No `bool ok = foo(); if (!ok) ...` patterns.
- **Public functions that cannot fail return `T`.** Don't pollute infallible APIs with `Result`.
- **`Result<void>`** for fallible operations with no payload.
- **`Result<T>::value()` is forbidden in engine code** outside tests. Use `if (auto r = ...; r) { use(*r); } else { handle(r.error()); }`. We reserve `.value()` (throws on error) for tests and examples where panicking is acceptable.
- **`Result` propagates** via `try_` / monadic `and_then` — pick one per module and stay consistent.

## Asserts

```cpp
#define ENGINE_ASSERT(cond, ...) /* in debug: assert + spdlog; in release: __builtin_assume */
#define ENGINE_DEBUG_ASSERT(cond, ...) /* debug only */
```

- `ENGINE_ASSERT` — invariant that should always hold; cheap enough to keep in release. Use for "this can't happen" checks.
- `ENGINE_DEBUG_ASSERT` — expensive invariant; debug-only.
- Both log with `engine::log::critical` before aborting in debug.

## What is a programmer error vs runtime error?

| Situation | Treatment |
|---|---|
| Passing a null pointer to a function that documents non-null | `ENGINE_ASSERT` (programmer error) |
| `Entity` handle stale | `World::get()` returns `nullptr`; `World::add()` returns `Result` (runtime — could be a deserialized handle) |
| Asset file missing | `Result<Error{NotFound}>` |
| GPU device lost | `Result<Error{BackendError}>`; renderer attempts recovery on next frame |
| Out-of-bounds component access via `Query` | `ENGINE_DEBUG_ASSERT` (the iterator's contract guarantees in-bounds; bug if not) |
| Loader given malformed bytes | `Result<Error{UnsupportedFormat}>` |
| Two systems with conflicting access registered | `ENGINE_ASSERT` at App startup — fail loud and early |

## Exceptions

- Internal use is fine — e.g. an STL function throwing `std::bad_alloc` inside a single `.cpp` file can be caught and translated to `Error{OutOfMemory}`.
- **Never propagate** across:
  - The C ABI boundary (callbacks from SDL3, miniaudio, etc.)
  - Plugin boundaries (because plugins may compile with different exception settings)
  - Coroutine boundaries (coroutine machinery + exceptions is full of corners)
- All public engine functions are noexcept-by-contract (we don't annotate `noexcept` on every function because that's noisy, but the contract holds).

## Logging on errors

Three policies:

1. **The function that *handles* the error logs it.** Lower-level functions return `Result` without logging — too much noise otherwise.
2. **Exception: backend errors are logged at the boundary** (e.g. SDL3 GPU returning an error message gets a `log::error` immediately, *and* an `Error` returned). Backend errors are rare and the original message is precious.
3. **Critical / fatal-class errors always log before aborting.** No silent abort.

## Recovery patterns

- **Asset failed to load:** keep the placeholder (pink-checkerboard texture, sine-wave audio). Surface the error in the structured log; continue running.
- **GPU device lost:** attempt one recreation; on second failure, log critical and exit.
- **OOM:** log critical and exit. No "graceful degradation" path — the user will see a crash report when M6 lands.

## Examples

Good:
```cpp
auto img = world.resource<AssetServer>().load_sync<Image>("ui/cursor.png");
if (!img) {
    log::warn("cursor load failed: {}", img.error().message);
    img = world.resource<AssetServer>().load_sync<Image>("ui/cursor_fallback.png");
}
auto handle = img.value_or_default(default_cursor_handle);
```

Bad:
```cpp
auto img = world.resource<AssetServer>().load_sync<Image>("ui/cursor.png").value();
//                                                                          ^^^
// `value()` panics in release if missing. Forbidden outside tests.
```

## Decisions & alternatives

| Decision | Rationale | Rejected |
|---|---|---|
| `std::expected` not exceptions | Predictable cost; works across plugin boundaries | Exceptions (issues at ABI/plugin boundaries; cost is unbounded) |
| `Error` carries `source_location` | Almost-free debugging when an error surfaces far from its source | Plain enum (loses context) |
| `.value()` banned in engine code | Forces callers to actually handle errors | Use `.value()` freely (turns soft errors into crashes) |
