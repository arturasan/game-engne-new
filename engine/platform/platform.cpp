#include "engine/platform/platform.hpp"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/platform/detail/window_backend.hpp"

namespace engine {

namespace {

struct PlatformState {
    Window window;
    Input input;
    PlatformEvents events;
    std::vector<PlatformEvent> headless_events;
};

[[nodiscard]] std::unordered_map<const App*, std::weak_ptr<PlatformState>>& states() {
    static auto* registry = new std::unordered_map<const App*, std::weak_ptr<PlatformState>>();
    return *registry;
}

[[nodiscard]] PlatformState& state_for(App& app) {
    if (auto it = states().find(&app); it != states().end()) {
        if (auto state = it->second.lock()) {
            return *state;
        }
    }
    throw std::logic_error("PlatformPlugin must be added before accessing platform state");
}

[[nodiscard]] const PlatformState& state_for(const App& app) {
    if (auto it = states().find(&app); it != states().end()) {
        if (auto state = it->second.lock()) {
            return *state;
        }
    }
    throw std::logic_error("PlatformPlugin must be added before accessing platform state");
}

void apply_event(PlatformState& state, const KeyEvent& event) {
    state.events.key.push_back(event);
    detail::InputAccess::set_key(state.input, event.key, event.pressed);
}

void apply_event(PlatformState& state, const MouseButtonEvent& event) {
    state.events.mouse_button.push_back(event);
    detail::InputAccess::set_mouse_position(state.input, event.position);
    detail::InputAccess::set_mouse_button(state.input, event.button, event.pressed);
}

void apply_event(PlatformState& state, const MouseMotionEvent& event) {
    state.events.mouse_motion.push_back(event);
    detail::InputAccess::set_mouse_position(state.input, event.position);
    detail::InputAccess::add_mouse_delta(state.input, event.delta);
}

void apply_event(PlatformState& state, const MouseWheelEvent& event) {
    state.events.mouse_wheel.push_back(event);
    detail::InputAccess::add_wheel_delta(state.input, event.delta);
}

void apply_event(PlatformState& state, const WindowResizeEvent& event) {
    state.events.window_resize.push_back(event);
    detail::WindowAccess::set_size(state.window, event.size);
}

void apply_event(PlatformState& state, const WindowCloseRequested& event) {
    state.events.window_close_requested.push_back(event);
    state.window.close();
}

} // namespace

void PlatformPlugin::build(App& app) const {
    auto state = std::make_shared<PlatformState>();
    state->window = Window(config);
    states()[&app] = state;
    app.add_system("platform_event_pump", [state = std::move(state)](App& running_app) {
        static_cast<void>(state);
        pump_platform_events(running_app);
    });
}

bool platform_attached(const App& app) noexcept {
    const auto it = states().find(&app);
    return it != states().end() && !it->second.expired();
}

bool platform_headless() noexcept {
    return detail::should_use_headless_backend();
}

Window& window(App& app) {
    return state_for(app).window;
}

const Window& window(const App& app) {
    return state_for(app).window;
}

Input& input(App& app) {
    return state_for(app).input;
}

const Input& input(const App& app) {
    return state_for(app).input;
}

PlatformEvents& platform_events(App& app) {
    return state_for(app).events;
}

const PlatformEvents& platform_events(const App& app) {
    return state_for(app).events;
}

void push_headless_event(App& app, PlatformEvent event) {
    if (!platform_headless()) {
        throw std::logic_error("headless events require ENGINE_HEADLESS=1");
    }
    state_for(app).headless_events.push_back(std::move(event));
}

void pump_platform_events(App& app) {
    PlatformState& state = state_for(app);
    detail::InputAccess::clear_frame_state(state.input);
    state.events.clear();

    std::vector<PlatformEvent> pending;
    if (platform_headless()) {
        pending.swap(state.headless_events);
    } else {
        pending.reserve(32);
        detail::WindowAccess::poll_events(state.window, pending);
    }

    for (const PlatformEvent& event : pending) {
        std::visit([&state](const auto& concrete_event) { apply_event(state, concrete_event); },
                   event);
    }
}

} // namespace engine
