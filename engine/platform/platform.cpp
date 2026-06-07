#include "engine/platform/platform.hpp"

#include <cassert>
#include <exception>
#include <utility>
#include <vector>

#include "engine/core/log.hpp"
#include "engine/ecs/events.hpp"
#include "engine/platform/detail/window_backend.hpp"

namespace engine {

namespace {

struct HeadlessEvents {
    std::vector<PlatformEvent> events;
};

void apply_event(World& world, const KeyEvent& event) {
    detail::InputAccess::set_key(world.resource<Input>(), event.key, event.pressed);
    world.resource<Events<KeyEvent>>().send(event);
}

void apply_event(World& world, const MouseButtonEvent& event) {
    Input& input = world.resource<Input>();
    detail::InputAccess::set_mouse_position(input, event.position);
    detail::InputAccess::set_mouse_button(input, event.button, event.pressed);
    world.resource<Events<MouseButtonEvent>>().send(event);
}

void apply_event(World& world, const MouseMotionEvent& event) {
    Input& input = world.resource<Input>();
    detail::InputAccess::set_mouse_position(input, event.position);
    detail::InputAccess::add_mouse_delta(input, event.delta);
    world.resource<Events<MouseMotionEvent>>().send(event);
}

void apply_event(World& world, const MouseWheelEvent& event) {
    detail::InputAccess::add_wheel_delta(world.resource<Input>(), event.delta);
    world.resource<Events<MouseWheelEvent>>().send(event);
}

void apply_event(World& world, const WindowResizeEvent& event) {
    detail::WindowAccess::set_size(world.resource<Window>(), event.size);
    world.resource<Events<WindowResizeEvent>>().send(event);
}

void apply_event(World& world, const WindowCloseRequested& event) {
    world.resource<Window>().close();
    world.resource<Events<WindowCloseRequested>>().send(event);
}

} // namespace

void PlatformPlugin::build(App& app) const {
    Window created_window;
    try {
        created_window = Window(config);
    } catch (const std::exception& exception) {
        log::error("platform initialization failed: {}", exception.what());
        app.request_exit();
        return;
    }

    app.world().insert_resource(std::move(created_window));
    app.world().insert_resource(Input{});
    app.add_event<KeyEvent>();
    app.add_event<MouseButtonEvent>();
    app.add_event<MouseMotionEvent>();
    app.add_event<MouseWheelEvent>();
    app.add_event<WindowResizeEvent>();
    app.add_event<WindowCloseRequested>();
    app.world().insert_resource(HeadlessEvents{});
    app.first().add("platform_event_pump",
                    [](App& running_app) { pump_platform_events(running_app); });
}

bool platform_attached(const App& app) noexcept {
    return app.world().try_resource<Window>() != nullptr &&
           app.world().try_resource<Input>() != nullptr;
}

bool platform_headless() noexcept {
    return detail::should_use_headless_backend();
}

Window& window(App& app) {
    return app.world().resource<Window>();
}

const Window& window(const App& app) {
    return app.world().resource<Window>();
}

Input& input(App& app) {
    return app.world().resource<Input>();
}

const Input& input(const App& app) {
    return app.world().resource<Input>();
}

void push_headless_event(App& app, PlatformEvent event) {
    assert(platform_headless());
    app.world().resource<HeadlessEvents>().events.push_back(std::move(event));
}

void pump_platform_events(App& app) {
    World& world = app.world();
    detail::InputAccess::clear_frame_state(world.resource<Input>());

    std::vector<PlatformEvent> pending;
    if (platform_headless()) {
        pending.swap(world.resource<HeadlessEvents>().events);
    } else {
        pending.reserve(32);
        detail::WindowAccess::poll_events(world.resource<Window>(), pending);
    }

    for (const PlatformEvent& event : pending) {
        std::visit([&world](const auto& concrete_event) { apply_event(world, concrete_event); },
                   event);
    }
}

} // namespace engine
