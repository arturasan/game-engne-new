#include "engine/platform/input.hpp"

#include <algorithm>

namespace engine {

namespace {

[[nodiscard]] std::size_t key_index(Key key) noexcept {
    return static_cast<std::size_t>(key);
}

[[nodiscard]] std::size_t mouse_button_index(MouseButton button) noexcept {
    return static_cast<std::size_t>(button);
}

} // namespace

bool valid_key(Key key) noexcept {
    return key_index(key) < static_cast<std::size_t>(Key::Count);
}

bool valid_mouse_button(MouseButton button) noexcept {
    return mouse_button_index(button) < static_cast<std::size_t>(MouseButton::Count);
}

bool Input::key_pressed(Key key) const noexcept {
    return valid_key(key) && keys_[key_index(key)];
}

bool Input::key_just_pressed(Key key) const noexcept {
    return valid_key(key) && keys_just_pressed_[key_index(key)];
}

bool Input::key_just_released(Key key) const noexcept {
    return valid_key(key) && keys_just_released_[key_index(key)];
}

bool Input::mouse_pressed(MouseButton button) const noexcept {
    return valid_mouse_button(button) && mouse_buttons_[mouse_button_index(button)];
}

bool Input::mouse_just_pressed(MouseButton button) const noexcept {
    return valid_mouse_button(button) && mouse_buttons_just_pressed_[mouse_button_index(button)];
}

bool Input::mouse_just_released(MouseButton button) const noexcept {
    return valid_mouse_button(button) && mouse_buttons_just_released_[mouse_button_index(button)];
}

vec2 Input::mouse_position() const noexcept {
    return mouse_position_;
}

vec2 Input::mouse_delta() const noexcept {
    return mouse_delta_;
}

vec2 Input::wheel_delta() const noexcept {
    return wheel_delta_;
}

void Input::clear_frame_state() noexcept {
    std::ranges::fill(keys_just_pressed_, false);
    std::ranges::fill(keys_just_released_, false);
    std::ranges::fill(mouse_buttons_just_pressed_, false);
    std::ranges::fill(mouse_buttons_just_released_, false);
    mouse_delta_ = {};
    wheel_delta_ = {};
}

void Input::set_key(Key key, bool pressed) noexcept {
    if (!valid_key(key)) {
        return;
    }

    const std::size_t index = key_index(key);
    if (pressed) {
        keys_just_pressed_[index] = !keys_[index];
        keys_[index] = true;
    } else {
        keys_just_released_[index] = keys_[index];
        keys_[index] = false;
    }
}

void Input::set_mouse_button(MouseButton button, bool pressed) noexcept {
    if (!valid_mouse_button(button)) {
        return;
    }

    const std::size_t index = mouse_button_index(button);
    if (pressed) {
        mouse_buttons_just_pressed_[index] = !mouse_buttons_[index];
        mouse_buttons_[index] = true;
    } else {
        mouse_buttons_just_released_[index] = mouse_buttons_[index];
        mouse_buttons_[index] = false;
    }
}

void Input::set_mouse_position(vec2 position) noexcept {
    mouse_position_ = position;
}

void Input::add_mouse_delta(vec2 delta) noexcept {
    mouse_delta_.x += delta.x;
    mouse_delta_.y += delta.y;
}

void Input::add_wheel_delta(vec2 delta) noexcept {
    wheel_delta_.x += delta.x;
    wheel_delta_.y += delta.y;
}

} // namespace engine
