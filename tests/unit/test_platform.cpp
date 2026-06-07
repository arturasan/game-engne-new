#include <doctest/doctest.h>

#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "engine/core/app.hpp"
#include "engine/platform/platform.hpp"

namespace {

class EnvVarGuard {
public:
    explicit EnvVarGuard(const char* name) : name_(name) {
        if (const char* value = std::getenv(name_); value != nullptr) {
            original_ = value;
        }
    }

    ~EnvVarGuard() {
        if (original_.has_value()) {
            static_cast<void>(::setenv(name_, original_->c_str(), 1));
        } else {
            static_cast<void>(::unsetenv(name_));
        }
    }

    EnvVarGuard(const EnvVarGuard&) = delete;
    EnvVarGuard& operator=(const EnvVarGuard&) = delete;

private:
    const char* name_;
    std::optional<std::string> original_;
};

void enable_headless() {
    REQUIRE(::setenv("ENGINE_HEADLESS", "1", 1) == 0);
}

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    REQUIRE(file.good());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void run_one_frame(engine::App& app) {
    app.set_max_frames(app.frame() + 1);
    CHECK(app.run() == 0);
}

} // namespace

static_assert(requires(const engine::Input& input) {
    { input.key_pressed(engine::Key::A) } -> std::same_as<bool>;
    { input.key_just_pressed(engine::Key::A) } -> std::same_as<bool>;
    { input.key_just_released(engine::Key::A) } -> std::same_as<bool>;
    { input.mouse_pressed(engine::MouseButton::Left) } -> std::same_as<bool>;
    { input.mouse_just_pressed(engine::MouseButton::Left) } -> std::same_as<bool>;
    { input.mouse_just_released(engine::MouseButton::Left) } -> std::same_as<bool>;
    { input.mouse_position() } -> std::same_as<engine::vec2>;
    { input.mouse_delta() } -> std::same_as<engine::vec2>;
    { input.wheel_delta() } -> std::same_as<engine::vec2>;
});

TEST_CASE("platform public enums keep stable integer mappings" * doctest::test_suite("fast")) {
    CHECK(static_cast<std::uint16_t>(engine::Key::Unknown) == 0);
    CHECK(static_cast<std::uint16_t>(engine::Key::A) == 4);
    CHECK(static_cast<std::uint16_t>(engine::Key::Escape) == 41);
    CHECK(static_cast<std::uint16_t>(engine::Key::EndCall) == 290);
    CHECK(static_cast<std::uint16_t>(engine::Key::Count) == 512);

    CHECK(static_cast<std::uint8_t>(engine::MouseButton::Left) == 0);
    CHECK(static_cast<std::uint8_t>(engine::MouseButton::Right) == 1);
    CHECK(static_cast<std::uint8_t>(engine::MouseButton::Middle) == 2);
    CHECK(static_cast<std::uint8_t>(engine::MouseButton::X1) == 3);
    CHECK(static_cast<std::uint8_t>(engine::MouseButton::X2) == 4);
}

TEST_CASE("PlatformPlugin registers resources and typed event channels" *
          doctest::test_suite("fast")) {
    enable_headless();
    engine::App app;

    CHECK_FALSE(engine::platform_attached(app));

    app.add_plugin(engine::PlatformPlugin{});

    CHECK(engine::platform_attached(app));
    CHECK(app.world().try_resource<engine::Window>() != nullptr);
    CHECK(app.world().try_resource<engine::Input>() != nullptr);
    CHECK(app.world().try_resource<engine::Events<engine::KeyEvent>>() != nullptr);
    CHECK(app.world().try_resource<engine::Events<engine::MouseButtonEvent>>() != nullptr);
    CHECK(app.world().try_resource<engine::Events<engine::MouseMotionEvent>>() != nullptr);
    CHECK(app.world().try_resource<engine::Events<engine::MouseWheelEvent>>() != nullptr);
    CHECK(app.world().try_resource<engine::Events<engine::WindowResizeEvent>>() != nullptr);
    CHECK(app.world().try_resource<engine::Events<engine::WindowCloseRequested>>() != nullptr);
}

TEST_CASE("PlatformPlugin handles window initialization failure without partial attach" *
          doctest::test_suite("fast")) {
    EnvVarGuard headless{"ENGINE_HEADLESS"};
    EnvVarGuard video_driver{"SDL_VIDEO_DRIVER"};
    EnvVarGuard legacy_video_driver{"SDL_VIDEODRIVER"};
    REQUIRE(::unsetenv("ENGINE_HEADLESS") == 0);
    REQUIRE(::setenv("SDL_VIDEO_DRIVER", "engine_invalid_video_driver", 1) == 0);
    REQUIRE(::unsetenv("SDL_VIDEODRIVER") == 0);

    engine::App app;

    CHECK_NOTHROW(app.add_plugin(engine::PlatformPlugin{}));
    CHECK_FALSE(engine::platform_attached(app));
    CHECK(app.world().try_resource<engine::Window>() == nullptr);
    CHECK(app.world().try_resource<engine::Input>() == nullptr);
    CHECK(app.world().try_resource<engine::Events<engine::KeyEvent>>() == nullptr);
    CHECK(app.first().size() == 0U);
    CHECK(app.run() == 0);
    CHECK(app.frame() == 0U);
}

