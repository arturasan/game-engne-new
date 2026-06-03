#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine {

using ComponentId = std::uint32_t;
using ResourceId = std::uint32_t;

namespace detail {

class ComponentColumn {
public:
    virtual ~ComponentColumn() = default;

    virtual void move_push_from(ComponentColumn& source, std::size_t row) = 0;
    virtual void erase_swap(std::size_t row) = 0;
};

template <typename T> class TypedComponentColumn final : public ComponentColumn {
public:
    void move_push_from(ComponentColumn& source, std::size_t row) override {
        auto& typed_source = static_cast<TypedComponentColumn<T>&>(source);
        values.push_back(std::move(typed_source.values[row]));
    }

    void erase_swap(std::size_t row) override {
        if (row + 1U != values.size()) {
            values[row] = std::move(values.back());
        }
        values.pop_back();
    }

    template <typename... Args> T& emplace_back(Args&&... args) {
        return values.emplace_back(std::forward<Args>(args)...);
    }

    [[nodiscard]] T& value(std::size_t row) {
        return values[row];
    }

    [[nodiscard]] const T& value(std::size_t row) const {
        return values[row];
    }

private:
    std::vector<T> values;
};

struct ComponentInfo {
    ComponentId id = 0;
    std::unique_ptr<ComponentColumn> (*make_column)() = nullptr;
};

inline std::vector<const ComponentInfo*>& component_registry() {
    static std::vector<const ComponentInfo*> registry;
    return registry;
}

inline ComponentId next_component_id() {
    static std::atomic<ComponentId> next = 0;
    return next.fetch_add(1, std::memory_order_relaxed);
}

inline ResourceId next_resource_id() {
    static std::atomic<ResourceId> next = 0;
    return next.fetch_add(1, std::memory_order_relaxed);
}

template <typename T> ResourceId normalized_resource_id_for() {
    static const ResourceId id = next_resource_id();
    return id;
}

inline bool register_component_info(const ComponentInfo& info) {
    auto& registry = component_registry();
    if (info.id >= registry.size()) {
        registry.resize(static_cast<std::size_t>(info.id) + 1U);
    }
    registry[info.id] = &info;
    return true;
}

inline const ComponentInfo& component_info_for_id(ComponentId id) {
    return *component_registry()[id];
}

template <typename T> std::unique_ptr<ComponentColumn> make_typed_column() {
    return std::make_unique<TypedComponentColumn<T>>();
}

template <typename T> const ComponentInfo& component_info_for() {
    using Raw = std::remove_cv_t<T>;
    static const ComponentInfo info{next_component_id(), &make_typed_column<Raw>};
    static const bool registered = register_component_info(info);
    static_cast<void>(registered);
    return info;
}

template <typename T, typename... Us>
inline constexpr bool different_from_all =
    (!std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<Us>> && ...);

template <typename... Ts> inline constexpr bool unique_component_types = true;

template <typename T, typename... Rest>
inline constexpr bool unique_component_types<T, Rest...> =
    different_from_all<T, Rest...> && unique_component_types<Rest...>;

} // namespace detail

template <typename T> ComponentId component_id_for() {
    return detail::component_info_for<T>().id;
}

template <typename T> ResourceId resource_id_for() {
    using Raw = std::remove_cvref_t<T>;
    return detail::normalized_resource_id_for<Raw>();
}

} // namespace engine
