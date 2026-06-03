#include "engine/core/app.hpp"

namespace engine {

int App::run() {
    while (!exit_requested_) {
        update_.run(*this);
        ++frame_;
        if (max_frames_ != 0 && frame_ >= max_frames_) {
            break;
        }
    }
    return 0;
}

} // namespace engine
