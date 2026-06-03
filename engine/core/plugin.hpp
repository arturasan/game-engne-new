#pragma once

#include <concepts>

namespace engine {

class App;

template <typename T>
concept Plugin = requires(T t, App& app) {
    { t.build(app) } -> std::same_as<void>;
};

} // namespace engine
