#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

#include "engine/core/app.hpp"
#include "engine/platform/events.hpp"
#include "engine/render/renderer.hpp"

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

[[nodiscard]] std::uint8_t byte_at(const engine::ReadbackImage& image, std::uint32_t x,
                                   std::uint32_t y, std::uint32_t channel) {
    const auto offset = static_cast<std::size_t>(y) * image.bytes_per_row +
                        static_cast<std::size_t>(x) * 4U + channel;
    return static_cast<std::uint8_t>(image.pixels[offset]);
}

void check_channel(std::uint8_t actual, std::uint8_t expected) {
    CHECK(std::abs(static_cast<int>(actual) - static_cast<int>(expected)) <= 1);
}

void check_center_pixel(const engine::ReadbackImage& image, std::uint8_t r, std::uint8_t g,
                        std::uint8_t b, std::uint8_t a) {
    const std::uint32_t x = image.size.width / 2U;
    const std::uint32_t y = image.size.height / 2U;
    check_channel(byte_at(image, x, y, 0), r);
    check_channel(byte_at(image, x, y, 1), g);
    check_channel(byte_at(image, x, y, 2), b);
    check_channel(byte_at(image, x, y, 3), a);
}

void check_layout(const engine::ReadbackImage& image, engine::Extent2d size) {
    CHECK(image.size == size);
    CHECK(image.bytes_per_row == size.width * 4U);
    CHECK(image.pixels.size() == static_cast<std::size_t>(image.bytes_per_row) * size.height);
}

} // namespace

static_assert(!std::is_copy_constructible_v<engine::Renderer>);
static_assert(!std::is_copy_assignable_v<engine::Renderer>);
static_assert(std::is_move_constructible_v<engine::Renderer>);
static_assert(std::is_move_assignable_v<engine::Renderer>);

static_assert(std::is_same_v<decltype(engine::Renderer::create_headless()),
                             engine::Result<engine::Renderer>>);

TEST_CASE("renderer public API types are resource-compatible" * doctest::test_suite("fast")) {
    engine::App app;
    app.world().insert_resource(engine::ClearColor{});

    CHECK(app.world().try_resource<engine::ClearColor>() != nullptr);
    CHECK(app.world().resource<engine::ClearColor>().value.r == doctest::Approx(0.1F));

    const engine::ReadbackImage image{};
    CHECK(image.size == engine::Extent2d{});
    CHECK(image.bytes_per_row == 0U);
    CHECK(image.pixels.empty());
}

TEST_CASE("zero-size headless creation is rejected before GPU creation" *
          doctest::test_suite("fast")) {
    auto renderer = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{0, 16}});

    REQUIRE_FALSE(renderer.has_value());
    CHECK(renderer.error().code == engine::ErrorCode::InvalidArgument);
}

TEST_CASE("renderer headers do not expose private dependencies" * doctest::test_suite("fast")) {
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

TEST_CASE("headless renderer clears red and reads back tightly packed RGBA8" *
          doctest::test_suite("slow")) {
    enable_headless();
    auto renderer = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{16, 16}});
    REQUIRE(renderer.has_value());

    auto frame = renderer->render_clear(engine::Color{.r = 1.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F});
    REQUIRE(frame.has_value());
    CHECK(*frame == engine::FrameStatus::Rendered);

    auto image = renderer->read_back();
    REQUIRE(image.has_value());
    check_layout(*image, engine::Extent2d{16, 16});
    check_center_pixel(*image, 255, 0, 0, 255);
}

TEST_CASE("headless renderer clamps clear colors before rendering" * doctest::test_suite("slow")) {
    enable_headless();
    auto renderer = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{8, 8}});
    REQUIRE(renderer.has_value());

    auto frame = renderer->render_clear(engine::Color{.r = 2.0F, .g = -1.0F, .b = 0.5F, .a = 4.0F});
    REQUIRE(frame.has_value());

    auto image = renderer->read_back();
    REQUIRE(image.has_value());
    check_layout(*image, engine::Extent2d{8, 8});
    check_center_pixel(*image, 255, 0, 128, 255);
}

TEST_CASE("headless readback requires a successful clear first" * doctest::test_suite("slow")) {
    enable_headless();
    auto renderer = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{8, 8}});
    REQUIRE(renderer.has_value());

    auto image = renderer->read_back();
    REQUIRE_FALSE(image.has_value());
    CHECK(image.error().code == engine::ErrorCode::InvalidArgument);
}

TEST_CASE("headless renderer resize recreates readback target" * doctest::test_suite("slow")) {
    enable_headless();
    auto renderer = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{16, 16}});
    REQUIRE(renderer.has_value());

    REQUIRE(renderer->render_clear(engine::Color{.r = 1.0F, .a = 1.0F}).has_value());
    auto first = renderer->read_back();
    REQUIRE(first.has_value());
    check_layout(*first, engine::Extent2d{16, 16});
    check_center_pixel(*first, 255, 0, 0, 255);

    auto resize = renderer->resize(engine::Extent2d{32, 8});
    REQUIRE(resize.has_value());
    REQUIRE(renderer->render_clear(engine::Color{.g = 1.0F, .a = 1.0F}).has_value());
    auto second = renderer->read_back();
    REQUIRE(second.has_value());
    check_layout(*second, engine::Extent2d{32, 8});
    check_center_pixel(*second, 0, 255, 0, 255);
}

