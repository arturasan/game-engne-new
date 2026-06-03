#include "engine/core/schedule.hpp"

#include <algorithm>

namespace engine {

bool SystemAccess::reads(ResourceId id) const {
    return std::ranges::any_of(resources, [id](const ResourceAccess& access) {
        return access.id == id && access.kind == ResourceAccessKind::read;
    });
}

bool SystemAccess::writes(ResourceId id) const {
    return std::ranges::any_of(resources, [id](const ResourceAccess& access) {
        return access.id == id && access.kind == ResourceAccessKind::write;
    });
}

bool SystemAccess::conflicts_with(const SystemAccess& other) const {
    for (const auto& mine : resources) {
        for (const auto& theirs : other.resources) {
            if (mine.id == theirs.id && (mine.kind == ResourceAccessKind::write ||
                                         theirs.kind == ResourceAccessKind::write)) {
                return true;
            }
        }
    }
    return false;
}

void Schedule::add(std::string_view label, SystemFn fn) {
    systems_.push_back({label, std::move(fn), {}});
}

void Schedule::run(App& app) {
    for (auto& s : systems_) {
        s.run(app);
    }
}

} // namespace engine
