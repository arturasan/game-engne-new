#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "engine/core/app.hpp"
#include "engine/core/log.hpp"

#include "tests/support/log_assert.hpp"

namespace {

std::filesystem::path test_log_path(std::string_view name) {
    return std::filesystem::path{"build/logs"} / std::format("{}.jsonl", name);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::optional<std::string> first_line(const std::filesystem::path& path) {
    std::ifstream input{path};
    std::string line;
    if (std::getline(input, line)) {
        return line;
    }
    return std::nullopt;
}

void emit_source_location_probe() {
    engine::log::info("source location probe");
}

} // namespace

TEST_CASE("log functions format messages and write JSON lines" * doctest::test_suite("fast")) {
    const auto path = test_log_path("format");
    engine::App app;
    app.add_plugin(engine::LogPlugin{engine::LogConfig{
        .level = engine::LogLevel::Trace, .json_path = path.string(), .color = false}});

    engine::log::info("loaded {} entities", 3);
    engine::log::flush();

    const auto line = first_line(path);
    REQUIRE(line.has_value());
    CHECK(line->starts_with('{'));
    CHECK(line->ends_with('}'));
    CHECK(engine::testing::detail::json_string_field(*line, "ts").has_value());
    CHECK(engine::testing::detail::json_string_field(*line, "level") == "info");
    CHECK(engine::testing::detail::json_string_field(*line, "target") == "engine");
    CHECK(engine::testing::detail::json_string_field(*line, "message") == "loaded 3 entities");
    CHECK(engine::testing::detail::json_string_field(*line, "thread").has_value());
    CHECK(line->find(R"("source_location":)") != std::string::npos);
    CHECK(line->find("test_log.cpp") != std::string::npos);
}

TEST_CASE("log JSON records carry call-site source location" * doctest::test_suite("fast")) {
    const auto path = test_log_path("source-location");
    engine::App app;
    app.add_plugin(engine::LogPlugin{engine::LogConfig{
        .level = engine::LogLevel::Info, .json_path = path.string(), .color = false}});

    emit_source_location_probe();
    engine::log::flush();

    const auto line = first_line(path);
    REQUIRE(line.has_value());
    CHECK(line->find(R"("source_location":)") != std::string::npos);
    CHECK(line->find("test_log.cpp") != std::string::npos);
    CHECK(line->find("emit_source_location_probe") != std::string::npos);
}

TEST_CASE("log level filters lower-severity records" * doctest::test_suite("fast")) {
    const auto path = test_log_path("level-filter");
    engine::App app;
    app.add_plugin(engine::LogPlugin{engine::LogConfig{
        .level = engine::LogLevel::Warn, .json_path = path.string(), .color = false}});

    engine::log::info("filtered info");
    engine::log::warn("visible warning");
    engine::log::flush();

    const std::string contents = read_file(path);
    CHECK(contents.find("filtered info") == std::string::npos);
    CHECK(contents.find("visible warning") != std::string::npos);
}

TEST_CASE("log_contains finds JSON log messages" * doctest::test_suite("fast")) {
    const auto path = test_log_path("helper");
    engine::App app;
    app.add_plugin(engine::LogPlugin{engine::LogConfig{
        .level = engine::LogLevel::Debug, .json_path = path.string(), .color = false}});

    engine::log::debug("helper token {}", 17);
    engine::log::flush();

    CHECK(engine::testing::log_contains(engine::testing::LogMatch{
        .level = "debug", .message_contains = "helper token 17", .path = path}));
}

TEST_CASE("set_level changes filtering at runtime" * doctest::test_suite("fast")) {
    const auto path = test_log_path("set-level");
    engine::App app;
    app.add_plugin(engine::LogPlugin{engine::LogConfig{
        .level = engine::LogLevel::Error, .json_path = path.string(), .color = false}});

    engine::log::warn("hidden warning");
    engine::log::set_level(engine::LogLevel::Trace);
    engine::log::trace("visible trace");
    engine::log::flush();

    const std::string contents = read_file(path);
    CHECK(contents.find("hidden warning") == std::string::npos);
    CHECK(contents.find("visible trace") != std::string::npos);
}