TEST_CASE("headless resize rejects zero dimensions" * doctest::test_suite("slow")) {
    enable_headless();
    auto renderer = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{12, 10}});
    REQUIRE(renderer.has_value());

    REQUIRE(renderer->render_clear(engine::Color{.r = 0.0F, .g = 1.0F, .b = 0.0F, .a = 1.0F})
                .has_value());
    auto before = renderer->read_back();
    REQUIRE(before.has_value());
    check_layout(*before, engine::Extent2d{12, 10});
    check_center_pixel(*before, 0, 255, 0, 255);

    auto resize = renderer->resize(engine::Extent2d{0, 8});
    REQUIRE_FALSE(resize.has_value());
    CHECK(resize.error().code == engine::ErrorCode::InvalidArgument);
    CHECK(renderer->size() == engine::Extent2d{12, 10});

    auto after = renderer->read_back();
    REQUIRE(after.has_value());
    check_layout(*after, engine::Extent2d{12, 10});
    check_center_pixel(*after, 0, 255, 0, 255);
}

TEST_CASE("destroying one headless renderer does not invalidate another" *
          doctest::test_suite("slow")) {
    enable_headless();
    auto first = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{8, 8}});
    auto second = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{8, 8}});
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    first = std::unexpected(engine::Error{});

    REQUIRE(second->render_clear(engine::Color{.r = 1.0F, .g = 0.0F, .b = 1.0F, .a = 1.0F})
                .has_value());
    auto image = second->read_back();
    REQUIRE(image.has_value());
    check_layout(*image, engine::Extent2d{8, 8});
    check_center_pixel(*image, 255, 0, 255, 255);
}

TEST_CASE("moving a headless renderer preserves ownership" * doctest::test_suite("slow")) {
    enable_headless();
    auto created = engine::Renderer::create_headless(
        engine::HeadlessRenderConfig{.size = engine::Extent2d{8, 8}});
    REQUIRE(created.has_value());
    engine::Renderer moved = std::move(*created);

    REQUIRE(moved.render_clear(engine::Color{.b = 1.0F, .a = 1.0F}).has_value());
    auto image = moved.read_back();
    REQUIRE(image.has_value());
    check_layout(*image, engine::Extent2d{8, 8});
    check_center_pixel(*image, 0, 0, 255, 255);
}

TEST_CASE("RenderPlugin inserts default ClearColor and works without PlatformPlugin headlessly" *
          doctest::test_suite("slow")) {
    enable_headless();
    engine::App app;

    app.add_plugin(engine::RenderPlugin{
        .headless = engine::HeadlessRenderConfig{.size = engine::Extent2d{8, 8}}});

    CHECK(app.world().try_resource<engine::ClearColor>() != nullptr);
    CHECK(app.world().try_resource<engine::Renderer>() != nullptr);
    run_one_frame(app);

    auto image = app.world().resource<engine::Renderer>().read_back();
    REQUIRE(image.has_value());
    check_layout(*image, engine::Extent2d{8, 8});
    check_center_pixel(*image, 26, 26, 26, 255);
}

TEST_CASE("RenderPlugin preserves existing ClearColor and renders after earlier Update systems" *
          doctest::test_suite("slow")) {
    enable_headless();
    engine::App app;
    app.world().insert_resource(engine::ClearColor{engine::Color{.r = 1.0F, .a = 1.0F}});
    app.add_system("change_clear_color", [](engine::ResMut<engine::ClearColor> clear_color) {
        clear_color->value = engine::Color{.r = 0.0F, .g = 0.0F, .b = 1.0F, .a = 1.0F};
    });

    app.add_plugin(engine::RenderPlugin{
        .headless = engine::HeadlessRenderConfig{.size = engine::Extent2d{8, 8}}});

    CHECK(app.world().resource<engine::ClearColor>().value.r == doctest::Approx(1.0F));
    run_one_frame(app);

    auto image = app.world().resource<engine::Renderer>().read_back();
    REQUIRE(image.has_value());
    check_layout(*image, engine::Extent2d{8, 8});
    check_center_pixel(*image, 0, 0, 255, 255);
}

TEST_CASE("RenderPlugin initialization failure registers no renderer or render system" *
          doctest::test_suite("fast")) {
    REQUIRE(::unsetenv("ENGINE_HEADLESS") == 0);
    engine::App app;
    const auto update_systems_before = app.update().size();

    app.add_plugin(engine::RenderPlugin{});

    CHECK(app.world().try_resource<engine::Renderer>() == nullptr);
    CHECK(app.update().size() == update_systems_before);
}
