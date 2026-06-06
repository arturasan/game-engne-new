# 0002 — Platform: Window + Input (SDL3 hidden)

- Owner: TBD
- Milestone: M1
- Status: implemented
- Tracking issue: TBD
- Implementation PR: https://github.com/arturasan/game-engne-new/pull/5
- Follow-up integration PR: https://github.com/arturasan/game-engne-new/pull/10
- Merged in: cb473ea (2026-06-03)

## Scope

Wrap SDL3 behind `engine::Window` and `engine::Input` in `engine/platform/`. Public headers must never mention `SDL_*`. Provide a `PlatformPlugin` that opens a window, pumps events into an `Input` resource, and dispatches typed input events.

## Acceptance criteria

- [x] `engine::WindowConfig` POD: `title`, `width`, `height`, `resizable`, `fullscreen`, `vsync`.
- [x] `engine::Window` exposes: `size() -> engine::Extent2d`, `close()`, `should_close()`, `swap()` (no-op in M1, hook for renderer in 0005).
- [x] `engine::Input` resource exposes: `key_pressed(Key)`, `key_just_pressed(Key)`, `key_just_released(Key)`, `mouse_pressed(MouseButton)`, `mouse_just_pressed(MouseButton)`, `mouse_just_released(MouseButton)`, `mouse_position() -> engine::vec2`, `mouse_delta() -> engine::vec2`, and `wheel_delta() -> engine::vec2`.
- [x] Typed event channels: `KeyEvent`, `MouseButtonEvent`, `MouseMotionEvent`, `MouseWheelEvent`, `WindowResizeEvent`, `WindowCloseRequested`.
- [x] `engine::Key` and `engine::MouseButton` enums cover the full SDL3 surface but are defined in `engine/platform/input.hpp` without including any SDL header.
- [x] `PlatformPlugin::build(App&)` inserts the `Window` + `Input` resources and registers a `First` schedule system that pumps events.
- [x] `ENGINE_HEADLESS=1` env var skips real window creation; events come from a programmable queue (foundation for replay).
- [x] Grep test: `grep -r 'SDL_\|SDL3' engine/platform/*.hpp` returns zero hits.
- [x] Unit tests in `tests/unit/test_platform.cpp`: enum round-trip, headless event injection drives `Input` state changes correctly. Tagged `[fast]`.
- [x] `examples/hello_window` updated to use `PlatformPlugin` and exit on `Escape`.

## Implementation notes

- SDL3 is private to implementation/detail files. Public platform headers expose only `engine::` types and include no SDL headers, pointer types, enums, or constants.
- `ENGINE_HEADLESS=1` selects the headless backend and drives input/window state through a programmable event queue for tests and future replay work.
- `engine::Input` mutation is private gameplay-facing API surface; the platform event pump routes frame clearing and event-derived state updates through `engine::detail::InputAccess`.
- Follow-up integration after `specs/0004-resources-and-events.md`: `PlatformPlugin` now stores `Window`, `Input`, an internal headless queue, and typed `Events<T>` channels as `World` resources. The event-maintenance systems run first, then the platform pump runs in `First`, so normal `Update` systems can read same-frame input events. The temporary global `App*` platform registry was removed.

## Out of scope

- Gamepad / joystick input (later milestone).
- Touch / multi-touch.
- IME / text input.
- Clipboard.
- Multiple windows — single primary window only for M1.
- Renderer integration (spec 0005).

## Files not to touch

- `engine/core/*`, `engine/ecs/*` — not affected.
- `engine/render*`, `engine/audio*`, `engine/assets*`.

## Notes for the implementing agent

- Read `docs/architecture/03-platform.md` for the target API shape.
- Backend lives in `engine/platform/detail/sdl3_window_backend.cpp`. The header includes SDL3; the .cpp does the work.
- On Linux, SDL3 from vcpkg defaults to Wayland; that's fine for M1. For CI add `SDL_VIDEODRIVER=offscreen` when `ENGINE_HEADLESS=1`.
- Map SDL keys to `engine::Key` via a generated table — keep it next to the backend, not in a public header.
- Do not leak SDL pointer types through `engine::detail::backend_handle` yet; defer until a plugin needs it.