TEST_CASE("headless key events update input and typed events in update systems" *
          doctest::test_suite("fast")) {
    enable_headless();
    engine::App app;
    app.add_plugin(engine::PlatformPlugin{});
    std::size_t key_events_seen = 0;
    bool update_saw_pressed = false;
    bool update_saw_just_pressed = false;
    bool update_saw_release = false;
    app.add_system("observe_key", [&](engine::Res<engine::Input> observed_input,
                                      engine::EventReader<engine::KeyEvent> reader) {
        key_events_seen += reader.read().size();
        update_saw_pressed = update_saw_pressed || observed_input->key_pressed(engine::Key::Escape);
        update_saw_just_pressed =
            update_saw_just_pressed || observed_input->key_just_pressed(engine::Key::Escape);
        update_saw_release =
            update_saw_release || observed_input->key_just_released(engine::Key::Escape);
    });

    engine::push_headless_event(app, engine::KeyEvent{.key = engine::Key::Escape, .pressed = true});
    run_one_frame(app);
    CHECK(engine::input(app).key_pressed(engine::Key::Escape));
    CHECK(engine::input(app).key_just_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_released(engine::Key::Escape));
    CHECK(update_saw_pressed);
    CHECK(update_saw_just_pressed);
    REQUIRE(app.world().resource<engine::Events<engine::KeyEvent>>().read().size() == 1U);
    CHECK(app.world().resource<engine::Events<engine::KeyEvent>>().read()[0].key ==
          engine::Key::Escape);
    CHECK(key_events_seen == 1U);

    run_one_frame(app);
    CHECK(engine::input(app).key_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_released(engine::Key::Escape));
    CHECK(key_events_seen == 1U);

    engine::push_headless_event(app,
                                engine::KeyEvent{.key = engine::Key::Escape, .pressed = false});
    run_one_frame(app);
    CHECK_FALSE(engine::input(app).key_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_pressed(engine::Key::Escape));
    CHECK(engine::input(app).key_just_released(engine::Key::Escape));
    CHECK(update_saw_release);
    CHECK(key_events_seen == 2U);
}

TEST_CASE("headless mouse events update input and typed channels" * doctest::test_suite("fast")) {
    enable_headless();
    engine::App app;
    app.add_plugin(engine::PlatformPlugin{});

    engine::push_headless_event(app, engine::MouseButtonEvent{
                                         .button = engine::MouseButton::Left,
                                         .pressed = true,
                                         .position = engine::vec2{10.0F, 20.0F},
                                     });
    run_one_frame(app);
    CHECK(engine::input(app).mouse_pressed(engine::MouseButton::Left));
    CHECK(engine::input(app).mouse_just_pressed(engine::MouseButton::Left));
    CHECK_FALSE(engine::input(app).mouse_just_released(engine::MouseButton::Left));
    CHECK(engine::input(app).mouse_position() == engine::vec2{10.0F, 20.0F});
    REQUIRE(app.world().resource<engine::Events<engine::MouseButtonEvent>>().read().size() == 1U);
    CHECK(app.world().resource<engine::Events<engine::MouseButtonEvent>>().read()[0].button ==
          engine::MouseButton::Left);

    run_one_frame(app);
    CHECK(engine::input(app).mouse_pressed(engine::MouseButton::Left));
    CHECK_FALSE(engine::input(app).mouse_just_pressed(engine::MouseButton::Left));

    engine::push_headless_event(app, engine::MouseButtonEvent{
                                         .button = engine::MouseButton::Left,
                                         .pressed = false,
                                         .position = engine::vec2{12.0F, 24.0F},
                                     });
    run_one_frame(app);
    CHECK_FALSE(engine::input(app).mouse_pressed(engine::MouseButton::Left));
    CHECK_FALSE(engine::input(app).mouse_just_pressed(engine::MouseButton::Left));
    CHECK(engine::input(app).mouse_just_released(engine::MouseButton::Left));
    CHECK(engine::input(app).mouse_position() == engine::vec2{12.0F, 24.0F});
    REQUIRE(app.world().resource<engine::Events<engine::MouseButtonEvent>>().read().size() == 1U);
    CHECK_FALSE(app.world().resource<engine::Events<engine::MouseButtonEvent>>().read()[0].pressed);

    engine::push_headless_event(app, engine::MouseMotionEvent{
                                         .position = engine::vec2{30.0F, 40.0F},
                                         .delta = engine::vec2{3.0F, 4.0F},
                                     });
    engine::push_headless_event(app, engine::MouseWheelEvent{
                                         .delta = engine::vec2{1.0F, -2.0F},
                                     });
    run_one_frame(app);
    CHECK(engine::input(app).mouse_position() == engine::vec2{30.0F, 40.0F});
    CHECK(engine::input(app).mouse_delta() == engine::vec2{3.0F, 4.0F});
    CHECK(engine::input(app).wheel_delta() == engine::vec2{1.0F, -2.0F});
    REQUIRE(app.world().resource<engine::Events<engine::MouseMotionEvent>>().read().size() == 1U);
    CHECK(app.world().resource<engine::Events<engine::MouseMotionEvent>>().read()[0].delta ==
          engine::vec2{3.0F, 4.0F});
    REQUIRE(app.world().resource<engine::Events<engine::MouseWheelEvent>>().read().size() == 1U);
    CHECK(app.world().resource<engine::Events<engine::MouseWheelEvent>>().read()[0].delta ==
          engine::vec2{1.0F, -2.0F});

    run_one_frame(app);
    CHECK(engine::input(app).mouse_position() == engine::vec2{30.0F, 40.0F});
    CHECK(engine::input(app).mouse_delta() == engine::vec2{});
    CHECK(engine::input(app).wheel_delta() == engine::vec2{});
}

