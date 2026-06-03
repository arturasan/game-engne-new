#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "engine/core/log.hpp"

namespace engine {
namespace {

class JsonSink final : public spdlog::sinks::sink {
public:
    explicit JsonSink(std::filesystem::path path) : path_(std::move(path)) {
        if (path_.has_parent_path()) {
            std::filesystem::create_directories(path_.parent_path());
        }
        stream_.open(path_, std::ios::out | std::ios::trunc);
    }

    void log(const spdlog::details::log_msg& msg) override {
        std::lock_guard lock{mutex_};
        stream_
            << std::format(
                   R"({{"ts":"{}","level":"{}","target":"{}","message":"{}","thread":"{}","source_location":{{"file":"{}","line":{},"function":"{}"}}}})",
                   timestamp(), level_name(msg.level),
                   escape(std::string_view{msg.logger_name.data(), msg.logger_name.size()}),
                   escape(std::string_view{msg.payload.data(), msg.payload.size()}), thread_id(),
                   escape(msg.source.filename), msg.source.line, escape(msg.source.funcname))
            << '\n';
    }

    void flush() override {
        std::lock_guard lock{mutex_};
        stream_.flush();
    }

    void set_pattern(const std::string&) override {}
    void set_formatter(std::unique_ptr<spdlog::formatter>) override {}

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    static std::string escape(std::string_view text) {
        std::string out;
        out.reserve(text.size());
        for (const char ch : text) {
            switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20U) {
                    out += std::format("\\u{:04x}", static_cast<unsigned int>(ch));
                } else {
                    out += ch;
                }
                break;
            }
        }
        return out;
    }

    static std::string timestamp() {
        const auto now = std::chrono::system_clock::now();
        return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(now));
    }

    static std::string thread_id() {
        return std::format("{}", std::this_thread::get_id());
    }

    static std::string_view level_name(spdlog::level::level_enum level) {
        switch (level) {
        case spdlog::level::trace:
            return "trace";
        case spdlog::level::debug:
            return "debug";
        case spdlog::level::info:
            return "info";
        case spdlog::level::warn:
            return "warn";
        case spdlog::level::err:
            return "error";
        case spdlog::level::critical:
            return "critical";
        case spdlog::level::off:
        case spdlog::level::n_levels:
            return "off";
        }
        return "off";
    }

    std::filesystem::path path_;
    std::ofstream stream_;
    std::mutex mutex_;
};

struct LogState {
    std::mutex mutex;
    std::shared_ptr<spdlog::logger> logger;
    std::shared_ptr<JsonSink> json_sink;
};

LogState& state() {
    static LogState value;
    return value;
}

spdlog::level::level_enum to_backend_level(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warn:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Critical:
        return spdlog::level::critical;
    case LogLevel::Off:
        return spdlog::level::off;
    }
    return spdlog::level::info;
}

void ensure_configured() {
    auto& log_state = state();
    std::lock_guard lock{log_state.mutex};
    if (log_state.logger != nullptr) {
        return;
    }
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderr_sink->set_pattern("[%T] [%^%l%$] %v");
    log_state.logger = std::make_shared<spdlog::logger>("engine", stderr_sink);
    log_state.logger->set_level(spdlog::level::info);
    log_state.logger->flush_on(spdlog::level::err);
}

std::shared_ptr<spdlog::logger> logger() {
    ensure_configured();
    auto& log_state = state();
    std::lock_guard lock{log_state.mutex};
    return log_state.logger;
}

} // namespace

void LogPlugin::build(App&) const {
    log::configure(config);
}

namespace log {

void configure(const LogConfig& config) {
    auto json_sink = std::make_shared<JsonSink>(config.json_path);
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    if (!config.color) {
        stderr_sink->set_color_mode(spdlog::color_mode::never);
    }
    stderr_sink->set_pattern("[%T] [%^%l%$] %v");

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::move(stderr_sink));
    sinks.push_back(json_sink);

    auto configured_logger = std::make_shared<spdlog::logger>("engine", sinks.begin(), sinks.end());
    configured_logger->set_level(to_backend_level(config.level));
    configured_logger->flush_on(spdlog::level::err);

    auto& log_state = state();
    std::lock_guard lock{log_state.mutex};
    log_state.json_sink = std::move(json_sink);
    log_state.logger = std::move(configured_logger);
}

void set_level(LogLevel level) {
    logger()->set_level(to_backend_level(level));
}

void flush() {
    logger()->flush();
}

void write(LogLevel level, std::string_view target, std::string message,
           std::source_location where) {
    static_cast<void>(target);
    auto active_logger = logger();
    const spdlog::source_loc source{where.file_name(), static_cast<int>(where.line()),
                                    where.function_name()};
    active_logger->log(source, to_backend_level(level), "{}", message);
}

} // namespace log
} // namespace engine
