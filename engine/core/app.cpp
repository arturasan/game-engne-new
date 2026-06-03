#include "engine/core/app.hpp"

#include "engine/core/log.hpp"

namespace engine {

namespace detail {

World& app_world(App& app) noexcept {
    return app.world();
}

const World& app_world(const App& app) noexcept {
    return app.world();
}

} // namespace detail

int App::run() {
    while (!exit_requested_) {
        first_.run(*this);
        update_.run(*this);
        ++frame_;
        if (max_frames_ != 0 && frame_ >= max_frames_) {
            break;
        }
    }
    log::flush();
    return 0;
}

} // namespace engine
