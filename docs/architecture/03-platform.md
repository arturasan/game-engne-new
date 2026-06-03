# Platform — Window, Input

## Scope

Abstraction over windowing, input devices, and OS events. SDL3 is the initial backend; the abstraction must be tight enough that SDL3 can be swapped (e.g. for GLFW, native Win32/Cocoa, or a WebGPU canvas in WASM) without touching anything above this layer.

## Public API (target shape)

```cpp
namespace engine {

struct WindowConfig {
    std::string title  = "engine";
    int  width         = 1280;
    int  height        = 720;
    bool resizable     = true;
    bool vsync         = true;
    bool decorations   = true;
    bool transparent   = false;
};

class Window {
public:
    [[nodiscard]] int  width()  const noexcept;
    [[nodiscard]] int  height() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    void request_close() noexcept;

    // Render integration. The Renderer pulls these via SurfaceDescriptor; no
    // backend types appear here.
    [[nodiscard]] SurfaceDescriptor surface_descriptor() const noexcept;
};

// ---------------------------- Input ---------------------------- //

enum class Key      : std::uint16_t { /* full set, USB HID-like ordering */ };
enum class MouseBtn : std::uint8_t  { Left, Right, Middle, X1, X2 };

struct KeyEvent      { Key key; bool pressed; bool repeat; uint32_t mods; };
struct MouseBtnEvent { MouseBtn button; bool pressed; float x, y; };
struct MouseMoveEvent{ float x, y; float dx, dy; };
struct MouseWheelEvent{ float dx, dy; };
struct WindowResize  { int width, height; };
struct TextInput     { std::array<char, 32> utf8; std::uint8_t len; };

// Polled state (resource on the World).
class Input {
public:
    bool key_down(Key) const;
    bool key_just_pressed(Key) const;
    bool key_just_released(Key) const;
    bool mouse_down(MouseBtn) const;
    glm_vec2_alias mouse_pos() const;       // engine::vec2 internally
    glm_vec2_alias mouse_delta() const;
    float          wheel_delta() const;
};

}  // namespace engine
```

## Backend boundary

```cpp
// engine/platform/sdl3_backend.cpp  (the ONLY file that #includes <SDL3/SDL.h>)
namespace engine::detail {
    void* sdl_window_handle(const Window&);  // for the renderer; opaque to callers
}
```

Anything that needs the raw `SDL_Window*` (e.g. claiming the window for the SDL3 GPU device) goes through `engine::detail::` and is not part of the public API.

## Event flow

```
[OS event] → SDL3 → platform/sdl3_backend.cpp
                         │
                         ▼
                Translate to engine::* event POD
                         │
                         ▼
       Push into engine::Events<KeyEvent>, etc.
                         │
                         ▼
         Update polled Input state (key_just_pressed, etc.)
                         │
                         ▼
   Systems in PreUpdate consume Events<...> and read Input
```

The polled `Input` resource and `Events<T>` queues coexist: use **events** for "did this discrete thing happen?", use **polled state** for "is this key currently down?". Both are derived from the same OS event stream during `PreUpdate`.

## Plugin

```cpp
struct WindowPlugin {
    WindowConfig config;
    void build(App& app) const;   // inserts Window resource, opens window, hooks event pump
};

struct InputPlugin {
    void build(App& app) const;   // inserts Input resource, registers event listeners
};
```

`WindowPlugin::build` opens the window during `build` (not during `run`) so that `Renderer` (which depends on the window's surface descriptor) can be initialized in its own plugin's `build` without ordering pain.

## Multi-window

M1 is single-window only. The API is shaped to allow multi-window in M3:

- `World` will hold `Windows` (a typed collection), not a single `Window`.
- Each window will have its own surface and camera; `Camera::target` will reference a `WindowId` or `RenderTexture`.

## Headless

For tests and CI, a **headless window backend** is selected when `ENGINE_HEADLESS=1` is set:

- `Window` reports a fixed size, never produces close events.
- Input is driven by recorded streams (see `docs/architecture/07-testing.md`).
- The renderer uses an off-screen target instead of a swap chain.

## Decisions & alternatives

| Decision | Rationale | Rejected |
|---|---|---|
| SDL3 over GLFW | Gamepads, audio init, mobile/web in one library | GLFW (cleaner API but smaller scope) |
| Events + polled Input both | Different use cases; recomputing one from the other is fragile | Events-only (annoying for "is W held"); polled-only (loses discreteness) |
| Window opened during `Plugin::build` | Removes ordering coupling between Window and Renderer | Defer window creation to first `Update` (caused init-order pain in prototypes) |
| Headless backend selected via env var | Simple, works with any test runner | Compile-time switch (forces a second build configuration) |

## Open questions

- IME / complex text input — defer to M3 when an inspector exists.
- Display DPI / scaling — abstract now (`Window::scale_factor()`), wire in M2.
