#include <SDL3/SDL.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/platform/detail/window_backend.hpp"

namespace engine::detail {

namespace {

class HeadlessWindowBackend final : public WindowBackend {
public:
    void poll_events(std::vector<PlatformEvent>&) override {}
    void swap() override {}
};

class SdlWindowBackend final : public WindowBackend {
public:
    explicit SdlWindowBackend(const WindowConfig& config) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }
        initialized_ = true;

        SDL_WindowFlags flags = 0;
        if (config.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }
        if (config.fullscreen) {
            flags |= SDL_WINDOW_FULLSCREEN;
        }

        window_ = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
        if (window_ == nullptr) {
            const std::string message = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
            SDL_Quit();
            initialized_ = false;
            throw std::runtime_error(message);
        }

        static_cast<void>(SDL_SetWindowSurfaceVSync(window_, config.vsync ? 1 : 0));
    }

    ~SdlWindowBackend() override {
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        if (initialized_) {
            SDL_Quit();
        }
    }

    SdlWindowBackend(const SdlWindowBackend&) = delete;
    SdlWindowBackend& operator=(const SdlWindowBackend&) = delete;

    void poll_events(std::vector<PlatformEvent>& events) override {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            translate_event(event, events);
        }
    }

    void swap() override {}

private:
    static void translate_event(const SDL_Event& event, std::vector<PlatformEvent>& events) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            events.push_back(KeyEvent{
                .key = key_from_scancode(event.key.scancode),
                .pressed = event.key.down,
                .repeat = event.key.repeat,
                .mods = static_cast<std::uint32_t>(event.key.mod),
            });
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (const auto button = mouse_button_from_index(event.button.button);
                button != MouseButton::Count) {
                events.push_back(MouseButtonEvent{
                    .button = button,
                    .pressed = event.button.down,
                    .position = vec2{event.button.x, event.button.y},
                });
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            events.push_back(MouseMotionEvent{
                .position = vec2{event.motion.x, event.motion.y},
                .delta = vec2{event.motion.xrel, event.motion.yrel},
            });
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            events.push_back(MouseWheelEvent{.delta = vec2{event.wheel.x, event.wheel.y}});
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            events.push_back(WindowResizeEvent{
                .size =
                    Extent2d{
                        .width = static_cast<std::uint32_t>(event.window.data1),
                        .height = static_cast<std::uint32_t>(event.window.data2),
                    },
            });
            break;
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            events.push_back(WindowCloseRequested{});
            break;
        default:
            break;
        }
    }

    static Key key_from_scancode(SDL_Scancode scancode) noexcept {
        const auto value = static_cast<std::uint16_t>(scancode);
        if (value >= static_cast<std::uint16_t>(Key::Count)) {
            return Key::Unknown;
        }
        return static_cast<Key>(value);
    }

    static MouseButton mouse_button_from_index(std::uint8_t button) noexcept {
        switch (button) {
        case SDL_BUTTON_LEFT:
            return MouseButton::Left;
        case SDL_BUTTON_RIGHT:
            return MouseButton::Right;
        case SDL_BUTTON_MIDDLE:
            return MouseButton::Middle;
        case SDL_BUTTON_X1:
            return MouseButton::X1;
        case SDL_BUTTON_X2:
            return MouseButton::X2;
        default:
            return MouseButton::Count;
        }
    }

    SDL_Window* window_ = nullptr;
    bool initialized_ = false;
};

[[nodiscard]] bool env_flag_enabled(const char* name) noexcept {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && std::string_view(value) != "0";
}

} // namespace

std::unique_ptr<WindowBackend> create_window_backend(const WindowConfig& config) {
    if (should_use_headless_backend()) {
        static_cast<void>(config);
        return std::make_unique<HeadlessWindowBackend>();
    }
    return std::make_unique<SdlWindowBackend>(config);
}

bool should_use_headless_backend() noexcept {
    return env_flag_enabled("ENGINE_HEADLESS");
}

} // namespace engine::detail
