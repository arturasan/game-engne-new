#include "engine/platform/window.hpp"

#include <algorithm>

#include "engine/platform/detail/window_backend.hpp"

namespace engine {

namespace {

[[nodiscard]] std::uint32_t clamp_dimension(int value) noexcept {
    return static_cast<std::uint32_t>(std::max(value, 1));
}

} // namespace

namespace detail {

NativeWindowLease WindowAccess::acquire_native_lease(Window& window) noexcept {
    if (window.backend_ == nullptr) {
        return {};
    }
    return NativeWindowLease{window.backend_, window.backend_->native_handle(),
                             window.backend_->vsync_requested(),
                             window.backend_->creation_thread()};
}

} // namespace detail

Window::Window() = default;

Window::Window(WindowConfig config)
    : size_{clamp_dimension(config.width), clamp_dimension(config.height)},
      backend_(detail::create_window_backend(config)) {}

Window::~Window() = default;

Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

Extent2d Window::size() const noexcept {
    return size_;
}

bool Window::should_close() const noexcept {
    return should_close_;
}

void Window::close() noexcept {
    should_close_ = true;
}

void Window::swap() noexcept {
    if (backend_ != nullptr) {
        backend_->swap();
    }
}

void Window::set_size(Extent2d size) noexcept {
    size_ = size;
}

void Window::poll_events(std::vector<PlatformEvent>& events) {
    if (backend_ != nullptr) {
        backend_->poll_events(events);
    }
}

} // namespace engine
