// hello_window — smallest runnable demo of the App loop.
// A real engine::Window (SDL3-backed) is added in a later spec; this file
// exists today to prove the App + Schedule + Plugin scaffolding builds and
// runs as an executable on both Linux and Windows.

#include "engine/core/app.hpp"

#include <cstdio>

namespace {

struct LogFramePlugin {
    void build(engine::App& app) const {
        app.add_system("log_frame", [](engine::App& a) {
            std::printf("frame %llu\n", static_cast<unsigned long long>(a.frame()));
        });
    }
};

}  // namespace

int main() {
    engine::App app;
    app.add_plugin(LogFramePlugin{}).set_max_frames(5);
    return app.run();
}
