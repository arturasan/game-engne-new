#pragma once

#include <functional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "engine/ecs/events.hpp"
#include "engine/ecs/world.hpp"

namespace engine {

class App;

using SystemFn = std::function<void(App&)>;

enum class ResourceAccessKind {
    read,
    write,
};

struct ResourceAccess {
    ResourceId id = 0;
    ResourceAccessKind kind = ResourceAccessKind::read;
};

struct SystemAccess {
    std::vector<ResourceAccess> resources;

    [[nodiscard]] bool reads(ResourceId id) const;
    [[nodiscard]] bool writes(ResourceId id) const;
    [[nodiscard]] bool conflicts_with(const SystemAccess& other) const;
};

struct SystemEntry {
    std::string_view label;
    SystemFn run;
    SystemAccess access;
};

class Schedule {
public:
    void add(std::string_view label, SystemFn fn);

    template <typename System> void add(std::string_view label, System&& system);

    void run(App& app);

    [[nodiscard]] std::size_t size() const noexcept {
        return systems_.size();
    }

    [[nodiscard]] std::span<const SystemEntry> systems() const noexcept {
        return systems_;
    }

private:
    std::vector<SystemEntry> systems_;
};

namespace detail {

World& app_world(App& app) noexcept;
const World& app_world(const App& app) noexcept;

template <typename T> struct AlwaysFalse : std::false_type {};

template <typename T> struct SystemTraits;

template <typename Return, typename... Args> struct SystemTraits<Return (*)(Args...)> {
    using Params = std::tuple<Args...>;
};

template <typename Class, typename Return, typename... Args>
struct SystemTraits<Return (Class::*)(Args...) const> {
    using Params = std::tuple<Args...>;
};

template <typename Class, typename Return, typename... Args>
struct SystemTraits<Return (Class::*)(Args...)> {
    using Params = std::tuple<Args...>;
};

template <typename F> struct SystemTraits {
    using Params = typename SystemTraits<decltype(&F::operator())>::Params;
};

struct NoParamState {};

template <typename Param> struct ParamState {
    using Type = NoParamState;
};

template <typename T> struct ParamState<EventReader<T>> {
    struct Type {
        std::uint64_t cursor = 0;
        std::vector<T> unread;
    };
};

template <typename Param> using ParamStateT = typename ParamState<std::remove_cvref_t<Param>>::Type;

template <typename Params, std::size_t... Indexes>
auto state_tuple_for(std::index_sequence<Indexes...>) {
    return std::tuple<ParamStateT<std::tuple_element_t<Indexes, Params>>...>{};
}

template <typename Params> auto state_tuple_for() {
    return state_tuple_for<Params>(std::make_index_sequence<std::tuple_size_v<Params>>{});
}

template <typename Param> void append_access(SystemAccess&, std::type_identity<Param>) {}

template <typename T> void append_access(SystemAccess& access, std::type_identity<Res<T>>) {
    access.resources.push_back({resource_id_for<std::remove_cv_t<T>>(), ResourceAccessKind::read});
}

template <typename T> void append_access(SystemAccess& access, std::type_identity<ResMut<T>>) {
    access.resources.push_back({resource_id_for<std::remove_cv_t<T>>(), ResourceAccessKind::write});
}

template <typename T> void append_access(SystemAccess& access, std::type_identity<EventReader<T>>) {
    access.resources.push_back({resource_id_for<Events<T>>(), ResourceAccessKind::read});
}

template <typename T> void append_access(SystemAccess& access, std::type_identity<EventWriter<T>>) {
    access.resources.push_back({resource_id_for<Events<T>>(), ResourceAccessKind::write});
}

template <typename Param> void append_access_for(SystemAccess& access) {
    using Raw = std::remove_cvref_t<Param>;
    if constexpr (std::is_same_v<Param, App&>) {
        static_cast<void>(access);
    } else {
        append_access(access, std::type_identity<Raw>{});
    }
}

template <typename Params, std::size_t... Indexes>
SystemAccess access_for(std::index_sequence<Indexes...>) {
    SystemAccess access;
    (append_access_for<std::tuple_element_t<Indexes, Params>>(access), ...);
    return access;
}

template <typename Params> SystemAccess access_for() {
    return access_for<Params>(std::make_index_sequence<std::tuple_size_v<Params>>{});
}

template <typename Param, typename State> decltype(auto) make_param(App& app, State& state) {
    using Raw = std::remove_cvref_t<Param>;
    if constexpr (std::is_same_v<Param, App&>) {
        static_cast<void>(state);
        return app;
    } else if constexpr (requires { typename Raw::unsupported_system_param; }) {
        static_assert(AlwaysFalse<Raw>::value, "unsupported system parameter");
    } else {
        static_assert(AlwaysFalse<Raw>::value, "unsupported system parameter");
    }
}

template <typename T, typename State> Res<T> make_param(App& app, State& state, Res<T>*) {
    static_cast<void>(state);
    return Res<T>{app_world(app).resource<T>()};
}

template <typename T, typename State> ResMut<T> make_param(App& app, State& state, ResMut<T>*) {
    static_cast<void>(state);
    return ResMut<T>{app_world(app).resource<T>()};
}

template <typename T, typename State>
EventReader<T> make_param(App& app, State& state, EventReader<T>*) {
    return EventReader<T>{app_world(app).resource<Events<T>>(), state.cursor, state.unread};
}

template <typename T, typename State>
EventWriter<T> make_param(App& app, State& state, EventWriter<T>*) {
    static_cast<void>(state);
    return EventWriter<T>{app_world(app).resource<Events<T>>()};
}

template <typename Param, typename State> decltype(auto) make_param_for(App& app, State& state) {
    using Raw = std::remove_cvref_t<Param>;
    if constexpr (std::is_same_v<Param, App&>) {
        static_cast<void>(state);
        return app;
    } else {
        return make_param(app, state, static_cast<Raw*>(nullptr));
    }
}

template <typename System, typename Params, typename State, std::size_t... Indexes>
void invoke_system(System& system, App& app, State& state, std::index_sequence<Indexes...>) {
    std::invoke(system, make_param_for<std::tuple_element_t<Indexes, Params>>(
                            app, std::get<Indexes>(state))...);
}

template <typename System, typename Params, typename State>
void invoke_system(System& system, App& app, State& state) {
    invoke_system<System, Params>(system, app, state,
                                  std::make_index_sequence<std::tuple_size_v<Params>>{});
}

} // namespace detail

template <typename System> void Schedule::add(std::string_view label, System&& system) {
    using Traits = detail::SystemTraits<std::remove_cvref_t<System>>;
    using Params = typename Traits::Params;
    auto state = detail::state_tuple_for<Params>();
    auto access = detail::access_for<Params>();
    systems_.push_back(
        {label,
         [system = std::forward<System>(system), state = std::move(state)](App& app) mutable {
             detail::invoke_system<std::remove_cvref_t<System>, Params>(system, app, state);
         },
         std::move(access)});
}

} // namespace engine
