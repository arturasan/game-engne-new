#pragma once

#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include "engine/core/plugin.hpp"

namespace engine {

class App;

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
};

struct LogConfig {
    LogLevel level = LogLevel::Info;
    std::string json_path = "build/logs/last_run.jsonl";
    bool color = true;
};

struct LogPlugin {
    LogConfig config;

    explicit LogPlugin(LogConfig config_value = {}) : config(std::move(config_value)) {}
    void build(App& app) const;
};

namespace log {

struct MessageFormat {
    std::string_view text;
    std::source_location where;

    consteval MessageFormat(char const* text_value,
                            std::source_location where_value = std::source_location::current())
        : text{text_value}, where{where_value} {}
};

void configure(const LogConfig& config);
void set_level(LogLevel level);
void flush();

void write(LogLevel level, std::string_view target, std::string message,
           std::source_location where);

template <typename... Args> void trace(MessageFormat fmt, Args&&... args) {
    write(LogLevel::Trace, "engine", std::vformat(fmt.text, std::make_format_args(args...)),
          fmt.where);
}

template <typename... Args> void debug(MessageFormat fmt, Args&&... args) {
    write(LogLevel::Debug, "engine", std::vformat(fmt.text, std::make_format_args(args...)),
          fmt.where);
}

template <typename... Args> void info(MessageFormat fmt, Args&&... args) {
    write(LogLevel::Info, "engine", std::vformat(fmt.text, std::make_format_args(args...)),
          fmt.where);
}

template <typename... Args> void warn(MessageFormat fmt, Args&&... args) {
    write(LogLevel::Warn, "engine", std::vformat(fmt.text, std::make_format_args(args...)),
          fmt.where);
}

template <typename... Args> void error(MessageFormat fmt, Args&&... args) {
    write(LogLevel::Error, "engine", std::vformat(fmt.text, std::make_format_args(args...)),
          fmt.where);
}

template <typename... Args> void critical(MessageFormat fmt, Args&&... args) {
    write(LogLevel::Critical, "engine", std::vformat(fmt.text, std::make_format_args(args...)),
          fmt.where);
}

} // namespace log
} // namespace engine
