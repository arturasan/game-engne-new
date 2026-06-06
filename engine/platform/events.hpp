#pragma once

#include <cstdint>
#include <variant>

#include "engine/platform/input.hpp"
#include "engine/platform/types.hpp"

namespace engine {

struct KeyEvent {
    Key key = Key::Unknown;
    bool pressed = false;
    bool repeat = false;
    std::uint32_t mods = 0;
};

struct MouseButtonEvent {
    MouseButton button = MouseButton::Left;
    bool pressed = false;
    vec2 position{};
};

struct MouseMotionEvent {
    vec2 position{};
    vec2 delta{};
};

struct MouseWheelEvent {
    vec2 delta{};
};

struct WindowResizeEvent {
    Extent2d size{};
};

struct WindowCloseRequested {};

using PlatformEvent = std::variant<KeyEvent, MouseButtonEvent, MouseMotionEvent, MouseWheelEvent,
                                   WindowResizeEvent, WindowCloseRequested>;

} // namespace engine
