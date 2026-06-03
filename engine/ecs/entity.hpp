#pragma once

#include <compare>
#include <cstdint>
#include <type_traits>

namespace engine {

struct Entity {
    std::uint32_t id = 0;
    std::uint32_t generation = 0;

    constexpr auto operator<=>(const Entity&) const = default;
};

static_assert(sizeof(Entity) == sizeof(std::uint64_t));
static_assert(std::is_standard_layout_v<Entity>);
static_assert(std::is_trivially_copyable_v<Entity>);

} // namespace engine
