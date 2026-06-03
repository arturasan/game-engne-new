#pragma once

#include <memory>
#include <vector>

#include "engine/platform/events.hpp"
#include "engine/platform/window.hpp"

namespace engine::detail {

class WindowBackend {
public:
    virtual ~WindowBackend() = default;
    virtual void poll_events(std::vector<PlatformEvent>& events) = 0;
    virtual void swap() = 0;
};

[[nodiscard]] std::unique_ptr<WindowBackend> create_window_backend(const WindowConfig& config);
[[nodiscard]] bool should_use_headless_backend() noexcept;

} // namespace engine::detail
