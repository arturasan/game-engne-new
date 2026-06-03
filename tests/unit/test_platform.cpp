#include <doctest/doctest.h>

#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/core/app.hpp"
#include "engine/platform/platform.hpp"

namespace {

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

TEST_CASE("headless key events update pressed and just state lifecycle" *
          doctest::test_suite("fast")) {
    enable_headless();
    engine::App app;
    app.add_plugin(engine::PlatformPlugin{});

    engine::push_headless_event(app, engine::KeyEvent{.key = engine::Key::Escape, .pressed = true});
    run_one_frame(app);
    CHECK(engine::input(app).key_pressed(engine::Key::Escape));
    CHECK(engine::input(app).key_just_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_released(engine::Key::Escape));
    REQUIRE(engine::platform_events(app).key.size() == 1);
    CHECK(engine::platform_events(app).key[0].key == engine::Key::Escape);

    run_one_frame(app);
    CHECK(engine::input(app).key_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_released(engine::Key::Escape));

    engine::push_headless_event(app,
                                engine::KeyEvent{.key = engine::Key::Escape, .pressed = false});
    run_one_frame(app);
    CHECK_FALSE(engine::input(app).key_pressed(engine::Key::Escape));
    CHECK_FALSE(engine::input(app).key_just_pressed(engine::Key::Escape));
    CHECK(engine::input(app).key_just_released(engine::Key::Escape));
}

TEST_CASE("headless mouse button events update pressed and just state lifecycle" *
          doctest::test_suite("fast")) {
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
}

TEST_CASE("headless motion and wheel events update deltas for one frame" *
          doctest::test_suite("fast")) {
    enable_headless();
    engine::App app;
    app.add_plugin(engine::PlatformPlugin{});

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
    REQUIRE(engine::platform_events(app).window_resize.size() == 1);
    CHECK(engine::platform_events(app).window_resize[0].size == engine::Extent2d{800, 600});

    engine::push_headless_event(app, engine::WindowCloseRequested{});
    run_one_frame(app);
    CHECK(engine::window(app).should_close());
    CHECK(engine::platform_events(app).window_close_requested.size() == 1);
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
