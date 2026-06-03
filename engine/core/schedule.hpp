#pragma once

#include <functional>
#include <string_view>
#include <vector>

namespace engine {

class App;

using SystemFn = std::function<void(App&)>;

struct SystemEntry {
    std::string_view label;
    SystemFn run;
};

class Schedule {
public:
    void add(std::string_view label, SystemFn fn);
    void run(App& app);

    [[nodiscard]] std::size_t size() const noexcept {
        return systems_.size();
    }

private:
    std::vector<SystemEntry> systems_;
};

} // namespace engine
