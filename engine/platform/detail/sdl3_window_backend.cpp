#include <SDL3/SDL.h>

#include <cassert>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "engine/platform/detail/window_backend.hpp"

namespace engine::detail {

namespace {

struct SubsystemState {
    std::mutex mutex;
    std::uint32_t active_leases = 0;
};

SubsystemState& subsystem_state() {
    static SubsystemState state;
    return state;
}

[[nodiscard]] std::string sdl_error_message(std::string_view context) {
    std::string message{context};
    message += ": ";
    const char* error = SDL_GetError();
    message += error != nullptr && error[0] != '\0' ? error : "unknown SDL error";
    return message;
}

class HeadlessWindowBackend final : public WindowBackend {
public:
    void poll_events(std::vector<PlatformEvent>&) override {}
    void swap() override {}
    [[nodiscard]] void* native_handle() noexcept override {
        return nullptr;
    }
    [[nodiscard]] bool vsync_requested() const noexcept override {
        return true;
    }
    [[nodiscard]] std::thread::id creation_thread() const noexcept override {
        return creation_thread_;
    }

private:
    std::thread::id creation_thread_ = std::this_thread::get_id();
};

class SdlWindowBackend final : public WindowBackend {
public:
    explicit SdlWindowBackend(const WindowConfig& config)
        : vsync_requested_(config.vsync), creation_thread_(std::this_thread::get_id()) {
        auto lease = PlatformSubsystemLease::acquire_window();
        if (!lease.has_value()) {
            throw std::runtime_error(lease.error().message);
        }
        subsystem_lease_ = std::move(*lease);

        SDL_WindowFlags flags = 0;
        if (config.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }
        if (config.fullscreen) {
            flags |= SDL_WINDOW_FULLSCREEN;
        }

        window_ = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
        if (window_ == nullptr) {
            throw std::runtime_error(sdl_error_message("SDL_CreateWindow failed"));
        }

        static_cast<void>(SDL_SetWindowSurfaceVSync(window_, config.vsync ? 1 : 0));
    }

    ~SdlWindowBackend() override {
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
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
    [[nodiscard]] void* native_handle() noexcept override {
        return window_;
    }
    [[nodiscard]] bool vsync_requested() const noexcept override {
        return vsync_requested_;
    }
    [[nodiscard]] std::thread::id creation_thread() const noexcept override {
        return creation_thread_;
    }

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
    PlatformSubsystemLease subsystem_lease_;
    bool vsync_requested_ = true;
    std::thread::id creation_thread_{};
};

[[nodiscard]] bool env_flag_enabled(const char* name) noexcept {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && std::string_view(value) != "0";
}

} // namespace

PlatformSubsystemLease::~PlatformSubsystemLease() {
    reset();
}

PlatformSubsystemLease::PlatformSubsystemLease(PlatformSubsystemLease&& other) noexcept
    : subsystem_flags_(std::exchange(other.subsystem_flags_, 0)) {}

PlatformSubsystemLease& PlatformSubsystemLease::operator=(PlatformSubsystemLease&& other) noexcept {
    if (this != &other) {
        reset();
        subsystem_flags_ = std::exchange(other.subsystem_flags_, 0);
    }
    return *this;
}

Result<PlatformSubsystemLease> PlatformSubsystemLease::acquire_window() {
    constexpr std::uint32_t flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    if (!SDL_InitSubSystem(flags)) {
        return Err{ErrorCode::BackendError, sdl_error_message("SDL_InitSubSystem failed")};
    }

    auto& state = subsystem_state();
    std::lock_guard lock{state.mutex};
    ++state.active_leases;
    return PlatformSubsystemLease{flags};
}

Result<PlatformSubsystemLease> PlatformSubsystemLease::acquire_headless_video() {
    if (std::getenv("SDL_VIDEO_DRIVER") == nullptr && std::getenv("SDL_VIDEODRIVER") == nullptr) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }

    constexpr std::uint32_t flags = SDL_INIT_VIDEO;
    if (!SDL_InitSubSystem(flags)) {
        return Err{ErrorCode::BackendError, sdl_error_message("SDL_InitSubSystem failed")};
    }

    auto& state = subsystem_state();
    std::lock_guard lock{state.mutex};
    ++state.active_leases;
    return PlatformSubsystemLease{flags};
}

void PlatformSubsystemLease::reset() noexcept {
    if (subsystem_flags_ == 0) {
        return;
    }

    SDL_QuitSubSystem(subsystem_flags_);
    subsystem_flags_ = 0;

    auto& state = subsystem_state();
    std::lock_guard lock{state.mutex};
    assert(state.active_leases > 0);
    --state.active_leases;
    if (state.active_leases == 0 && SDL_WasInit(0) == 0) {
        SDL_Quit();
    }
}

std::shared_ptr<WindowBackend> create_window_backend(const WindowConfig& config) {
    if (should_use_headless_backend()) {
        static_cast<void>(config);
        return std::make_shared<HeadlessWindowBackend>();
    }
    return std::make_shared<SdlWindowBackend>(config);
}

bool should_use_headless_backend() noexcept {
    return env_flag_enabled("ENGINE_HEADLESS");
}

} // namespace engine::detail
