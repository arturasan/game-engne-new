#include <cstdio>

#include "engine/core/app.hpp"
#include "engine/platform/platform.hpp"

namespace {

struct HelloWindowPlugin {
    void build(engine::App& app) const {
        app.add_system("hello_window", [](engine::App& a) {
            const auto& window = engine::window(a);
            const auto& input = engine::input(a);
            const auto size = window.size();
            std::printf("frame %llu %ux%u\n", static_cast<unsigned long long>(a.frame()),
                        size.width, size.height);

            if (input.key_just_pressed(engine::Key::Escape) || window.should_close()) {
                a.request_exit();
            }
        });
    }
};

} // namespace

int main() {
    engine::App app;
    app.add_plugin(engine::PlatformPlugin{.config = engine::WindowConfig{.title = "hello_window"}})
        .add_plugin(HelloWindowPlugin{});
    return app.run();
}
