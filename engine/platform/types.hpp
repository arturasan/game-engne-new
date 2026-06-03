#pragma once

#include <cstdint>

namespace engine {

struct Extent2d {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const Extent2d&, const Extent2d&) = default;
};

struct vec2 {
    float x = 0.0F;
    float y = 0.0F;

    friend bool operator==(const vec2&, const vec2&) = default;
};

} // namespace engine