TEST_CASE("headless window resize and close requested update window state" *
          doctest::test_suite("fast")) {
    enable_headless();
    engine::App app;
    app.add_plugin(engine::PlatformPlugin{.config = engine::WindowConfig{
                                              .width = 640,
                                              .height = 480,
                                          }});

    CHECK(engine::window(app).size() == engine::Extent2d{640, 480});
    CHECK_FALSE(engine::window(app).should_close());

    engine::push_headless_event(app, engine::WindowResizeEvent{
                                         .size = engine::Extent2d{800, 600},
                                     });
    run_one_frame(app);
    CHECK(engine::window(app).size() == engine::Extent2d{800, 600});
    REQUIRE(app.world().resource<engine::Events<engine::WindowResizeEvent>>().read().size() == 1U);
    CHECK(app.world().resource<engine::Events<engine::WindowResizeEvent>>().read()[0].size ==
          engine::Extent2d{800, 600});

    engine::push_headless_event(app, engine::WindowCloseRequested{});
    run_one_frame(app);
    CHECK(engine::window(app).should_close());
    CHECK(app.world().resource<engine::Events<engine::WindowCloseRequested>>().read().size() == 1U);
}

TEST_CASE("platform pump runs in First before normal Update systems" *
          doctest::test_suite("fast")) {
    enable_headless();
    engine::App app;
    app.add_plugin(engine::PlatformPlugin{});
    bool update_saw_event = false;
    bool update_saw_input = false;
    app.add_system("observe_first_pump", [&](engine::Res<engine::Input> observed_input,
                                             engine::EventReader<engine::KeyEvent> reader) {
        update_saw_event = update_saw_event || !reader.read().empty();
        update_saw_input = update_saw_input || observed_input->key_just_pressed(engine::Key::A);
    });

    engine::push_headless_event(app, engine::KeyEvent{.key = engine::Key::A, .pressed = true});
    run_one_frame(app);

    CHECK(update_saw_event);
    CHECK(update_saw_input);
}

TEST_CASE("platform resources and events are isolated between App instances" *
          doctest::test_suite("fast")) {
    enable_headless();
    engine::App first;
    engine::App second;
    first.add_plugin(engine::PlatformPlugin{});
    second.add_plugin(engine::PlatformPlugin{});

    engine::push_headless_event(first, engine::KeyEvent{.key = engine::Key::B, .pressed = true});
    run_one_frame(first);
    run_one_frame(second);

    CHECK(engine::input(first).key_pressed(engine::Key::B));
    CHECK_FALSE(engine::input(second).key_pressed(engine::Key::B));
    CHECK(first.world().resource<engine::Events<engine::KeyEvent>>().read().size() == 1U);
    CHECK(second.world().resource<engine::Events<engine::KeyEvent>>().read().empty());
}

TEST_CASE("public engine headers do not leak private backend dependencies" *
          doctest::test_suite("fast")) {
    const std::filesystem::path root = ENGINE_SOURCE_DIR;
    const std::filesystem::path engine_dir = root / "engine";
    const std::string forbidden[] = {"SDL_",  "SDL3",   "SDL_Window", "SDL_GPU",
                                     "glm::", "spdlog", "ImGui"};

    for (const auto& entry : std::filesystem::recursive_directory_iterator(engine_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".hpp") {
            continue;
        }

        const std::string contents = read_file(entry.path());
        for (const std::string& token : forbidden) {
            CAPTURE(entry.path().string());
            CAPTURE(token);
            CHECK(contents.find(token) == std::string::npos);
        }
    }
}
