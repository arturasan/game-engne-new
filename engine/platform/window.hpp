#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/platform/events.hpp"
#include "engine/platform/types.hpp"

namespace engine {

struct WindowConfig {
    std::string title = "engine";
    int width = 1280;
    int height = 720;
    bool resizable = true;
    bool fullscreen = false;
    bool vsync = true;
};

namespace detail {

class WindowBackend;
struct WindowAccess;

} // namespace detail

class Window {
public:
    Window();
    explicit Window(WindowConfig config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    [[nodiscard]] Extent2d size() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    void close() noexcept;
    void swap() noexcept;

private:
    void set_size(Extent2d size) noexcept;
    void poll_events(std::vector<PlatformEvent>& events);

    friend struct detail::WindowAccess;

    Extent2d size_{};
    bool should_close_ = false;
    std::unique_ptr<detail::WindowBackend> backend_;
};

namespace detail {

struct WindowAccess {
    static void set_size(Window& window, Extent2d size) noexcept {
        window.set_size(size);
    }

    static void poll_events(Window& window, std::vector<PlatformEvent>& events) {
        window.poll_events(events);
    }
};

} // namespace detail

} // namespace engine
