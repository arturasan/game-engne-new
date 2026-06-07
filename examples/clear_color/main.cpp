#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "engine/core/app.hpp"
#include "engine/core/log.hpp"
#include "engine/platform/platform.hpp"
#include "engine/render/renderer.hpp"

namespace {

[[nodiscard]] bool headless_requested() noexcept {
    const char* value = std::getenv("ENGINE_HEADLESS");
    return value != nullptr && value[0] != '\0' && std::string_view(value) != "0";
}

[[nodiscard]] engine::ClearColor cornflower_blue() noexcept {
    return engine::ClearColor{engine::Color{.r = 0.392F, .g = 0.584F, .b = 0.929F, .a = 1.0F}};
}

void write_ppm(const engine::ReadbackImage& image, const std::filesystem::path& path) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << "P6\n" << image.size.width << ' ' << image.size.height << "\n255\n";
    for (std::uint32_t y = 0; y < image.size.height; ++y) {
        const auto* row = image.pixels.data() + static_cast<std::size_t>(y) * image.bytes_per_row;
        for (std::uint32_t x = 0; x < image.size.width; ++x) {
            const auto offset = static_cast<std::size_t>(x) * 4U;
            const char rgb[] = {
                static_cast<char>(row[offset + 0U]),
                static_cast<char>(row[offset + 1U]),
                static_cast<char>(row[offset + 2U]),
            };
            file.write(rgb, sizeof(rgb));
        }
    }
}

} // namespace

int main() {
    engine::App app;
    app.world().insert_resource(cornflower_blue());

    if (headless_requested()) {
        app.add_plugin(engine::RenderPlugin{});
        app.set_max_frames(5);
        const int first_result = app.run();
        if (first_result != 0) {
            return first_result;
        }

        auto image = app.world().resource<engine::Renderer>().read_back();
        if (!image.has_value()) {
            engine::log::error("clear_color readback failed: {}", image.error().message);
            return 1;
        }
        write_ppm(*image, "build/test-output/clear_color_frame_5.ppm");

        app.set_max_frames(60);
        return app.run();
    }

    app.add_plugin(engine::PlatformPlugin{.config =
                                              engine::WindowConfig{
                                                  .title = "clear_color",
                                                  .width = 1280,
                                                  .height = 720,
                                                  .resizable = true,
                                                  .fullscreen = false,
                                                  .vsync = true,
                                              }})
        .add_plugin(engine::RenderPlugin{});
    app.set_max_frames(60);
    return app.run();
}
