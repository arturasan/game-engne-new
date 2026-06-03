#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace engine::testing {
namespace detail {

inline std::optional<std::string> json_string_field(std::string_view line, std::string_view key) {
    const std::string marker = std::format(R"("{}":")", key);
    const std::size_t marker_pos = line.find(marker);
    if (marker_pos == std::string_view::npos) {
        return std::nullopt;
    }

    std::string value;
    bool escaped = false;
    for (std::size_t i = marker_pos + marker.size(); i < line.size(); ++i) {
        const char ch = line[i];
        if (escaped) {
            value += ch;
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value += ch;
    }

    return std::nullopt;
}

} // namespace detail

struct LogMatch {
    std::string_view level;
    std::string_view message_contains;
    std::filesystem::path path = "build/logs/last_run.jsonl";
};

inline bool log_contains(const LogMatch& match) {
    std::ifstream input{match.path};
    std::string line;

    while (std::getline(input, line)) {
        const auto level = detail::json_string_field(line, "level");
        const auto message = detail::json_string_field(line, "message");
        if (level == match.level && message.has_value() &&
            message->find(match.message_contains) != std::string::npos) {
            return true;
        }
    }

    return false;
}

} // namespace engine::testing
