#include "engine/core/schedule.hpp"

namespace engine {

void Schedule::add(std::string_view label, SystemFn fn) {
    systems_.push_back({label, std::move(fn)});
}

void Schedule::run(App& app) {
    for (auto& s : systems_) {
        s.run(app);
    }
}

}  // namespace engine
