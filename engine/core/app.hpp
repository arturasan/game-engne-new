#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

#include "engine/core/plugin.hpp"
#include "engine/core/schedule.hpp"
#include "engine/ecs/events.hpp"
#include "engine/ecs/world.hpp"

namespace engine {

class App {
public:
    App& add_system(std::string_view label, SystemFn fn) {
        update_.add(label, std::move(fn));
        return *this;
    }

    template <typename System> App& add_system(std::string_view label, System&& system) {
        update_.add(label, std::forward<System>(system));
        return *this;
    }

    template <typename T> App& add_event() {
        world_.insert_resource(Events<T>{});
        first_.add("events_update", [](ResMut<Events<T>> events) { events->update(); });
        return *this;
    }

    template <Plugin P> App& add_plugin(P plugin) {
        plugin.build(*this);
        return *this;
    }

    App& set_max_frames(std::uint64_t n) noexcept {
        max_frames_ = n;
        return *this;
    }

    App& request_exit() noexcept {
        exit_requested_ = true;
        return *this;
    }

    [[nodiscard]] std::uint64_t frame() const noexcept {
        return frame_;
    }
    [[nodiscard]] World& world() noexcept {
        return world_;
    }
    [[nodiscard]] const World& world() const noexcept {
        return world_;
    }
    [[nodiscard]] Schedule& first() noexcept {
        return first_;
    }
    [[nodiscard]] Schedule& update() noexcept {
        return update_;
    }

    int run();

private:
    World world_;
    Schedule first_;
    Schedule update_;
    std::uint64_t frame_ = 0;
    std::uint64_t max_frames_ = 0;
    bool exit_requested_ = false;
};

} // namespace engine
