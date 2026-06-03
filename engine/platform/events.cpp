#include "engine/platform/events.hpp"

namespace engine {

void PlatformEvents::clear() noexcept {
    key.clear();
    mouse_button.clear();
    mouse_motion.clear();
    mouse_wheel.clear();
    window_resize.clear();
    window_close_requested.clear();
}

} // namespace engine
