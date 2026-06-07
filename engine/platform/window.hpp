#pragma once

#include <memory>
#include <string>
#include <thread>
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
class NativeWindowLease;
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
    std::shared_ptr<detail::WindowBackend> backend_;
};

namespace detail {

class NativeWindowLease {
public:
    NativeWindowLease() = default;

    [[nodiscard]] void* native_handle() const noexcept {
        return native_handle_;
    }

    [[nodiscard]] bool vsync_requested() const noexcept {
        return vsync_requested_;
    }

    [[nodiscard]] std::thread::id creation_thread() const noexcept {
        return creation_thread_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return backend_ != nullptr && native_handle_ != nullptr;
    }

private:
    friend struct WindowAccess;

    NativeWindowLease(std::shared_ptr<WindowBackend> backend, void* native_handle,
                      bool vsync_requested, std::thread::id creation_thread) noexcept
        : backend_(std::move(backend)), native_handle_(native_handle),
          vsync_requested_(vsync_requested), creation_thread_(creation_thread) {}

    std::shared_ptr<WindowBackend> backend_;
    void* native_handle_ = nullptr;
    bool vsync_requested_ = true;
    std::thread::id creation_thread_{};
};

struct WindowAccess {
    static void set_size(Window& window, Extent2d size) noexcept {
        window.set_size(size);
    }

    static void poll_events(Window& window, std::vector<PlatformEvent>& events) {
        window.poll_events(events);
    }

    static NativeWindowLease acquire_native_lease(Window& window) noexcept;
};

} // namespace detail

} // namespace engine
