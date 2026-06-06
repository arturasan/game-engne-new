#pragma once

#include "engine/core/app.hpp"
#include "engine/platform/events.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"

namespace engine {

struct PlatformPlugin {
    WindowConfig config{};
    void build(App& app) const;
};

[[nodiscard]] bool platform_attached(const App& app) noexcept;
[[nodiscard]] bool platform_headless() noexcept;
[[nodiscard]] Window& window(App& app);
[[nodiscard]] const Window& window(const App& app);
[[nodiscard]] Input& input(App& app);
[[nodiscard]] const Input& input(const App& app);

void push_headless_event(App& app, PlatformEvent event);
void pump_platform_events(App& app);

} // namespace engine
